#pragma once

#include <iostream>
#include <utility>
#include <atomic>
#include <memory>
#include <concepts>
#include <cstddef>

namespace my_std {

template <typename T>
class weak_ptr;

template <typename From, typename To>
concept convertible_ptr = std::convertible_to<From*, To*>;

struct ctrl_blk_base {
    alignas(64) std::atomic<long> strong_count;
    alignas(64) std::atomic<long> weak_count;

    ctrl_blk_base(long strong_count, long weak_count)
        : strong_count(std::atomic<long>(strong_count))
        , weak_count(std::atomic<long>(weak_count)) {}

    virtual void dispose() = 0;
    virtual void destroy() = 0;
    virtual ~ctrl_blk_base() = default;
};

template <typename T>
struct separate_ctrl_blk : ctrl_blk_base {
    T* ptr_;

    separate_ctrl_blk(long strong_count, long weak_count, T* ptr)
        : ctrl_blk_base(strong_count, weak_count), ptr_(ptr) {}

    void dispose() override {
        delete ptr_;
    }

    void destroy() override {
        delete this;
    }
}; 

template <typename T>
struct combined_ctrl_blk : ctrl_blk_base {
    alignas(T) unsigned char storage[sizeof(T)];

    combined_ctrl_blk(long strong_count, long weak_count)
        : ctrl_blk_base(strong_count, weak_count) {}

    void dispose() override {
        reinterpret_cast<T*>(storage)->~T();
    }

    void destroy() override {
        delete this;
    }
}; 

template <typename T>
class shared_ptr {
private:
    T* ptr_;
    ctrl_blk_base* ctrl_blk_ptr_;

    template <typename S>
    friend class weak_ptr;

    template <typename U, typename... Args>
    friend shared_ptr<U> make_shared(Args&&... args);

    shared_ptr(T* ptr, ctrl_blk_base* ctrl_blk_ptr) noexcept
        : ptr_(ptr), ctrl_blk_ptr_(ctrl_blk_ptr) {}

public:
    shared_ptr() noexcept
        : ptr_(nullptr), ctrl_blk_ptr_(nullptr) {}

    explicit shared_ptr(T* ptr)
        : ptr_(ptr), ctrl_blk_ptr_(nullptr) 
    {
        try {
            ctrl_blk_ptr_ = new separate_ctrl_blk(1, 1, ptr_);
        } catch (...) {
            delete ptr_;
            throw;
        }
    }
    
    shared_ptr(const shared_ptr& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        if (ctrl_blk_ptr_) 
            ctrl_blk_ptr_->strong_count.fetch_add(1, std::memory_order_relaxed);
    }

    shared_ptr(shared_ptr&& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        other.ptr_ = nullptr;
        other.ctrl_blk_ptr_ = nullptr;
    }

    template <typename S> 
    requires convertible_ptr<S, T>
    shared_ptr(const weak_ptr<S>& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        long count = ctrl_blk_ptr_ ? ctrl_blk_ptr_->strong_count.load(std::memory_order_relaxed) : 0;
        while (count != 0 
            && !ctrl_blk_ptr_->strong_count.compare_exchange_strong(count, count + 1, std::memory_order_relaxed)) {}
        if (count == 0) {
            ptr_ = nullptr;
            ctrl_blk_ptr_ = nullptr;
        }
    }

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this == &other) return *this;

        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->dispose();
            if (ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                ctrl_blk_ptr_->destroy();
            }
        }

        ptr_ = other.ptr_;
        ctrl_blk_ptr_ = other.ctrl_blk_ptr_;

        if (ctrl_blk_ptr_) 
            ctrl_blk_ptr_->strong_count.fetch_add(1, std::memory_order_relaxed);

        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this == &other) return *this;

        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->dispose();
            if (ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                ctrl_blk_ptr_->destroy();
            }
        }

        ptr_ = other.ptr_;
        ctrl_blk_ptr_ = other.ctrl_blk_ptr_;
        
        other.ptr_ = nullptr;
        other.ctrl_blk_ptr_ = nullptr;

        return *this;
    }

    ~shared_ptr() {
        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->dispose();
            if (ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                ctrl_blk_ptr_->destroy();
            }
        }
    }

    long use_count() const noexcept {
        return ctrl_blk_ptr_ == nullptr 
            ? 0 
            : ctrl_blk_ptr_->strong_count.load(std::memory_order_relaxed);
    }

    T* get() const noexcept {
        return ptr_;
    }

    T& operator*() noexcept {
        return *ptr_;
    }

    const T& operator*() const noexcept {
        return *ptr_;
    }

    T* operator->() const noexcept {
        return ptr_;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }
};

#include "weak_ptr.hpp"

}