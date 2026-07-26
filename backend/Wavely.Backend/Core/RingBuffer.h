#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace Wavely::Backend::Core
{
    /// Lock-free single-producer/single-consumer rolling buffer of float samples: the capture
    /// thread is the sole producer (Write), the waveform-emission call is the sole consumer
    /// (ReadLatest). Unlike a bounded FIFO queue, older samples are simply overwritten as new
    /// ones arrive - callers only ever want "the most recent N samples", never guaranteed
    /// delivery of every sample, so there is no head/tail bookkeeping to block on.
    class SpscRingBuffer
    {
    public:
        explicit SpscRingBuffer(std::size_t capacity)
            : m_buffer(capacity)
        {
        }

        void Write(const float* samples, std::size_t count) noexcept
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t writeIndex = m_writeIndex.load(std::memory_order_relaxed);
                m_buffer[writeIndex % m_buffer.size()] = samples[i];
                m_writeIndex.store(writeIndex + 1, std::memory_order_release);
            }
        }

        /// Copies the most recent `count` samples into `out`, oldest first. Zero-fills the front
        /// of `out` if fewer than `count` samples have been written yet.
        void ReadLatest(float* out, std::size_t count) const noexcept
        {
            const std::size_t writeIndex = m_writeIndex.load(std::memory_order_acquire);
            const std::size_t available = std::min(count, writeIndex);
            const std::size_t capacity = m_buffer.size();
            const std::size_t zeroFillCount = count - available;

            for (std::size_t i = 0; i < count; ++i)
            {
                if (i < zeroFillCount)
                {
                    out[i] = 0.0f;
                }
                else
                {
                    const std::size_t sampleIndex = writeIndex - (count - i);
                    out[i] = m_buffer[sampleIndex % capacity];
                }
            }
        }

    private:
        std::vector<float> m_buffer;
        std::atomic<std::size_t> m_writeIndex{ 0 };
    };
}
