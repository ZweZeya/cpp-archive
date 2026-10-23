#pragma once

#include <memory>
#include <utility>
#include <cstddef>
#include <type_traits>
#include <new>
#include <stdexcept>


class Shape {
private:
    static constexpr std::size_t BUF_SIZE = 32;

    alignas(std::max_align_t) std::byte buf[BUF_SIZE];

    template <typename T>
    static constexpr bool fits = 
        sizeof(T) <= BUF_SIZE &&
        alignof(T) <= alignof(std::max_align_t) &&
        std::is_nothrow_move_constructible_v<T>;

    struct VTable {
        double (*area_fn)(const void*);
        void* (*clone_fn)(const void*, std::byte*);
        void* (*move_fn)(void*, std::byte*);
        void (*destroy_fn)(void*);
    };

    template <typename T>
    static constexpr VTable vtable_for = {
        [](const void* p){ return static_cast<const T*>(p)->area(); },
        [](const void* p, std::byte* buf) {
            if constexpr (fits<T>)
                return static_cast<void*>(new(buf) T(*static_cast<const T*>(p)));
            else
                return static_cast<void*>(new T(*static_cast<const T*>(p)));
        },
        [](void* p, std::byte* buf) {
            if constexpr (fits<T>) {
                T* obj = new(buf) T(std::move(*static_cast<T*>(p)));
                static_cast<T*>(p)->~T();
                return static_cast<void*>(obj);
            } else
                return p;
        },
        [](void* p){ 
            if constexpr (fits<T>)
                static_cast<T*>(p)->~T();
            else
                delete static_cast<T*>(p);
            
        }   
    };

    void* obj_ = nullptr;
    const VTable* vt_;

public:

    template <typename T, typename D = std::decay_t<T>>
    requires (!std::is_same_v<Shape, D>)
    explicit Shape(T&& t) : vt_(&vtable_for<D>) {
        if constexpr (fits<D>) {
            obj_ = new(&buf) D(std::forward<T>(t));
        } else {
            obj_ = new D(std::forward<T>(t));
        }
    }

    Shape(const Shape& other) : vt_(other.vt_) {
        if (other.obj_ != nullptr)
            obj_ = vt_->clone_fn(other.obj_, buf);
    }

    Shape(Shape&& other) noexcept : vt_(other.vt_) {
        if (other.obj_ != nullptr)
            obj_ = vt_->move_fn(other.obj_, buf);

        other.obj_ = nullptr;
    }

    Shape& operator=(const Shape& other) {
        if (this == &other) return *this;

        void* tmp_obj = nullptr;
        alignas(std::max_align_t) std::byte tmp_buf[BUF_SIZE];
        if (other.obj_ != nullptr) 
            tmp_obj = other.vt_->clone_fn(other.obj_, tmp_buf);

        if (obj_ != nullptr)
            vt_->destroy_fn(obj_);
        obj_ = nullptr;

        vt_ = other.vt_;
        if (tmp_obj != nullptr)
            obj_ = vt_->move_fn(tmp_obj, buf);

        return *this;
    }

    Shape& operator=(Shape&& other) noexcept {
        if (this == &other) return *this;

        if (obj_ != nullptr)
            vt_->destroy_fn(obj_);
        obj_ = nullptr;

        vt_ = other.vt_;

        if (other.obj_ != nullptr) 
            obj_ = vt_->move_fn(other.obj_, buf);

        other.obj_ = nullptr;

        return *this;
    }

    ~Shape() {
        if (obj_ != nullptr)
            vt_->destroy_fn(obj_);
    }

    explicit operator bool() const noexcept { return obj_ != nullptr; }

    double area() const { 
        if (obj_ == nullptr) 
            throw std::runtime_error("no object");

        return vt_->area_fn(obj_); 
    }
};
