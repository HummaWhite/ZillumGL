#pragma once

#include <iostream>

template<typename E>
struct EnableEnumBitMask
{
    static constexpr bool enable = false;
};

template<typename E>
typename std::enable_if<EnableEnumBitMask<E>::enable, E>::type
operator | (E lhs, E rhs)
{
    typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(
        static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}