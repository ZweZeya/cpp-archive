#pragma once 

#include <cstddef>
#include <memory>
#include <new>

namespace my_std {

class arena {
private:
    std::byte* buffer_;
    std::size_t offset_;
    std::size_t capacity_;

public:
    explicit arena(std::size_t size) 
        : buffer_(static_cast<std::byte*>(operator new(size))), offset_(0), capacity_(size) {}

    arena(const arena&) = delete;
    arena(arena&&) = delete;

    ~arena() {
        operator delete(buffer_);
    }

    void* allocate(std::size_t size, std::size_t alignment) {
        std::byte* current_ptr = buffer_ + offset_;
        std::size_t space = capacity_ - offset_;
        void* align_ptr = current_ptr;

        if (std::align(alignment, size, align_ptr, space) == nullptr) 
            throw std::bad_alloc();

        offset_ = static_cast<std::byte*>(align_ptr) - buffer_ + size;
        return align_ptr;
    }

    void reset() {
        offset_ = 0;
    }
};

template <typename T>
class arena_allocator {
private:
    Arena* arena_;

    template <typename U>
    friend class arena_allocator;

public:
    using value_type = T;

    explicit arena_allocator(Arena* arena) : arena_(arena) {}

    template <typename U>
    arena_allocator(const arena_allocator<U>& other) noexcept : arena_(other.arena) {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(arena_->allocate(sizeof(T) * n, alignof(T)));
    }

    void deallocate(T*, std::size_t) {}

    bool operator==(const arena_allocator& other) {
        return this == other;
    }

    bool operator!=(const arena_allocator& other) {
        return this != other;
    }
};
    
}