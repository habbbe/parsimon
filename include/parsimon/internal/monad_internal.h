#ifndef MONAD_INTERNAL_H
#define MONAD_INTERNAL_H

#include <utility>

namespace parsimon::internal {

/**
 * Currying of an arbitrary function
 */
template <std::size_t num_args, typename F>
inline constexpr auto curry_n(F f) {
    if constexpr (num_args > 1) {
        return [=](auto&& v) {
            return curry_n<num_args-1>([&](auto&&...vs) {
                return f(std::forward<decltype(v)>(v), std::forward<decltype(vs)>(vs)...);
            });
        };
    } else {
        return f;
    }
}

/**
 * Recursive lifting of functions
 */
template <typename F, typename Parser, typename... Ps>
inline constexpr auto lift_internal(F f, Parser p, Ps... ps) {
    return p >>= [=](auto&& r) {
        if constexpr (sizeof...(ps) == 0) {
            return f(std::forward<decltype(r)>(r));
        } else {
            return lift_internal(f(std::forward<decltype(r)>(r)), ps...);
        }
    };
}

/**
 * Intermediate step for lifting
 */
template <typename F, typename... Parsers>
inline constexpr auto lift_prepare(F f, Parsers... ps) {
    return lift_internal(curry_n<sizeof...(Parsers)>(f), ps...);
}

}

#endif // MONAD_INTERNAL_H
