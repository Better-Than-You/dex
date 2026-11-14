#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>

namespace dex {

/// Lock-free single-producer / single-consumer ring buffer.
///
/// One thread writes (`push`), one thread reads (`pop`). Capacity must be a
/// power of two. When the buffer is full the producer drops the newest
/// samples and reports the loss through `dropped()` so the consumer can
/// recover instead of blocking the real-time audio callback.
template <typename T, std::size_t Capacity>
class SpscRingBuffer
{
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "SpscRingBuffer capacity must be a power of two");

public:
    static constexpr std::size_t capacity() { return Capacity; }

    /// Producer thread. Returns the number of samples actually stored.
    std::size_t push(const T *samples, std::size_t count)
    {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t head = m_head.load(std::memory_order_acquire);
        const std::size_t freeSlots = Capacity - (tail - head);
        const std::size_t toWrite = count < freeSlots ? count : freeSlots;

        if (toWrite < count)
            m_dropped.fetch_add(count - toWrite, std::memory_order_relaxed);

        if (toWrite == 0)
            return 0;

        const std::size_t first = Capacity - (tail & (Capacity - 1));
        const std::size_t chunk = toWrite < first ? toWrite : first;
        std::memcpy(m_data.data() + (tail & (Capacity - 1)), samples, chunk * sizeof(T));
        if (toWrite > chunk) {
            std::memcpy(m_data.data(), samples + chunk, (toWrite - chunk) * sizeof(T));
        }

        m_tail.store(tail + toWrite, std::memory_order_release);
        return toWrite;
    }

    /// Consumer thread. Returns the number of samples read.
    std::size_t pop(T *samples, std::size_t count)
    {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t tail = m_tail.load(std::memory_order_acquire);
        const std::size_t available = tail - head;
        const std::size_t toRead = count < available ? count : available;

        if (toRead == 0)
            return 0;

        const std::size_t first = Capacity - (head & (Capacity - 1));
        const std::size_t chunk = toRead < first ? toRead : first;
        std::memcpy(samples, m_data.data() + (head & (Capacity - 1)), chunk * sizeof(T));
        if (toRead > chunk)
            std::memcpy(samples + chunk, m_data.data(), (toRead - chunk) * sizeof(T));

        m_head.store(head + toRead, std::memory_order_release);
        return toRead;
    }

    /// Consumer thread.
    std::size_t available() const
    {
        const std::size_t tail = m_tail.load(std::memory_order_acquire);
        const std::size_t head = m_head.load(std::memory_order_acquire);
        return tail - head;
    }

    /// Samples lost because the consumer fell behind.
    std::size_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }

    /// Only safe when neither side is running (e.g. between stop/start).
    void reset()
    {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
        m_dropped.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> m_data{};
    std::atomic<std::size_t> m_head{0}; // consumer position
    std::atomic<std::size_t> m_tail{0}; // producer position
    std::atomic<std::size_t> m_dropped{0};
};

} // namespace dex
