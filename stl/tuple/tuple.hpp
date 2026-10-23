#pragma once

#include <cstddef>
#include <utility>

namespace my_std {

template <std::size_t I, typename T>
struct tuple_leaf { T value; };

template <typename Seq, typename... Ts>
struct tuple_impl;

template <std::size_t... Is, typename... Ts>
struct tuple_impl<std::index_sequence<Is...>, Ts...> : tuple_leaf<Is, Ts>... {};

template <typename... Ts>
struct tuple : tuple_impl<std::make_index_sequence<sizeof...(Ts)>, Ts...> {};

template <std::size_t I, typename... Ts>
struct tuple_element;

template <typename Head, typename... Tail>
struct tuple_element<0, Head, Tail...> { using type = Head; };

template <std::size_t I, typename Head, typename... Tail>
struct tuple_element<I, Head, Tail...> { 
    using type = typename tuple_element<I - 1, Tail...>::type; 
};

template <std::size_t I, typename T>
T& get_value(const tuple_leaf<I, T>& leaf) { return leaf.value; }

template <std::size_t I, typename... Ts>
typename tuple_element<I, Ts...>::type get(const tuple<Ts...>& t) {
    return get_value<I>(t);
}

}