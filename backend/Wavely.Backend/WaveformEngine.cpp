#include "pch.h"
#include "WaveformEngine.h"
#include "WaveformEngine.g.cpp"

#include "Core/ComPtr.h"
#include "Core/Handle.h"
#include "Core/MusicAppAllowlist.h"
#include "Core/WinrtGuard.h"

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <propidl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace winrt::Wavely::Backend::implementation
{
    namespace
    {
        using winrt::Windows::Storage::Streams::Buffer;
        using winrt::Windows::Storage::Streams::IBuffer;

        constexpr std::chrono::milliseconds kEmitInterval{ 16 };
        constexpr std::chrono::milliseconds kPollInterval{ 5 };
        constexpr std::chrono::seconds kTargetReevaluationInterval{ 2 };
        constexpr REFERENCE_TIME kBufferDuration = 200 * 10'000; // 200ms, expressed in 100ns units.
        // Process-loopback-activated IAudioClient instances don't support GetMixFormat (there is
        // no real device to query a mix format from), so the capture format has to be supplied
        // up front instead of negotiated. Float32/48kHz/stereo matches the shared-mode engine
        // format WASAPI uses on essentially all modern Windows installs.
        constexpr std::uint32_t kCaptureSampleRate = 48000;
        constexpr std::uint16_t kCaptureChannelCount = 2;
        // Typical program material (streamed video commentary, loudness-normalized music) sits
        // well below full scale - a raw magnitude spectrum alone renders as visually flat for
        // anything but very loud test tones. sqrt() is the standard perceptual curve real VU
        // meters/spectrum analyzers use to make quiet passages still visibly move without letting
        // loud ones clip out immediately; kVisualGain then lifts the whole curve so mid-volume
        // content reads clearly.
        constexpr float kVisualGain = 3.0f;

        void logFailure(const wchar_t* context, HRESULT hr)
        {
            wchar_t message[256];
            swprintf_s(message, L"WaveformEngine: %s (0x%08X)\n", context, static_cast<unsigned>(hr));
            OutputDebugStringW(message);
        }

        /// Averages an interleaved multi-channel buffer down to mono, appending each frame's
        /// average to `out`. Frequency content (what an EQ display shows) doesn't depend on
        /// which channel it came from, so there is no reason to carry stereo through the FFT.
        void monoMixInto(const float* interleaved, std::uint32_t frameCount, std::uint32_t channelCount, std::vector<float>& out)
        {
            for (std::uint32_t frame = 0; frame < frameCount; ++frame)
            {
                float sum = 0.0f;
                for (std::uint32_t channel = 0; channel < channelCount; ++channel)
                {
                    sum += interleaved[frame * channelCount + channel];
                }
                out.push_back(sum / static_cast<float>(channelCount));
            }
        }

        /// Standard iterative radix-2 Cooley-Tukey FFT, in place. `data.size()` must be a power
        /// of two (enforced by kFftSize being a compile-time power of two).
        void fft(std::vector<std::complex<float>>& data)
        {
            const std::size_t n = data.size();
            for (std::size_t i = 1, j = 0; i < n; ++i)
            {
                std::size_t bit = n >> 1;
                for (; (j & bit) != 0; bit >>= 1)
                {
                    j ^= bit;
                }
                j ^= bit;
                if (i < j)
                {
                    std::swap(data[i], data[j]);
                }
            }

            for (std::size_t len = 2; len <= n; len <<= 1)
            {
                const float angle = -2.0f * std::numbers::pi_v<float> / static_cast<float>(len);
                const std::complex<float> wlen(std::cos(angle), std::sin(angle));
                for (std::size_t i = 0; i < n; i += len)
                {
                    std::complex<float> w(1.0f, 0.0f);
                    for (std::size_t j = 0; j < len / 2; ++j)
                    {
                        const auto u = data[i + j];
                        const auto v = data[i + j + len / 2] * w;
                        data[i + j] = u + v;
                        data[i + j + len / 2] = u - v;
                        w *= wlen;
                    }
                }
            }
        }

        // Concentrating the log-spaced bands into the perceptually useful ~40Hz-16kHz range
        // (instead of the full 0-Nyquist span, most of which is either sub-bass rumble or
        // near-silent air above most recorded music's rolloff) gives every band, especially the
        // treble ones, meaningfully more resolution and visible movement.
        constexpr double kMinFrequencyHz = 40.0;
        constexpr double kMaxFrequencyHz = 16000.0;

        std::size_t binForFrequency(double frequencyHz, std::size_t fftSize, std::uint32_t sampleRate)
        {
            return static_cast<std::size_t>(std::lround(frequencyHz * static_cast<double>(fftSize) / static_cast<double>(sampleRate)));
        }

        /// Log-spaced bin boundary for `band` of `bandCount`, over usable bins [minBin, maxBin].
        /// Low bands cover a couple of bins each (bass carries the most energy and needs the
        /// least averaging), high bands cover progressively wider bin ranges - the same reason a
        /// real EQ's bands aren't linearly spaced.
        std::size_t logBinBoundary(std::size_t band, std::size_t bandCount, std::size_t minBin, std::size_t maxBin)
        {
            const double t = static_cast<double>(band) / static_cast<double>(bandCount);
            const double bin = static_cast<double>(minBin) * std::pow(static_cast<double>(maxBin) / static_cast<double>(minBin), t);
            return std::clamp<std::size_t>(static_cast<std::size_t>(std::lround(bin)), minBin, maxBin);
        }

        /// Resolves a PID to its executable file name (e.g. L"Spotify.exe", no path). Empty on
        /// failure (process exited, insufficient access, ...).
        std::wstring getProcessImageName(DWORD processId)
        {
            ::Wavely::Backend::Core::UniqueHandle process(
                OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
            if (!process)
            {
                return {};
            }
            wchar_t buffer[MAX_PATH];
            DWORD size = static_cast<DWORD>(std::size(buffer));
            if (!QueryFullProcessImageNameW(process.get(), 0, buffer, &size))
            {
                return {};
            }
            const std::wstring_view path(buffer, size);
            const auto lastSlash = path.find_last_of(L"\\/");
            return std::wstring(lastSlash == std::wstring_view::npos ? path : path.substr(lastSlash + 1));
        }

        /// Finds the process ID of a whitelisted music app (Core::IsWhitelistedProcessName) that
        /// currently has an *actively rendering* audio session, scanning every render device's
        /// session list rather than just the default device: the session-active signal stays
        /// reliable even for apps individually routed through Elgato Wave Link or similar
        /// per-app routing software (ADR-003's finding was that only the *loopback sample data*
        /// on those virtual devices is broken, not the session state itself).
        ///
        /// Apps like Spotify run as several same-named processes (one per Chromium
        /// renderer/GPU/utility role, confirmed directly on this project - a running Spotify
        /// client showed 7 distinct "Spotify.exe" PIDs); this finds the *specific* PID that is
        /// actually rendering audio right now rather than an arbitrary one, since process-
        /// loopback capture is scoped to a target PID (plus its process tree).
        DWORD findWhitelistedActiveProcessId(IMMDeviceEnumerator* enumerator)
        {
            ::Wavely::Backend::Core::ComPtr<IMMDeviceCollection> collection;
            if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put())))
            {
                return 0;
            }

            UINT deviceCount = 0;
            collection->GetCount(&deviceCount);
            for (UINT d = 0; d < deviceCount; ++d)
            {
                ::Wavely::Backend::Core::ComPtr<IMMDevice> device;
                if (FAILED(collection->Item(d, device.put())))
                {
                    continue;
                }

                ::Wavely::Backend::Core::ComPtr<IAudioSessionManager2> sessionManager;
                if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, sessionManager.put_void())))
                {
                    continue;
                }
                ::Wavely::Backend::Core::ComPtr<IAudioSessionEnumerator> sessionEnumerator;
                if (FAILED(sessionManager->GetSessionEnumerator(sessionEnumerator.put())))
                {
                    continue;
                }
                int sessionCount = 0;
                if (FAILED(sessionEnumerator->GetCount(&sessionCount)))
                {
                    continue;
                }

                for (int i = 0; i < sessionCount; ++i)
                {
                    ::Wavely::Backend::Core::ComPtr<IAudioSessionControl> session;
                    if (FAILED(sessionEnumerator->GetSession(i, session.put())))
                    {
                        continue;
                    }
                    AudioSessionState state{};
                    if (FAILED(session->GetState(&state)) || state != AudioSessionStateActive)
                    {
                        continue;
                    }
                    const auto session2 = session.try_as<IAudioSessionControl2>();
                    if (!session2)
                    {
                        continue;
                    }
                    DWORD processId = 0;
                    if (FAILED(session2->GetProcessId(&processId)) || processId == 0)
                    {
                        continue;
                    }
                    if (::Wavely::Backend::Core::IsWhitelistedProcessName(getProcessImageName(processId)))
                    {
                        return processId;
                    }
                }
            }
            return 0;
        }

        /// Bridges the callback-based ActivateAudioInterfaceAsync into a synchronous call: the
        /// capture thread already blocks on WASAPI calls throughout this file, so there is no
        /// reason to keep this one asynchronous too. winrt::implements provides the classic COM
        /// IUnknown plumbing for IActivateAudioInterfaceCompletionHandler even though it isn't an
        /// IInspectable-based WinRT interface - it only requires deriving from IUnknown, which is
        /// true here.
        struct ActivationCompletionHandler : winrt::implements<ActivationCompletionHandler, IActivateAudioInterfaceCompletionHandler>
        {
            ::Wavely::Backend::Core::UniqueHandle completedEvent{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
            HRESULT activateResult = E_FAIL;
            winrt::com_ptr<IUnknown> activatedInterface;

            HRESULT __stdcall ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) noexcept override
            {
                IUnknown* rawInterface = nullptr;
                operation->GetActivateResult(&activateResult, &rawInterface);
                activatedInterface.attach(rawInterface);
                SetEvent(completedEvent.get());
                return S_OK;
            }
        };

        /// Activates process-scoped WASAPI loopback capture for `processId` - captures that
        /// process's (and its child processes') audio output directly, before it reaches any
        /// render device or app-routing software (Elgato Wave Link, VoiceMeeter, ...). This is
        /// what makes capture immune to ADR-003's "virtual device hands back all-zero samples"
        /// problem: there is no device involved in the capture path at all. See
        /// docs/ADR-004-per-process-loopback-capture.md.
        ::Wavely::Backend::Core::ComPtr<IAudioClient> activateProcessLoopbackClient(DWORD processId)
        {
            AUDIOCLIENT_ACTIVATION_PARAMS params{};
            params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
            params.ProcessLoopbackParams.TargetProcessId = processId;
            params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

            PROPVARIANT activationParams{};
            activationParams.vt = VT_BLOB;
            activationParams.blob.cbSize = sizeof(params);
            activationParams.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

            const auto handler = winrt::make_self<ActivationCompletionHandler>();
            if (!handler->completedEvent)
            {
                logFailure(L"failed to create the activation-completed event", HRESULT_FROM_WIN32(GetLastError()));
                return nullptr;
            }

            ::Wavely::Backend::Core::ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
            const HRESULT hr = ActivateAudioInterfaceAsync(
                VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &activationParams, handler.get(), asyncOp.put());
            if (FAILED(hr))
            {
                logFailure(L"ActivateAudioInterfaceAsync failed", hr);
                return nullptr;
            }

            // Activation completes essentially immediately in practice (no device I/O involved);
            // the timeout is only a safety net against hanging the capture thread forever.
            if (WaitForSingleObject(handler->completedEvent.get(), 2000) != WAIT_OBJECT_0)
            {
                logFailure(L"process-loopback activation timed out", E_FAIL);
                return nullptr;
            }
            if (FAILED(handler->activateResult) || !handler->activatedInterface)
            {
                logFailure(L"process-loopback activation failed", handler->activateResult);
                return nullptr;
            }

            return handler->activatedInterface.try_as<IAudioClient>();
        }
    }

    WaveformEngine::~WaveformEngine()
    {
        Stop();
    }

    void WaveformEngine::Start()
    {
        if (m_running.exchange(true))
        {
            return;
        }
        m_captureThread = std::thread([this] { captureThreadProc(); });
    }

    void WaveformEngine::Stop()
    {
        if (!m_running.exchange(false))
        {
            return;
        }
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
    }

    void WaveformEngine::captureThreadProc()
    {
        const ::Wavely::Backend::Core::WinrtApartmentGuard apartmentGuard;

        ::Wavely::Backend::Core::ComPtr<IMMDeviceEnumerator> enumerator;
        const HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), enumerator.put_void());
        if (FAILED(hr))
        {
            logFailure(L"failed to create the device enumerator", hr);
            return;
        }

        while (m_running.load(std::memory_order_relaxed))
        {
            const DWORD processId = findWhitelistedActiveProcessId(enumerator.get());
            if (processId == 0)
            {
                std::this_thread::sleep_for(kTargetReevaluationInterval);
                continue;
            }
            runProcessCaptureSession(enumerator.get(), processId);
        }
    }

    void WaveformEngine::runProcessCaptureSession(IMMDeviceEnumerator* enumerator, DWORD processId)
    {
        const auto audioClient = activateProcessLoopbackClient(processId);
        if (!audioClient)
        {
            std::this_thread::sleep_for(kTargetReevaluationInterval);
            return;
        }

        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = kCaptureChannelCount;
        format.nSamplesPerSec = kCaptureSampleRate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        HRESULT hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, kBufferDuration, 0, &format, nullptr);
        if (FAILED(hr))
        {
            logFailure(L"failed to initialize the process-loopback audio client", hr);
            std::this_thread::sleep_for(kTargetReevaluationInterval);
            return;
        }
        m_sampleRate = format.nSamplesPerSec;

        ::Wavely::Backend::Core::ComPtr<IAudioCaptureClient> captureClient;
        hr = audioClient->GetService(__uuidof(IAudioCaptureClient), captureClient.put_void());
        if (FAILED(hr))
        {
            logFailure(L"failed to get the capture client", hr);
            std::this_thread::sleep_for(kTargetReevaluationInterval);
            return;
        }

        hr = audioClient->Start();
        if (FAILED(hr))
        {
            logFailure(L"failed to start the audio client", hr);
            std::this_thread::sleep_for(kTargetReevaluationInterval);
            return;
        }

        auto lastEmit = std::chrono::steady_clock::now();
        auto lastTargetCheck = lastEmit;
        std::vector<float> monoScratch;
        monoScratch.reserve(kFftSize);

        while (m_running.load(std::memory_order_relaxed))
        {
            UINT32 packetLength = 0;
            hr = captureClient->GetNextPacketSize(&packetLength);
            while (SUCCEEDED(hr) && packetLength != 0)
            {
                BYTE* data = nullptr;
                UINT32 frameCount = 0;
                DWORD flags = 0;
                hr = captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr);
                if (FAILED(hr))
                {
                    logFailure(L"GetBuffer failed", hr);
                    break;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    monoScratch.assign(frameCount, 0.0f);
                }
                else
                {
                    monoScratch.clear();
                    monoMixInto(reinterpret_cast<const float*>(data), frameCount, kCaptureChannelCount, monoScratch);
                }
                m_ringBuffer.Write(monoScratch.data(), monoScratch.size());

                hr = captureClient->ReleaseBuffer(frameCount);
                if (FAILED(hr))
                {
                    logFailure(L"ReleaseBuffer failed", hr);
                    break;
                }

                hr = captureClient->GetNextPacketSize(&packetLength);
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - lastEmit >= kEmitInterval)
            {
                emitBands();
                lastEmit = now;
            }

            if (now - lastTargetCheck >= kTargetReevaluationInterval)
            {
                lastTargetCheck = now;
                if (findWhitelistedActiveProcessId(enumerator) != processId)
                {
                    audioClient->Stop();
                    return;
                }
            }

            std::this_thread::sleep_for(kPollInterval);
        }

        audioClient->Stop();
    }

    void WaveformEngine::emitBands()
    {
        // thread_local, not local: reused across calls without reallocating (this runs every
        // ~16ms on the capture thread, and only the capture thread ever calls emitBands).
        thread_local std::vector<float> samples(kFftSize);
        thread_local std::vector<std::complex<float>> spectrum(kFftSize);

        m_ringBuffer.ReadLatest(samples.data(), samples.size());

        for (std::size_t i = 0; i < kFftSize; ++i)
        {
            // Hann window: the FFT assumes it's analyzing one period of a periodic signal: an
            // arbitrary snippet's hard edges would otherwise leak energy across every bin.
            const float window = 0.5f - 0.5f * std::cos(
                2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kFftSize - 1));
            spectrum[i] = std::complex<float>(samples[i] * window, 0.0f);
        }

        fft(spectrum);

        constexpr std::size_t nyquistBin = kFftSize / 2;
        const std::size_t minBin = std::clamp<std::size_t>(binForFrequency(kMinFrequencyHz, kFftSize, m_sampleRate), 1, nyquistBin - 1);
        const std::size_t maxBin = std::clamp<std::size_t>(binForFrequency(kMaxFrequencyHz, kFftSize, m_sampleRate), minBin + 1, nyquistBin);

        std::array<float, kBandCount> bands{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const std::size_t startBin = logBinBoundary(band, kBandCount, minBin, maxBin);
            const std::size_t endBin = std::max(startBin + 1, logBinBoundary(band + 1, kBandCount, minBin, maxBin));
            float peakMagnitude = 0.0f;
            for (std::size_t bin = startBin; bin < endBin && bin <= maxBin; ++bin)
            {
                peakMagnitude = std::max(peakMagnitude, std::abs(spectrum[bin]));
            }
            const float normalized = peakMagnitude / static_cast<float>(kFftSize);
            bands[band] = std::clamp(std::sqrt(normalized) * kVisualGain, 0.0f, 1.0f);
        }

        constexpr auto byteSize = static_cast<uint32_t>(bands.size() * sizeof(float));
        Buffer buffer(byteSize);
        buffer.Length(byteSize);
        std::memcpy(buffer.data(), bands.data(), byteSize);

        m_waveformDataReadyEvent(*this, buffer);
    }

    winrt::event_token WaveformEngine::WaveformDataReady(
        winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::WaveformEngine, winrt::Windows::Storage::Streams::IBuffer> const& handler)
    {
        return m_waveformDataReadyEvent.add(handler);
    }

    void WaveformEngine::WaveformDataReady(winrt::event_token const& token) noexcept
    {
        m_waveformDataReadyEvent.remove(token);
    }
}
