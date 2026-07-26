#pragma once
#include "WaveformEngine.g.h"

#include "Core/RingBuffer.h"

#include <mmdeviceapi.h>

#include <atomic>
#include <thread>

namespace winrt::Wavely::Backend::implementation
{
    /// Captures a live audio render device's output via WASAPI loopback on a dedicated thread
    /// and republishes it as kBandCount log-spaced frequency-domain magnitude bands (a real
    /// equalizer-style spectrum - not a time-domain amplitude history - per explicit product
    /// direction: each band reflects the *current* instant's bass/mid/treble energy, so bars
    /// move somewhat independently rather than "scrolling" like a timeline).
    ///
    /// Not fixed to the system default render device: apps commonly get individually redirected
    /// to a different output (Elgato Wave Link, VoiceMeeter, and similar routing/streaming
    /// software are common on the kind of machine that runs a media-widget app at all), which
    /// would otherwise make the waveform stay flat while music audibly plays. Every device is
    /// checked for an actively-rendering audio session (the same signal Volume Mixer's animated
    /// icon uses) and capture follows whichever one is actually busy, re-evaluated periodically.
    /// See docs/ADR-003-waveform-device-selection.md.
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
        void runCaptureSession(IMMDeviceEnumerator* enumerator, IMMDevice* device);
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
