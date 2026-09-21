#pragma once

#include <cstddef>
#include <utility>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace my_std {

template <typename T>
class basic_string {
private:
    T* str_;
    std::size_t size_;
    std::size_t capacity_;

    void expand(std::size_t new_capacity) {
        T* new_str = static_cast<T*>(operator new(sizeof(T) * new_capacity));
        for (std::size_t i = 0; i < size_ + 1; i++) {
            new(&new_str[i]) T(std::move(str_[i]));
        }
        for (std::size_t i = 0; i < size_ + 1; i++) {
            str_[i].~T();
        }

        operator delete(str_);
        str_ = new_str;
        capacity_ = new_capacity;
    }

    void init_empty_str() {
        str_ = static_cast<T*>(operator new(sizeof(T)));
        new(&str_[0]) T('\0');
    }

public:
    basic_string() : size_(0), capacity_(1) {
        str_ = static_cast<T*>(operator new(sizeof(T)));
        new(&str_[0]) T('\0');
    }

    basic_string(const T* c_str) : size_(strlen(c_str)), capacity_(size_ + 1) {
        if (c_str == nullptr) throw std::logic_error("string initialisation with null is not valid.");

        str_ = static_cast<T*>(operator new(sizeof(T) * capacity_));
        for (std::size_t i = 0; i < size_ + 1; i++) {
            new(&str_[i]) T(c_str[i]);
        }
    }

    basic_string(const basic_string& other) : size_(other.size_), capacity_(other.capacity_) {
        str_ = static_cast<T*>(operator new(sizeof(T) * capacity_));
        for (std::size_t i = 0; i < size_ + 1; i++) {
            new(&str_[i]) T(std::move(other.str_[i]));
        }
    }

    basic_string(basic_string&& other) : str_(other.str_), size_(other.size_), capacity_(other.capacity_) {
        other.size_ = 0;
        other.capacity_ = 1;
        other.init_empty_str();
    }

    basic_string& operator=(const basic_string& other) {
        if (this == &other) return *this;

        if (str_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                str_[i].~T();
            }
            operator delete(str_);
        }
        
        size_ = other.size_;
        capacity_ = other.capacity_;
        str_ = static_cast<T*>(operator new(sizeof(T) * capacity_));

        for (std::size_t i = 0; i < size_; i++) {
            new(&str_[i]) T(other.str_[i]);
        }
        new(&str_[size_]) T('\0');

        return *this;
    }

    basic_string& operator=(basic_string&& other) {
        if (this == &other) return *this;

        if (str_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                str_[i].~T();
            }
            operator delete(str_);
        }
        
        str_ = other.str_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.size_ = 0;
        other.capacity_ = 1;
        other.init_empty_str();

        return *this;
    }

    ~basic_string() {
        if (str_ != nullptr) {
            for (std::size_t i = 0; i < size_; i++) {
                str_[i].~T();
            }
            operator delete(str_);
        }
    }

    void push_back(const T& c) {
        if (size_ + 1 >= capacity_) {
            expand(capacity_ == 0 ? size_ + 2 : capacity_ << 1);
        }
        new(&str_[size_ + 1]) T(std::move(str_[size_]));
        new(&str_[size_]) T(c);
        size_++;
    }

    void push_back(T&& c) {
        if (size_ + 1 >= capacity_) {
            expand(capacity_ == 0 ? size_ + 2 : capacity_ << 1);
        }
        new(&str_[size_ + 1]) T(std::move(str_[size_]));
        new(&str_[size_]) T(std::move(c));
        size_++;
    }

    void pop_back() {
        if (size_ == 0) throw std::runtime_error("string is empty");
        str_[size_ - 1] = std::move(str_[size_]);
        str_[size_].~T();
        size_--;
    }

    std::size_t size() const { return size_; }

    std::size_t length() const { return size_; }

    bool empty() const { return size_ == 0; }

    T& operator[](std::size_t index) { return str_[index]; }

    const T& operator[](std::size_t index) const { return str_[index]; }

    const T* c_str() const { return str_; }

    friend std::ostream& operator<<(std::ostream& os, const basic_string& str) {
        os << str.str_;
        return os;
    }
};

using string = basic_string<char>;

}
