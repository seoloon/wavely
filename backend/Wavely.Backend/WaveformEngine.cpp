#include "pch.h"
#include "WaveformEngine.h"
#include "WaveformEngine.g.cpp"

#include "Core/ComPtr.h"
#include "Core/WinrtGuard.h"

#include <audioclient.h>
#include <audiopolicy.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace winrt::Wavely::Backend::implementation
{
    namespace
    {
        using winrt::Windows::Storage::Streams::Buffer;
        using winrt::Windows::Storage::Streams::IBuffer;

        constexpr std::chrono::milliseconds kEmitInterval{ 16 };
        constexpr std::chrono::milliseconds kPollInterval{ 5 };
        constexpr std::chrono::seconds kDeviceReevaluationInterval{ 2 };
        constexpr REFERENCE_TIME kBufferDuration = 200 * 10'000; // 200ms, expressed in 100ns units.
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

        std::wstring getDeviceId(IMMDevice* device)
        {
            if (device == nullptr)
            {
                return {};
            }
            LPWSTR rawId = nullptr;
            if (FAILED(device->GetId(&rawId)))
            {
                return {};
            }
            const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> id(rawId, CoTaskMemFree);
            return std::wstring(id.get());
        }

        /// True if `device` has at least one audio session actively rendering right now - the
        /// same state Volume Mixer's per-app animated icon reflects.
        bool deviceHasActiveSession(IMMDevice* device)
        {
            ::Wavely::Backend::Core::ComPtr<IAudioSessionManager2> sessionManager;
            if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, sessionManager.put_void())))
            {
                return false;
            }

            ::Wavely::Backend::Core::ComPtr<IAudioSessionEnumerator> sessionEnumerator;
            if (FAILED(sessionManager->GetSessionEnumerator(sessionEnumerator.put())))
            {
                return false;
            }

            int sessionCount = 0;
            if (FAILED(sessionEnumerator->GetCount(&sessionCount)))
            {
                return false;
            }

            for (int i = 0; i < sessionCount; ++i)
            {
                ::Wavely::Backend::Core::ComPtr<IAudioSessionControl> session;
                if (FAILED(sessionEnumerator->GetSession(i, session.put())))
                {
                    continue;
                }
                AudioSessionState state{};
                if (SUCCEEDED(session->GetState(&state)) && state == AudioSessionStateActive)
                {
                    return true;
                }
            }
            return false;
        }

        /// Some virtual audio devices (Elgato Wave Link's routing endpoints, observed directly
        /// on this project - see docs/ADR-003) report an actively-rendering session but hand back
        /// all-zero sample data on WASAPI loopback: AUDCLNT_BUFFERFLAGS_SILENT is never set, the
        /// buffer is just zeroed. deviceHasActiveSession alone can't tell the difference, so this
        /// briefly opens a real loopback capture on the candidate and checks whether any sample
        /// is actually non-zero before committing to it for the full capture session.
        bool deviceHasRealAudioData(IMMDevice* device)
        {
            ::Wavely::Backend::Core::ComPtr<IAudioClient> audioClient;
            if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, audioClient.put_void())))
            {
                return false;
            }

            WAVEFORMATEX* rawMixFormat = nullptr;
            if (FAILED(audioClient->GetMixFormat(&rawMixFormat)))
            {
                return false;
            }
            const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mixFormat(rawMixFormat, CoTaskMemFree);
            if (mixFormat->wBitsPerSample != 32)
            {
                return false;
            }
            const std::uint32_t channelCount = mixFormat->nChannels;

            constexpr REFERENCE_TIME probeBufferDuration = 100 * 10'000; // 100ms, in 100ns units.
            if (FAILED(audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, probeBufferDuration, 0, mixFormat.get(), nullptr)))
            {
                return false;
            }

            ::Wavely::Backend::Core::ComPtr<IAudioCaptureClient> captureClient;
            if (FAILED(audioClient->GetService(__uuidof(IAudioCaptureClient), captureClient.put_void())))
            {
                return false;
            }

            if (FAILED(audioClient->Start()))
            {
                return false;
            }

            bool foundNonZero = false;
            for (int attempt = 0; attempt < 6 && !foundNonZero; ++attempt)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));

                UINT32 packetLength = 0;
                if (FAILED(captureClient->GetNextPacketSize(&packetLength)))
                {
                    break;
                }
                while (packetLength != 0)
                {
                    BYTE* data = nullptr;
                    UINT32 frameCount = 0;
                    DWORD flags = 0;
                    if (FAILED(captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr)))
                    {
                        packetLength = 0;
                        break;
                    }
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                    {
                        const auto* samples = reinterpret_cast<const float*>(data);
                        const std::uint32_t sampleCount = frameCount * channelCount;
                        for (std::uint32_t i = 0; i < sampleCount && !foundNonZero; ++i)
                        {
                            foundNonZero = samples[i] != 0.0f;
                        }
                    }
                    captureClient->ReleaseBuffer(frameCount);
                    if (foundNonZero || FAILED(captureClient->GetNextPacketSize(&packetLength)))
                    {
                        break;
                    }
                }
            }

            audioClient->Stop();
            return foundNonZero;
        }

        /// Prefers the system default render device (the common case, and cheapest to check);
        /// falls back to scanning every active render device for one with a live session AND
        /// real (non-zero) loopback data - the per-app-routed case (Elgato Wave Link,
        /// VoiceMeeter, and similar). Some per-app virtual routing (observed directly: Elgato
        /// Wave Link's independent per-app channels, e.g. for Spotify) never surfaces real audio
        /// on any WASAPI loopback-able endpoint at all - that is an architectural limit of the
        /// routing software, not something detectable/fixable here, so the final fallback is
        /// still the default device even when nothing checked out as genuinely audible.
        ::Wavely::Backend::Core::ComPtr<IMMDevice> findBestRenderDevice(IMMDeviceEnumerator* enumerator)
        {
            ::Wavely::Backend::Core::ComPtr<IMMDevice> defaultDevice;
            enumerator->GetDefaultAudioEndpoint(eRender, eConsole, defaultDevice.put());
            if (defaultDevice && deviceHasActiveSession(defaultDevice.get()) && deviceHasRealAudioData(defaultDevice.get()))
            {
                return defaultDevice;
            }

            ::Wavely::Backend::Core::ComPtr<IMMDeviceCollection> collection;
            if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put())))
            {
                UINT count = 0;
                collection->GetCount(&count);
                for (UINT i = 0; i < count; ++i)
                {
                    ::Wavely::Backend::Core::ComPtr<IMMDevice> candidate;
                    if (FAILED(collection->Item(i, candidate.put())))
                    {
                        continue;
                    }
                    if (deviceHasActiveSession(candidate.get()) && deviceHasRealAudioData(candidate.get()))
                    {
                        return candidate;
                    }
                }
            }

            return defaultDevice;
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
            const auto device = findBestRenderDevice(enumerator.get());
            if (!device)
            {
                logFailure(L"no render device available", E_FAIL);
                std::this_thread::sleep_for(kDeviceReevaluationInterval);
                continue;
            }
            runCaptureSession(enumerator.get(), device.get());
        }
    }

    void WaveformEngine::runCaptureSession(IMMDeviceEnumerator* enumerator, IMMDevice* device)
    {
        ::Wavely::Backend::Core::ComPtr<IAudioClient> audioClient;
        HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, audioClient.put_void());
        if (FAILED(hr))
        {
            logFailure(L"failed to activate the audio client", hr);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }

        WAVEFORMATEX* rawMixFormat = nullptr;
        hr = audioClient->GetMixFormat(&rawMixFormat);
        if (FAILED(hr))
        {
            logFailure(L"failed to get the mix format", hr);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }
        const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mixFormat(rawMixFormat, CoTaskMemFree);

        if (mixFormat->wBitsPerSample != 32)
        {
            // The WASAPI shared-mode engine format is effectively always 32-bit IEEE float on
            // modern Windows; bail rather than mis-decode an unexpected format.
            logFailure(L"unexpected non-float mix format", E_NOTIMPL);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }
        const std::uint32_t channelCount = mixFormat->nChannels;
        m_sampleRate = mixFormat->nSamplesPerSec;

        hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, kBufferDuration, 0, mixFormat.get(), nullptr);
        if (FAILED(hr))
        {
            logFailure(L"failed to initialize the audio client", hr);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }

        ::Wavely::Backend::Core::ComPtr<IAudioCaptureClient> captureClient;
        hr = audioClient->GetService(__uuidof(IAudioCaptureClient), captureClient.put_void());
        if (FAILED(hr))
        {
            logFailure(L"failed to get the capture client", hr);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }

        hr = audioClient->Start();
        if (FAILED(hr))
        {
            logFailure(L"failed to start the audio client", hr);
            std::this_thread::sleep_for(kDeviceReevaluationInterval);
            return;
        }

        const std::wstring currentDeviceId = getDeviceId(device);
        auto lastEmit = std::chrono::steady_clock::now();
        auto lastDeviceCheck = lastEmit;
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
                    monoMixInto(reinterpret_cast<const float*>(data), frameCount, channelCount, monoScratch);
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

            if (now - lastDeviceCheck >= kDeviceReevaluationInterval)
            {
                lastDeviceCheck = now;
                const auto bestDevice = findBestRenderDevice(enumerator);
                if (getDeviceId(bestDevice.get()) != currentDeviceId)
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
