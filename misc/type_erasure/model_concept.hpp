#pragma once

#include <memory>
#include <utility>
#include <type_traits>
#include <concepts>

namespace my_std {

template <typename T>
concept HasArea = requires(const T& t) {
    { t.area() } -> std::convertible_to<double>;
};

class Shape {
private:
    struct Concept {
        virtual ~Concept() = default;
        virtual double area() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    template <typename T>
    struct Model : Concept {
        T obj;
        Model(T obj) : obj(std::move(obj)) {}
        double area() const override { return obj.area(); }
        std::unique_ptr<Concept> clone() const { return std::make_unique(*this); }
    };

    std::unique_ptr<Concept> ptr_;

public:
    template <typename T>
    requires (HasArea<T> && !std::same_as<std::remove_cvref_t<T>, Shape>)
    explicit Shape(T obj) : ptr_(std::make_unique<Model<T>>(std::move(obj))) {}

    Shape(const Shape& other) : ptr_(other.ptr_->clone()) {}

    Shape(Shape&& other) noexcept = default;

    ~Shape() = default;

    Shape& operator=(const Shape& other) { return *this = Shape(other); }

    Shape& operator=(Shape&& other) noexcept = default;

    double area() const { return ptr_->area(); }
};

};