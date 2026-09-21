#pragma once

namespace my_std {

template <typename T>
struct is_lvalue_reference {
    static constexpr bool value = false;
};

template <typename T>
struct is_lvalue_reference<T&> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
struct remove_reference {
    using type = T;
};

template <typename T>
struct remove_reference<T&> {
    using type = T;
};

template <typename T>
struct remove_reference<T&&> {
    using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T>
constexpr remove_reference_t<T>&& move(T&& o) noexcept {
    using Type = remove_reference_t<T>;
    return static_cast<Type&&>(o);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>& o) noexcept {
    return static_cast<T&&>(o);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& o) noexcept {
    static_assert(!is_lvalue_reference_v<T>);
    return static_cast<T&&>(o);
}

}
