#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <array>
#include <atomic>

namespace my_std {

template <typename T, std::size_t N>
class spsc_queue {
static_assert(N > 0, "N must be positive.");

private:
    static constexpr std::size_t kSlots = N + 1;

    std::array<T, kSlots> buffer_;
    alignas(64) std::atomic<std::size_t> head { 0 };
    alignas(64) std::atomic<std::size_t> tail { 0 };

public:
    spsc_queue() {}

    spsc_queue(const spsc_queue& other) = delete;

    spsc_queue(spsc_queue&& other) = delete;

    spsc_queue& operator=(const spsc_queue& other) = delete;

    spsc_queue& operator=(spsc_queue&& other) = delete;

    bool try_push(const T& value) {
        std::size_t current_tail = tail.load(std::memory_order_relaxed);
        std::size_t next_tail = (current_tail + 1) % kSlots;
        if (next_tail == head.load(std::memory_order_acquire)) return false;

        buffer_[current_tail] = value;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_push(T&& value) {
        std::size_t current_tail = tail.load(std::memory_order_relaxed);
        std::size_t next_tail = (current_tail + 1) % kSlots;
        if (next_tail == head.load(std::memory_order_acquire)) return false;

        buffer_[current_tail] = std::move(value);
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        std::size_t current_head = head.load(std::memory_order_relaxed);
        if (current_head == tail.load(std::memory_order_acquire)) return std::nullopt;

        auto res = std::make_optional<T>(std::move(buffer_[current_head]));
        head.store((current_head + 1) % kSlots, std::memory_order_release);
        return res;
    }
};

}