#pragma once

namespace my_std {

template <typename T>
class unique_ptr {
private:
    T* ptr_;

public:
    unique_ptr() : ptr_(nullptr) {}

    explicit unique_ptr(T* ptr) : ptr_(ptr) {}

    unique_ptr(const unique_ptr& other) = delete;

    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    unique_ptr& operator=(const unique_ptr& other) = delete;

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this == &other) return *this;

        delete ptr_;

        ptr_ = other.ptr_;
        other.ptr_ = nullptr;

        return *this;
    }

    ~unique_ptr() {
        delete ptr_;
    }

    T* get() const noexcept { return ptr_; }

    T& operator*() noexcept { return *ptr_; }

    T* operator->() const noexcept { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* release() noexcept {
        T* res = ptr_;
        ptr_ = nullptr;
        return res;
    }

    void reset(T* ptr = nullptr) {
        T* old_ptr = ptr_;
        ptr_ = ptr;
        delete old_ptr;
    }
};

}
