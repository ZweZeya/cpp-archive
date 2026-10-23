#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <new>

namespace my_std {

template <std::size_t N>
concept PositiveSize = N > 0;

template <typename T, std::size_t N>
requires PositiveSize<N>
class pool_allocator {
private:
    union blk {
        blk* next_;
        T data_;
    };

    blk* pool_;
    blk* free_;
    

public:
    using value_type = T;

    pool_allocator() : pool_(static_cast<blk*>(operator new(sizeof(blk) * N))), free_(pool_) {
        blk* curr = pool_;
        for (std::size_t i = 0; i < N - 1; i++) {
            curr->next_ = curr + 1;
            curr++;
        }
        curr->next_ = nullptr;
    }

    pool_allocator(const pool_allocator&) = delete;

    pool_allocator(pool_allocator&& other) noexcept : pool_(other.pool_), free_(other.free_) {
        other.pool_ = nullptr;
        other.free_ = nullptr;
    }

    ~pool_allocator() {
        operator delete(pool_);
    }

    pool_allocator& operator=(const pool_allocator&) = delete;

    pool_allocator& operator=(pool_allocator&& other) noexcept {
        if (this == &other) return *this;

        blk* old_pool = pool_;

        pool_ = other.pool_;
        free_ = other.free_;
        
        other.pool_ = nullptr;
        other.free_ = nullptr;

        operator delete(old_pool);

        return *this;
    }


    T* allocate() {
        if (free_ == nullptr) 
            throw std::bad_alloc();
        blk* alloc_blk = free_;
        free_ = free_->next_;
        return reinterpret_cast<T*>(alloc_blk);
    }

    void deallocate(T* p) {
        blk* dealloc_blk = reinterpret_cast<blk*>(p);
        assert(dealloc_blk >= pool_ && dealloc_blk < pool_ + N);
        dealloc_blk->next_ = free_;
        free_ = dealloc_blk;
    }
};

}