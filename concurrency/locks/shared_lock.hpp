#pragma once

#include <system_error>
#include "common.hpp"

namespace my_std {

template <typename Mutex>
class shared_lock {
private:
    Mutex* m_;
    bool owns_;

    void check_can_lock() {
        if (!m_) throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
        if (owns_) throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
    }

public:
    shared_lock() noexcept : m_(nullptr), owns_(false) {}

    explicit shared_lock(Mutex& m) : m_(&m), owns_(false) {
        lock();
    }

    shared_lock(Mutex& m, my_std::defer_lock_t) noexcept : m_(&m), owns_(false) {}

    shared_lock(Mutex& m, my_std::try_to_lock_t) : m_(&m), owns_(false) {
        try_lock();
    }

    shared_lock(Mutex& m, my_std::adopt_lock_t) noexcept : m_(&m), owns_(true) {}

    shared_lock(const shared_lock&) = delete;

    shared_lock(shared_lock&& other) noexcept : m_(other.m_), owns_(other.owns_) {
        other.m_ = nullptr;
        other.owns_ = false;
    }

    shared_lock& operator=(const shared_lock&) = delete;

    shared_lock& operator=(shared_lock&& other) noexcept {
        if (this == &other) return *this;

        if (owns_) m_->unlock_shared();

        m_ = other.m_;
        owns_ = other.owns_;

        other.m_ = nullptr;
        other.owns_ = false;
        return *this;
    }

    ~shared_lock() { if (owns_) m_->unlock_shared(); }

    void lock() { 
        check_can_lock();
        m_->lock_shared();
        owns_ = true;
    }

    bool try_lock() {
        check_can_lock();
        if (!m_->try_lock_shared()) return false;
        owns_ = true;
        return true;
    }

    void unlock() {
        if (!owns_) throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
        m_->unlock_shared();
        owns_ = false;
    }

    Mutex* release() noexcept {
        Mutex* released_m = m_;
        m_ = nullptr;
        owns_ = false;
        return released_m;
    }

    Mutex* mutex() const noexcept { return m_; }

    bool owns_lock() const noexcept { return owns_; }

    explicit operator bool() const noexcept { return owns_; }

    void swap(shared_lock& other) noexcept {
        Mutex* tmp_m = m_;
        bool tmp_owns = owns_;

         m_ = other.m_;
        owns_ = other.owns_;

        other.m_ = tmp_m;
        other.owns_ = tmp_owns;
    }

    friend void swap(shared_lock& lk1, shared_lock& lk2) noexcept {
        lk1.swap(lk2);
    }
};    

}
