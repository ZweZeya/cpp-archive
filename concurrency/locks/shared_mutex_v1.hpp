#pragma once

#include <semaphore>

namespace my_std {

class shared_mutex_v1 {
private:
    unsigned num_readers_{0};
    std::binary_semaphore turnstile_{1};    
    std::binary_semaphore resource_{1};    
    std::binary_semaphore reader_{1};    

public:
    shared_mutex_v1() noexcept = default;

    shared_mutex_v1(const shared_mutex_v1&) = delete;

    shared_mutex_v1& operator=(const shared_mutex_v1&) = delete;

    void lock() {
        turnstile_.acquire();
        resource_.acquire();
    }

    bool try_lock() {
        if (!turnstile_.try_acquire()) return false;

        if (!resource_.try_acquire()) {
            turnstile_.release();
            return false;
        }

        return true;
    }

    void unlock() {
        resource_.release();
        turnstile_.release();
    }

    void lock_shared() {
        turnstile_.acquire();
        turnstile_.release();

        reader_.acquire();
        num_readers_++;
        if (num_readers_ == 1) resource_.acquire();
        reader_.release();
    }

    bool try_lock_shared() {
        if (!turnstile_.try_acquire()) return false;
        turnstile_.release();

        if (!reader_.try_acquire()) {
            return false;
        }

        if (num_readers_ > 0 || resource_.try_acquire()) {
            num_readers_++;
            reader_.release();
            return true;
        }

        reader_.release();
        return false;
    }

    void unlock_shared() {
        reader_.acquire();
        num_readers_--;
        if (num_readers_ == 0) resource_.release();
        reader_.release();
    }
};

}
