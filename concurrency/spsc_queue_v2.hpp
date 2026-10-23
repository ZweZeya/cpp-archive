#pragma once 

#include <cstddef>
#include <array>
#include <atomic>
#include <optional>
#include <utility>
#include <new>
#include <concepts>

namespace my_std {

template <typename T, std::size_t N> requires (N > 0)
class spsc_queue_v2 {
private:
    constexpr static std::size_t Slots = N + 1;
    alignas(64) alignas(T) std::byte buffer_[sizeof(T) * Slots];
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;

    std::size_t get_offset(std::size_t index) const noexcept { return index * sizeof(T); }

public:
    spsc_queue_v2() : head_(0), tail_(0) {}

    spsc_queue_v2(const spsc_queue_v2&) = delete;

    spsc_queue_v2(spsc_queue_v2&&) = delete;

    spsc_queue_v2& operator=(const spsc_queue_v2&) = delete;

    spsc_queue_v2& operator=(spsc_queue_v2&&) = delete;

    ~spsc_queue_v2() {
        std::size_t current_head = head_.load(std::memory_order_relaxed);
        std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        while (current_tail != current_head) {
            T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[get_offset(current_tail)]));
            ptr->~T();
            current_tail = (current_tail + 1) % Slots;
        }
    }

    bool try_push(T&& item) {
        return try_emplace(std::forward<T>(item));
    }

    template <typename... Args>
    requires (std::constructible_from<T, Args...>)
    bool try_emplace(Args&&... args) {
        std::size_t current_tail = tail_.load(std::memory_order_acquire);
        std::size_t current_head = head_.load(std::memory_order_relaxed);
        std::size_t new_head = (current_head + 1) % Slots;
        if (new_head == current_tail) 
            return false;

        new(&buffer_[get_offset(current_head)]) T(std::forward<Args>(args)...);
        head_.store(new_head, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        std::size_t current_head = head_.load(std::memory_order_acquire);
        std::size_t current_tail = tail_.load(std::memory_order_relaxed);
        if (current_head == current_tail)
            return std::nullopt;

        T* ptr = std::launder(reinterpret_cast<T*>(&buffer_[get_offset(current_tail)]));
        std::optional<T> result(std::move(*ptr));
        ptr->~T();
        tail_.store((current_tail + 1) % Slots, std::memory_order_release);
        return result;
    }
};

}
