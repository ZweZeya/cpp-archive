#pragma once

#include <memory>
#include <type_traits>
#include <cstddef>

namespace my_std {

template <typename T, typename Deleter = std::default_delete<T>>
class unique_ptr {
public:
    using pointer = T*;

private:
    pointer ptr_;
    [[no_unique_address]] Deleter deleter_{};

public:
    unique_ptr() noexcept requires (!std::is_pointer_v<Deleter>) : ptr_(nullptr) {}

    unique_ptr(std::nullptr_t) noexcept requires (!std::is_pointer_v<Deleter>) : ptr_(nullptr) {}

    explicit unique_ptr(pointer ptr) noexcept requires (!std::is_pointer_v<Deleter>) : ptr_(ptr) {}

    unique_ptr(pointer ptr, const Deleter& deleter) : ptr_(ptr), deleter_(deleter) {}

    unique_ptr(pointer ptr, Deleter&& deleter) : ptr_(ptr), deleter_(std::move(deleter)) {}

    unique_ptr(const unique_ptr&) = delete;

    unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_), deleter_(std::move(other.deleter_)) {
        other.ptr_ = nullptr;
    }

    template <typename U, typename E>
    requires std::is_convertible_v<U*, pointer> && std::is_convertible_v<E, Deleter>
    unique_ptr(unique_ptr<U, E>&& other) noexcept : ptr_(other.release()), deleter_(std::move(other.get_deleter())) {}

    ~unique_ptr() {
        if (ptr_ != nullptr) deleter_(ptr_);
    }

    unique_ptr& operator=(const unique_ptr&) = delete;

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this == &other) return *this;
        
        pointer tmp = other.release();
        reset(tmp);
        deleter_ = std::move(other.deleter_);

        return *this;
    }

    template <typename U, typename E>
    requires std::is_convertible_v<U*, pointer> && std::is_assignable_v<Deleter&, E&&>
    unique_ptr& operator=(unique_ptr<U, E>&& other) noexcept {
        pointer tmp = other.release();
        reset(tmp);
        deleter_ = std::move(other.get_deleter());

        return *this;
    }

    unique_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    pointer release() noexcept {
        pointer result = ptr_;
        ptr_ = nullptr;
        return result;
    }

    void reset(pointer ptr = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = ptr;

        if (old_ptr != nullptr) deleter_(old_ptr);
    }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    pointer get() const noexcept { return ptr_; }

    Deleter& get_deleter() noexcept { return deleter_; }

    const Deleter& get_deleter() const noexcept { return deleter_; }

    T& operator*() const noexcept { return *ptr_; }

    pointer operator->() const noexcept { return ptr_; }

};

}
