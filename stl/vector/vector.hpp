#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>

namespace my_std {

template <typename T>
class vector {
private:
    T* data_;
    std::size_t capacity_;
    std::size_t size_;

    void allocate_(std::size_t capacity);

    void clear_();

    void clear_and_delete_();

public:
    vector();

    vector(const vector& other);

    vector(vector&& other) noexcept;

    vector& operator=(const vector& other);

    vector& operator=(vector&& other) noexcept;

    ~vector();

    void push_back(const T& val);

    void push_back(T&& val);

    template <typename... Args>
    void emplace_back(Args&&... args);

    void reserve(std::size_t size);

    void pop_back();

    std::size_t size() const;

    std::size_t capacity() const;

    bool empty() const;

    void clear();

    T& operator[](std::size_t index);

    const T& operator[](std::size_t index) const;
};

template <typename T>
void vector<T>::allocate_(std::size_t capacity) {
    T* newData = static_cast<T*>(operator new(sizeof(T) * capacity));
    for (std::size_t i = 0; i < size_; i++) {
        new (&newData[i]) T(std::move(data_[i]));
    }
    capacity_ = capacity;
    for (std::size_t i = 0; i < size_; i++) {
        data_[i].~T();
    }
    operator delete(data_);
    data_ = newData;
}

template<typename T>
void vector<T>::clear_() {
    for (std::size_t i = 0; i < size_; i++) {
        data_[i].~T();
    }
    size_ = 0;
}

template<typename T>
void vector<T>::clear_and_delete_() {
    clear_();
    operator delete(data_);
}

template <typename T>
vector<T>::vector()
    : data_(nullptr), capacity_(0), size_(0) {}

template <typename T>
vector<T>::vector(const vector& other)
    : capacity_(other.capacity_), size_(other.size_)
{
    data_ = static_cast<T*>(operator new(sizeof(T) * capacity_));
    for (std::size_t i = 0; i < size_; i++) {
        new(&data_[i]) T(other.data_[i]);
    }
}

template <typename T>
vector<T>::vector(vector&& other) noexcept
    : data_(other.data_), capacity_(other.capacity_), size_(other.size_)
{
    other.data_ = nullptr;
    other.capacity_ = 0;
    other.size_ = 0;
}

template <typename T>
vector<T>& vector<T>::operator=(const vector<T>& other) {
    if (this == &other) return *this;

    if (data_) {
        clear_and_delete_();
    }

    capacity_ = other.capacity_;
    size_ = other.size_;
    data_ = static_cast<T*>(operator new(sizeof(T) * capacity_));
    for (std::size_t i = 0; i < size_; i++) {
        new(&data_[i]) T(other.data_[i]);
    }

    return *this;
}

template <typename T>
vector<T>& vector<T>::operator=(vector<T>&& other) noexcept {
    if (this == &other) return *this;

    if (data_) {
        clear_and_delete_();
    }

    data_ = other.data_;
    capacity_ = other.capacity_;
    size_ = other.size_;

    other.data_ = nullptr;
    other.capacity_ = 0;
    other.size_ = 0;

    return *this;
}

template <typename T>
vector<T>::~vector() {
    clear_and_delete_();
}

template <typename T>
void vector<T>::push_back(const T& val) {
    if (size_ == capacity_) {
        allocate_(capacity_ == 0 ? 1 : capacity_ << 1);
    }
    new (&data_[size_]) T(val);
    size_++;
}

template <typename T>
void vector<T>::push_back(T&& val) {
    if (size_ == capacity_) {
        allocate_(capacity_ == 0 ? 1 : capacity_ << 1);
    }
    new (&data_[size_]) T(std::move(val));
    size_++;
}

template <typename T>
template <typename... Args>
void vector<T>::emplace_back(Args&&... args) {
    if (size_ == capacity_) {
        allocate_(capacity_ == 0 ? 1 : capacity_ << 1);
    }
    new(&data_[size_]) T(std::forward<Args>(args)...);
    size_++;
}

template <typename T>
void vector<T>::reserve(std::size_t size) {
    if (size > capacity_) {
        allocate_(size);
    }
}

template <typename T>
void vector<T>::pop_back() {
    if (size_ == 0) throw std::runtime_error("already empty");

    size_--;
    data_[size_].~T();
}

template <typename T>
std::size_t vector<T>::size() const { return size_; }

template <typename T>
std::size_t vector<T>::capacity() const { return capacity_; }

template <typename T>
bool vector<T>::empty() const { return size_ == 0;}

template <typename T>
void vector<T>::clear() { clear_();}

template <typename T>
T& vector<T>::operator[](std::size_t index) { return data_[index]; }

template <typename T>
const T& vector<T>::operator[](std::size_t index) const { return data_[index]; }

}