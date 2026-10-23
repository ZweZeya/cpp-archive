#pragma once

#include <cstddef>
#include <stdexcept>
#include <iterator>
#include <iostream>
#include <string>
#include <algorithm>

namespace my_std {

template <typename CharT, typename Traits = std::char_traits<CharT>>
class basic_string {
public:
    using value_type = CharT;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = CharT*;
    using const_iterator = const CharT*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    static constexpr std::size_t INLINE_BUFFER_SIZE = 16;

    std::size_t size_;
    CharT* ptr_;

    union {
        std::size_t capacity_;
        CharT buffer_[INLINE_BUFFER_SIZE];
    };

    CharT* allocate(std::size_t n) {
        return static_cast<CharT*>(operator new(sizeof(CharT) * n));
    }

    bool is_local() const noexcept { return ptr_ == buffer_; }

public:
    basic_string() noexcept : size_(0), ptr_(buffer_) {
        buffer_[0] = '\0';
    }

    basic_string(const CharT* s) : size_(Traits::length(s)) {
        if (size_ < INLINE_BUFFER_SIZE) {
            ptr_ = buffer_;
        } else {
            ptr_ = allocate(size_ + 1);
            capacity_ = size_;
        }
        Traits::copy(ptr_, s, size_ + 1);
    }

    basic_string(const basic_string& other) : size_(other.size_) {
        if (size_ < INLINE_BUFFER_SIZE) {
            ptr_ = buffer_;
        } else {
            capacity_ = other.capacity_;
            ptr_ = allocate(capacity_ + 1);
        }
        Traits::copy(ptr_, other.ptr_, size_ + 1);
    }

    basic_string(basic_string&& other) noexcept : size_(other.size_) {
        if (other.is_local()) {
            ptr_ = buffer_;
            Traits::copy(ptr_, other.ptr_, size_ + 1);
        } else {
            ptr_ = other.ptr_;
            capacity_ = other.capacity_;
        }
        other.size_ = 0;
        other.buffer_[0] = '\0';
        other.ptr_ = other.buffer_;
    }

    basic_string& operator=(const basic_string& other) {
        if (this == &other) return *this;

        std::size_t new_size = other.size_;
        CharT* new_ptr;
        bool is_prev_heap = !is_local();
        CharT* old_ptr = ptr_;

        if (new_size < INLINE_BUFFER_SIZE) {
            new_ptr = buffer_;
        } else {
            new_ptr = allocate(new_size + 1);
            capacity_ = new_size;
        }

        Traits::copy(new_ptr, other.ptr_, new_size + 1);

        size_ = new_size;
        ptr_ = new_ptr;

        if (is_prev_heap)
            operator delete(old_ptr);

        return *this;
    }

    basic_string& operator=(basic_string&& other) noexcept {
        if (this == &other) return *this;

        CharT* new_ptr;
        bool is_prev_heap = !is_local();
        CharT* old_ptr = ptr_;

        if (other.is_local()) {
            new_ptr = buffer_;
            Traits::copy(new_ptr, other.ptr_, other.size_ + 1);
        } else {
            new_ptr = other.ptr_;
            capacity_ = other.capacity_;
        }

        size_ = other.size_;
        ptr_ = new_ptr;

        other.size_ = 0;
        other.buffer_[0] = '\0';
        other.ptr_ = other.buffer_;

        if (is_prev_heap)
            operator delete(old_ptr);

        return *this;
    }

    ~basic_string() {
        if (!is_local())
            operator delete(ptr_);
    }

    reference at(std::size_t index) {
        if (index >= size_) 
            throw std::out_of_range("Index is out of range");
        return ptr_[index];
    }

    const_reference at(std::size_t index) const {
        if (index >= size_) 
            throw std::out_of_range("Index is out of range");
        return ptr_[index];
    }

    reference operator[](std::size_t index) { return ptr_[index]; }

    const_reference operator[](std::size_t index) const { return ptr_[index]; }

    reference front() { return ptr_[0]; }

    const_reference front() const { return ptr_[0]; }

    reference back() { return ptr_[size_ - 1]; }

    const_reference back() const { return ptr_[size_ - 1]; }

    const CharT* data() const { return ptr_; }

    const CharT* c_str() const { return ptr_; }

    iterator begin() { return ptr_; }

    iterator end() { return ptr_ + size_; }

    const_iterator cbegin() { return ptr_; }

    const_iterator cend() { return ptr_ + size_; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }

    const_reverse_iterator crbegin() { return const_reverse_iterator(cend()); }

    const_reverse_iterator crend() { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return size_ == 0; }

    std::size_t size() const noexcept { return size_; }

    void reserve(std::size_t n) {
        if (n < INLINE_BUFFER_SIZE) return;

        if (!is_local() && n <= capacity_) return;

        CharT* new_ptr = allocate(n + 1);
        Traits::copy(new_ptr, ptr_, size_ + 1);

        bool is_prev_heap = !is_local();
        CharT* old_ptr = ptr_;
        ptr_ = new_ptr;
        capacity_ = n;

        if (is_prev_heap)
            operator delete(old_ptr);
    }

    std::size_t length() const noexcept { return size_; }

    std::size_t capacity() const noexcept { 
        return is_local() ? INLINE_BUFFER_SIZE - 1 : capacity_; 
    }

    void clear() {
        ptr_[0] = '\0';
        size_ = 0;
    }

    void push_back(CharT ch) {
        std::size_t current_capacity = capacity();
        if (size_ == current_capacity) {
            std::size_t new_capacity = current_capacity << 1;
            reserve(new_capacity);
        }
        
        ptr_[size_] = ch;
        size_++;
        ptr_[size_] = '\0';
    }

    basic_string& operator+=(CharT ch) {
        push_back(ch);
        return *this;
    }

    basic_string& operator+=(const CharT* s) {
        return append(s, Traits::length(s));
    }

    basic_string& operator+=(const basic_string& s) {
        return append(s.ptr_, s.size_);
    }

    basic_string& append(const CharT* s, std::size_t count) {
        std::size_t new_size = size_ + count;

        if (new_size <= capacity()) {
            Traits::copy(ptr_ + size_, s, count);
            size_ = new_size;
            ptr_[size_] = '\0';
            return *this;
        }

        std::size_t new_capacity = new_size << 1;

        CharT* new_ptr = allocate(new_capacity + 1);
        Traits::copy(new_ptr , ptr_, size_);
        Traits::copy(new_ptr + size_, s, count);

        bool is_prev_heap = !is_local();
        CharT* old_ptr = ptr_;
        ptr_ = new_ptr;
        size_ = new_size;
        capacity_ = new_capacity;
        ptr_[size_] = '\0';

        if (is_prev_heap)
            operator delete(old_ptr);
        return *this;
    }

    bool operator==(const basic_string& other) const {
        return size_ == other.size_
            ? Traits::compare(ptr_, other.ptr_, size_ + 1) == 0
            : false;
    }

    friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const basic_string& s) {
        os << s.c_str();
        return os;
    }
};

}