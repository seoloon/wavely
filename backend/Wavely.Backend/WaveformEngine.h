#pragma once
#include "WaveformEngine.g.h"

#include "Core/RingBuffer.h"

#include <mmdeviceapi.h>

#include <atomic>
#include <thread>

namespace winrt::Wavely::Backend::implementation
{
    /// Captures a whitelisted native music-streaming app's audio output (see
    /// Core/MusicAppAllowlist.h) via Windows' per-process WASAPI loopback capture on a dedicated
    /// thread and republishes it as kBandCount log-spaced frequency-domain magnitude bands (a
    /// real equalizer-style spectrum - not a time-domain amplitude history - per explicit product
    /// direction: each band reflects the *current* instant's bass/mid/treble energy, so bars
    /// move somewhat independently rather than "scrolling" like a timeline).
    ///
    /// Captures the target process's audio directly rather than a render device's mixed output:
    /// this both (a) restricts the waveform to just the whitelisted music app instead of
    /// reflecting whatever else happens to also be making noise (games, notifications, browser
    /// tabs, ...), and (b) captures "pre-routing" - before Elgato Wave Link/VoiceMeeter/similar
    /// per-app audio routing touches it - which sidesteps ADR-003's finding that some routed
    /// virtual devices hand back all-zero loopback samples even though the session is genuinely
    /// active. See docs/ADR-004-per-process-loopback-capture.md.
    struct WaveformEngine : WaveformEngineT<WaveformEngine>
    {
        WaveformEngine() = default;
        ~WaveformEngine();

        void Start();
        void Stop();
        winrt::event_token WaveformDataReady(winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::WaveformEngine, winrt::Windows::Storage::Streams::IBuffer> const& handler);
        void WaveformDataReady(winrt::event_token const& token) noexcept;

    private:
        void captureThreadProc();
        void runProcessCaptureSession(IMMDeviceEnumerator* enumerator, DWORD processId);
        void emitBands();

        static constexpr std::size_t kBandCount = 20;
        // Power of 2, required by the radix-2 FFT. At a typical 48kHz mix format this covers
        // ~21ms of audio per analysis window - short enough to feel live, long enough for
        // meaningful low-frequency resolution.
        static constexpr std::size_t kFftSize = 1024;
        static constexpr std::size_t kRingBufferCapacity = kFftSize * 4;

        std::thread m_captureThread;
        std::atomic<bool> m_running{ false };
        std::uint32_t m_sampleRate{ 48000 };
        ::Wavely::Backend::Core::SpscRingBuffer m_ringBuffer{ kRingBufferCapacity };
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Wavely::Backend::WaveformEngine, winrt::Windows::Storage::Streams::IBuffer>> m_waveformDataReadyEvent;
    };
}
namespace winrt::Wavely::Backend::factory_implementation
{
    struct WaveformEngine : WaveformEngineT<WaveformEngine, implementation::WaveformEngine>
    {
    };
}
