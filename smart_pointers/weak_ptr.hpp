#pragma once

#include "shared_ptr.hpp"

namespace my_std {

template <typename T>
class weak_ptr {
private:
    T* ptr_;
    ctrl_blk_base* ctrl_blk_ptr_;

    template <typename S>
    friend class shared_ptr;

public:
    weak_ptr() noexcept
        : ptr_(nullptr), ctrl_blk_ptr_(nullptr) {}

    weak_ptr(const weak_ptr& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        if (ctrl_blk_ptr_) {
            ctrl_blk_ptr_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    weak_ptr(weak_ptr&& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        other.ptr_ = nullptr;
        other.ctrl_blk_ptr_ = nullptr;
    }

    template <typename S>
    requires convertible_ptr<S, T>
    weak_ptr(const shared_ptr<S>& other) noexcept
        : ptr_(other.ptr_), ctrl_blk_ptr_(other.ctrl_blk_ptr_)
    {
        if (ctrl_blk_ptr_) {
            ctrl_blk_ptr_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    weak_ptr& operator=(const weak_ptr& other) noexcept {
        if (this == &other) return *this;

        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->destroy();
        }

        ptr_ = other.ptr_;
        ctrl_blk_ptr_ = other.ctrl_blk_ptr_;

        if (ctrl_blk_ptr_) {
            ctrl_blk_ptr_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }

        return *this;
    }

    weak_ptr& operator=(weak_ptr&& other) noexcept {
        if (this == &other) return *this;

        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->destroy();
        }

        ptr_ = other.ptr_;
        ctrl_blk_ptr_ = other.ctrl_blk_ptr_;

        other.ptr_ = nullptr;
        other.ctrl_blk_ptr_ = nullptr;

        return *this;
    }

    template <typename S>
    requires convertible_ptr<S, T>
    weak_ptr& operator=(const shared_ptr<S>& other) noexcept {
        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->destroy();
        }

        ptr_ = other.ptr_;
        ctrl_blk_ptr_ = other.ctrl_blk_ptr_;

        if (ctrl_blk_ptr_) {
            ctrl_blk_ptr_->weak_count.fetch_add(1, std::memory_order_relaxed);
        }

        return *this;
    }

    ~weak_ptr() {
        if (ctrl_blk_ptr_ && ctrl_blk_ptr_->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ctrl_blk_ptr_->destroy();
        }
    }

    long use_count() const noexcept {
        return ctrl_blk_ptr_ == nullptr
            ? 0
            : ctrl_blk_ptr_->strong_count.load(std::memory_order_relaxed);
    }

    bool expired() const noexcept {
        return !ctrl_blk_ptr_
            || ctrl_blk_ptr_->strong_count.load(std::memory_order_relaxed) == 0;
    }

    shared_ptr<T> lock() const noexcept {
        return shared_ptr<T>(*this);
    }
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    auto* block = new combined_ctrl_blk<T>(1, 1);
    T* ptr;
    try {
        ptr = new(block->storage) T(std::forward<Args>(args)...);
    } catch (...) {
        delete block;
        throw;
    }
    return shared_ptr<T>(ptr, block);
}

int main() {
    shared_ptr<int> p1(new int(123));
    weak_ptr<int> p2(p1);
    shared_ptr<int> p3(p2);
    p2.lock();

    auto p4 = make_shared<int>(12);

    return 0;
}

}