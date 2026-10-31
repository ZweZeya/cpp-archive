#pragma once

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <new>
#include <utility>
#include <cassert>
#include <type_traits>
#include <memory>

namespace my_std {
  
template <typename T>
class vector {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    struct raw_deleter {
        void operator()(T* p) noexcept { operator delete(p); }
    };

    using raw_buffer = std::unique_ptr<T, raw_deleter>;

    pointer data_;
    std::size_t size_;
    std::size_t capacity_;

    T* allocate(std::size_t n) {
        return static_cast<T*>(operator new(sizeof(T) * n));
    }

    void move_elements(T* dst, T* src, std::size_t n) {
        std::size_t i;
        try {
            for (i = 0; i < n; i++) {
                new(&dst[i]) T(std::move_if_noexcept(src[i]));
            } 
        } catch(...) {
            destruct_range(dst, 0, i);
            throw;
        }
    }

    void destruct_range(T* data, std::size_t start, std::size_t end) {
        for (std::size_t i = start; i < end; i++) {
            data[i].~T();
        }
    }

    void destroy(T* data, std::size_t n) {
        destruct_range(data, 0, n);
        operator delete(data);
    }

public:
    vector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    vector(const vector& other) : size_(other.size_), capacity_(other.capacity_) {
        data_ = allocate(capacity_);
        std::size_t i;
        try {
            for (i = 0; i < size_; i++) {
                new(&data_[i]) T(other.data_[i]);
            }
        } catch(...) {
            destroy(data_, i);
            throw;
        }
    }

    vector(vector&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    ~vector() {
        destroy(data_, size_);
    }

    vector& operator=(const vector& other) {
        if (this == &other) return *this;

        T* new_data = allocate(other.capacity_);
        std::size_t i;
        try {
            for (i = 0; i < other.size_; i++) {
                new(&new_data[i]) T(other.data_[i]);
            }
        } catch(...) {
            destroy(new_data, i);
            throw;
        }

        T* old_data = data_;
        std::size_t old_size = size_;

        data_ = new_data;
        size_ = other.size_;
        capacity_ = other.capacity_;

        destroy(old_data, old_size);
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this == &other) return *this;

        T* old_data = data_;
        std::size_t old_size = size_;

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;

        destroy(old_data, old_size);

        return *this;
    }

    T* data() { return data_; }

    const T* data() const { return data_; }

    reference operator[](std::size_t index) { return data_[index]; }

    const_reference operator[](std::size_t index) const { return data_[index]; }

    reference at(std::size_t index) {
        if (index >= size_) throw std::out_of_range("Index out of range");
        return data_[index];
    }

    const_reference at(std::size_t index) const {
        if (index >= size_) throw std::out_of_range("Index out of range");
        return data_[index];
    }

    reference front() { return data_[0]; }

    const_reference front() const { return data_[0]; }

    reference back() { return data_[size_ - 1]; }

    const_reference back() const { return data_[size_ - 1]; }

    iterator begin() { return data_; }

    iterator end() { return data_ + size_; }

    const_iterator cbegin() const noexcept { return data_; }

    const_iterator cend() const noexcept { return data_ + size_; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }

    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return size_ == 0; }

    std::size_t size() const noexcept { return size_; }

    std::size_t capacity() const noexcept { return capacity_; }

    void reserve(std::size_t n) {
        if (n <= capacity_) return;

        raw_buffer new_data(allocate(n));
        move_elements(new_data.get(), data_, size_);

        T* old_data = data_;
        data_ = new_data.release();
        capacity_ = n;
        destroy(old_data, size_);
    }

    void push_back(T&& value) {
        emplace_back(std::forward<T>(value));
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            std::size_t new_capacity = capacity_ == 0 ? 1 : capacity_ << 1;

            raw_buffer new_data(allocate(new_capacity));

            new(&new_data.get()[size_]) T(std::forward<Args>(args)...);
            try {
                move_elements(new_data.get(), data_, size_);
            } catch (...) {
                new_data.get()[size_].~T();
                throw;
            }

            T* old_data = data_;
            data_ = new_data.release();
            size_++;
            capacity_ = new_capacity;
            destroy(old_data, size_ - 1);
        } else {
            new(&data_[size_++]) T(std::forward<Args>(args)...);

        }
    }

    void pop_back() {
        if (size_ == 0) 
            throw std::runtime_error("vector is empty");

        data_[--size_].~T();
    }

    void clear() {
        destruct_range(data_, 0, size_);
        size_ = 0;
    }

    void resize(std::size_t n) requires std::is_default_constructible_v<T>  {
        if (n == size_) return;

        if (n < size_) {
            destruct_range(data_, n, size_);
            size_ = n;
            return;
        }
        
        if (n > capacity_) {
            size_t new_capacity = capacity_;
            while (n > new_capacity) {
                new_capacity = new_capacity == 0 ? 1 : new_capacity << 1;
            }
            
            raw_buffer new_data(allocate(new_capacity));
            move_elements(new_data.get(), data_, size_);

            T* old_data = data_;
            data_ = new_data.release();
            capacity_ = new_capacity;
            destroy(old_data, size_);
        }
        
        std::size_t i;
        try {
            for (i = size_; i < n; i++) {
                new (&data_[i]) T{};
            }
            size_ = n;
        } catch(...) {
            destruct_range(data_, size_, i);
            throw;
        }
    }
};

}