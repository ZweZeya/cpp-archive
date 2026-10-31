#pragma once

#include <condition_variable>
#include <mutex>

namespace my_std {

class shared_mutex_v2 {
private:
    bool writer_{false};
    unsigned readers_{0};
    std::mutex m_;
    std::condition_variable gate1_;
    std::condition_variable gate2_;

public:
    shared_mutex_v2() noexcept = default;

    shared_mutex_v2(const shared_mutex_v2&) = delete;

    shared_mutex_v2& operator=(const shared_mutex_v2&) = delete;

    void lock() {
        std::unique_lock lk(m_);
        gate1_.wait(lk, [this]{ return !writer_; });
        writer_ = true;
        gate2_.wait(lk, [this]{ return readers_ == 0; });
    }

    bool try_lock() {
        std::lock_guard lk(m_);
        if (readers_ > 0 || writer_) return false;
        writer_ = true;
        return true;
    }

    void unlock() {
        std::lock_guard lk(m_);
        writer_ = false;
        gate1_.notify_all();
    }

    void lock_shared() {
        std::unique_lock lk(m_);
        gate1_.wait(lk, [this]{ return !writer_; });
        readers_++;
    }

    bool try_lock_shared() {
        std::lock_guard lk(m_);
        if (writer_) return false;
        readers_++;
        return true;
    }

    void unlock_shared() {
        std::lock_guard lk(m_);
        readers_--;
        if (readers_ == 0 && writer_) gate2_.notify_one();
    }
};

}
