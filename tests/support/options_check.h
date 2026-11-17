#pragma once

// Compile-time check of a widget options struct against rule 4 of docs/api-conventions.md:
// an aggregate with a default for every member, and the common block members (size, width,
// height, readOnly, loading, mixed, error, helpText) with their names and types unchanged and
// in this order wherever they appear. Use it once per options struct in the family's API test:
//
//     static_assert(facet::test::OptionsFollowConventions<facet::SliderOptions>());
//
// Members outside the common block are not checked here; reviews check that they come first.

#include <cstddef>
#include <type_traits>

#include "facet/widgets/common.h"

namespace facet::test
{

namespace detail
{

// Tracks the offsets of the common members seen so far; each must come after the previous.
struct Order
{
    std::size_t last = 0;
    bool seen = false;
    bool ok = true;

    constexpr void Next(std::size_t offset)
    {
        if (seen && offset <= last)
            ok = false;
        last = offset;
        seen = true;
    }
};

} // namespace detail

template <class T>
consteval bool OptionsFollowConventions()
{
    if constexpr (!std::is_aggregate_v<T> || !std::is_default_constructible_v<T>)
    {
        return false;
    }
    else
    {
        detail::Order order;
        bool types = true;
        if constexpr (requires { &T::size; })
        {
            types = types && std::is_same_v<decltype(T::size), ControlSize>;
            order.Next(offsetof(T, size));
        }
        if constexpr (requires { &T::width; })
        {
            types = types && std::is_same_v<decltype(T::width), float>;
            order.Next(offsetof(T, width));
        }
        if constexpr (requires { &T::height; })
        {
            types = types && std::is_same_v<decltype(T::height), float>;
            order.Next(offsetof(T, height));
        }
        if constexpr (requires { &T::readOnly; })
        {
            types = types && std::is_same_v<decltype(T::readOnly), bool>;
            order.Next(offsetof(T, readOnly));
        }
        if constexpr (requires { &T::loading; })
        {
            types = types && std::is_same_v<decltype(T::loading), bool>;
            order.Next(offsetof(T, loading));
        }
        if constexpr (requires { &T::mixed; })
        {
            types = types && std::is_same_v<decltype(T::mixed), bool>;
            order.Next(offsetof(T, mixed));
        }
        if constexpr (requires { &T::error; })
        {
            types = types && std::is_same_v<decltype(T::error), const char*>;
            order.Next(offsetof(T, error));
        }
        if constexpr (requires { &T::helpText; })
        {
            types = types && std::is_same_v<decltype(T::helpText), const char*>;
            order.Next(offsetof(T, helpText));
        }
        return types && order.ok;
    }
}

} // namespace facet::test
