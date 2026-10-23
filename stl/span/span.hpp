#pragma once

#include <cstddef>
#include <limits>
#include <array>
#include <type_traits>
#include <cassert>
#include <stdexcept>
#include <iterator>

namespace my_std {

inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();


template <class T, std::size_t Extent>
struct span_storage {
    T* data_;
    constexpr span_storage(T* data, [[maybe_unused]] std::size_t size) noexcept : data_(data) {
        assert(size == Extent);
    }
    constexpr std::size_t size() const noexcept { return Extent; }
};

template <class T>
struct span_storage<T, dynamic_extent> {
    T* data_;
    std::size_t size_;
    constexpr span_storage(T* data, std::size_t size) noexcept : data_(data), size_(size) {}
    constexpr std::size_t size() const noexcept { return size_; }
};

template <class T, std::size_t Extent = dynamic_extent>
class Span {
private:
    span_storage<T, Extent> storage_;

public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using iterator = T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    
    constexpr explicit(Extent != dynamic_extent)
    Span(T* data, std::size_t size) noexcept : storage_(data, size) {}

    template <std::size_t N>
    requires (Extent == dynamic_extent || Extent == N)
    constexpr Span(std::type_identity_t<T> (&arr)[N]) noexcept : storage_(arr, N) {}

    template <class U, std::size_t N>
    requires (Extent == dynamic_extent || Extent == N) && std::is_convertible_v<U(*)[], T(*)[]>
    constexpr Span(std::array<U, N>& arr) noexcept : storage_(arr.data(), N) {}

    template <class U, std::size_t N>
    requires (Extent == dynamic_extent || Extent == N) && std::is_convertible_v<const U(*)[], T(*)[]>
    constexpr Span(const std::array<U, N>& arr) noexcept : storage_(arr.data(), N) {}

    template <class U, std::size_t N>
    requires (Extent == dynamic_extent || Extent == N || N == dynamic_extent) && std::is_convertible_v<U(*)[], T(*)[]>
    constexpr explicit(Extent != dynamic_extent && N == dynamic_extent)
    Span(const Span<U, N>& other) noexcept : storage_(other.data(), other.size()) {}

    constexpr T& front() const {
        assert(!empty());
        return storage_.data_[0];
    }

    constexpr T& back() const {
        assert(!empty());
        return storage_.data_[size() - 1];
    }

    constexpr T& at(std::size_t index) const {
        if (index >= size()) 
            throw std::out_of_range("Out of range");
        return storage_.data_[index];
    }

    constexpr T& operator[](std::size_t index) const {
        assert(index < size());
        return storage_.data_[index];
    }
    
    constexpr T* data() const noexcept {
        return storage_.data_;
    }

    constexpr std::size_t size() const noexcept {
        return storage_.size();
    }

    constexpr std::size_t size_bytes() const noexcept { return size() * sizeof(T); }

    constexpr bool empty() const noexcept { return size() == 0; }

    constexpr iterator begin() const noexcept { return storage_.data_; }

    constexpr iterator end() const noexcept { return storage_.data_ + size(); }

    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

    constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    template <std::size_t Count>
    requires (Count <= Extent)
    constexpr Span<T, Count> first() const noexcept {
        assert(Count <= size());
        return Span<T, Count>(storage_.data_, Count);
    }

    template <std::size_t Count>
    requires (Count <= Extent)
    constexpr Span<T, Count> last() const noexcept {
        assert(Count <= size());
        return Span<T, Count>(storage_.data_ + (size() - Count), Count);
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    requires (Offset <= Extent && (Count == dynamic_extent || Count <= Extent - Offset))
    constexpr auto subspan() const noexcept {
        assert(Offset <= size() && (Count == dynamic_extent || Count <= size() - Offset));
        if constexpr (Count != dynamic_extent)
            return Span<T, Count>(storage_.data_ + Offset, Count);
        else if constexpr (Extent != dynamic_extent) 
            return Span<T, Extent - Offset>(storage_.data_ + Offset, Extent - Offset);
        else
            return Span<T, dynamic_extent>(storage_.data_ + Offset, size() - Offset);
    }

    constexpr Span<T, dynamic_extent> first(std::size_t count) const noexcept {
        assert(count <= size());
        return Span<T, dynamic_extent>(storage_.data_, count);
    }

    constexpr Span<T, dynamic_extent> last(std::size_t count) const noexcept {
        assert(count <= size());
        return Span<T, dynamic_extent>(storage_.data_ + (size() - count), count);
    }

    constexpr Span<T, dynamic_extent> subspan(std::size_t offset, std::size_t count = dynamic_extent) const noexcept {
        assert(offset <= size() && (count == dynamic_extent || count <= size() - offset));
        if (count != dynamic_extent)
            return Span<T, dynamic_extent>(storage_.data_ + offset, count);
        else
            return Span<T, dynamic_extent>(storage_.data_ + offset, size() - offset);
    }
};

}
