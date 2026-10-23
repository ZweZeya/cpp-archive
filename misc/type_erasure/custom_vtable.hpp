#pragma once

#include <utility>
#include <type_traits>

namespace my_std {

struct vtable {
    double (*area) (const void*);    
    void* (*clone) (const void*);    
    void (*destroy) (void*);    
};

template <typename T>
vtable vtable_for = {
    [](const void* p) -> double { return static_cast<const T*>(p)->area(); },
    [](const void* p) -> void* { return new T(*static_cast<const T*>(p)); },
    [](void* p) { delete static_cast<T*>(p); }
};

class Shape {
private:
    vtable* vt_;
    void* ptr_ = nullptr;

public:
    template <typename T, typename U = std::remove_cvref_t<T>>
    requires (!std::same_as<U, Shape>)
    explicit Shape(T&& obj) 
        : vt_(&vtable_for<U>), ptr_(new U(std::forward<T>(obj))) {}

    Shape(const Shape& other) 
        : vt_(other.vt_), ptr_(other.vt_->clone(other.ptr_)) {}

    Shape(Shape&& other) noexcept
        : vt_(other.vt_), ptr_(other.ptr_) 
    {
        other.ptr_ = nullptr;
    }

    ~Shape() { vt_->destroy(ptr_); }

    Shape& operator=(const Shape& other) {
        if (this == &other) return *this;
        
        void* new_ptr = other.vt_->clone(other.ptr_);

        vt_->destroy(ptr_);

        vt_ = other.vt_;
        ptr_ = new_ptr;

        return *this;
    }

    Shape& operator=(Shape&& other) noexcept {
        if (this == &other) return *this;

        void* old_ptr = ptr_;
        vtable* old_vt = vt_;

        vt_ = other.vt_;
        ptr_ = other.ptr_;

        other.ptr_ = nullptr;

        old_vt->destroy(old_ptr);

        return *this;
    }

    double area() const { return vt_->area(ptr_); }
};

}