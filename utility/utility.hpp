#pragma once

namespace my_std {

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
remove_reference_t<T>&& move(T&& o) {
    using Type = remove_reference_t<T>;
    return static_cast<Type>(o);
}

template <typename T>
T&& forward(remove_reference_t<T>& o) {
    return static_cast<T&&>(o);
}

template <typename T>
T&& forward(remove_reference_t<T>&& o) {
    return static_cast<T&&>(o);
}

}