#pragma once

#include <memory>
#include <cstddef>
#include <stdexcept>

namespace my_std {

template <typename T, typename Alloc = std::allocator<T>>
class vector2 {
private:
    Alloc allocator_;
    T* data_;
    std::size_t capacity_;
    std::size_t size_;

    void expand_(std::size_t capacity) {
        T* new_data = static_cast<T*>(allocator_.allocate(capacity));
        for (std::size_t i = 0; i < size_; i++) {
            new(&new_data[i]) T(std::move(data_[i]));
        }
        for (std::size_t i = 0; i < size_; i++) {
            data_[i].~T();
        }
        allocator_.deallocate(data_, capacity_);
        data_ = new_data;
        capacity_ = capacity;
    }

public:
    vector2() : data_(nullptr), capacity_(0), size_(0) {}

    vector2(const vector2& other) : capacity_(other.capacity_), size_(other.size_) {
        data_ = static_cast<T*>(allocator_.allocate(capacity_));
        for (std::size_t i = 0; i < size_; i++) {
            new(&data_[i]) T(other.data_[i]);
        }
    }

    vector2(vector2&& other) : data_(other.data_), capacity_(other.capacity_), size_(other.size_) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    vector2& operator=(const vector2& other) {
        if (this == &other) return *this;

        if (data_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                data_[i].~T();
            }
            allocator_.deallocate(data_, capacity_);
        }

        capacity_ = other.capacity_;
        size_ = other.size_;
        data_ = static_cast<T*>(allocator_.allocate(capacity_));
        for (std::size_t i = 0; i < size_; i++) {
            new(&data_[i]) T(other.data_[i]);
        }

        return *this;
    }

    vector2& operator=(vector2&& other) {
        if (this == &other) return *this;

        if (data_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                data_[i].~T();
            }
            allocator_.deallocate(data_, capacity_);
        }

        capacity_ = other.capacity_;
        size_ = other.size_;
        data_ = other.data_;
        
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;

        return *this;
    }

    ~vector2() {
        if (data_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                data_[i].~T();
            }
            allocator_.deallocate(data_, capacity_);
        }
    }

    void reserve(std::size_t size) {
        if (size > capacity_) {
            expand_(size);
        }
    }

    void push_back(const T& val) {
        if (size_ == capacity_) {
            expand_(capacity_ == 0 ? 1 : capacity_ << 1);
        }
        new (&data_[size_++]) T(val);
    }

    void push_back(T&& val) {
        if (size_ == capacity_) {
            expand_(capacity_ == 0 ? 1 : capacity_ << 1);
        }
        new(&data_[size_++]) T(std::move(val));
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            expand_(capacity_ == 0 ? 1 : capacity_ << 1);
        }
        new(&data_[size_++]) T(std::forward<Args>(args)...);
    }

    void pop_back() {
        if (size_ == 0) throw std::runtime_error("vector is already empty");
        size_--;
        data_[size_].~T();
    }

    std::size_t size() const { return size_; }

    bool empty() const { return size_ == 0; }

    T& operator[](std::size_t index) { return data_[index]; }

    const T& operator[](std::size_t index) const { return data_[index]; }

    void clear() {
        if (data_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                data_[i].~T();
            }
            allocator_.deallocate(data_, capacity_);
            data_ = nullptr;
        }
        
        capacity_ = 0;
        size_ = 0;
    }
};

}