#ifndef PARSER_COMBINATORS_H
#define PARSER_COMBINATORS_H

#include <type_traits>
#include <unordered_map>
#include <vector>
#include "parser_core.h"
#include "monad.h"

namespace parse {

/**
 * Transform a parser to a parser that always succeeds.
 * Will return true as result if the parser succeeded, and false otherwise.
 * The result of the parse will be ignored.
 */
template <typename Parser>
inline constexpr auto succeed(Parser p) {
    return parser([=](auto &s) {
        if (has_result(apply(p, s))) {
            return return_success(true);
        } else {
            return return_success(false);
        }
    });
}

/**
 * Change the error to be returned upon a failed parse with the provided parser
 */
template <typename Parser>
inline constexpr auto change_error(Parser p, const char *error) {
    return parser([=](auto &s) {
        if (auto result = apply(p, s); has_result(result)) {
            return result;
        } else {
            using return_type = std::decay_t<decltype(get_result(result))>;
            return return_fail<return_type>(error);
        }
    });
}

/**
 * Transform a parser to a parser that fails on a successful, but empty result (as decided by std::empty
 * or == 0 if integral)
 */
template <typename Parser>
inline constexpr auto not_empty(Parser p) {
    return parser([=](auto &s) {
        using result_type = std::decay_t<decltype(get_result(apply(p, s)))>;
        constexpr auto empty = [](auto &t) {
            if constexpr (std::is_integral<result_type>::value) {
                return t == 0;
            } else {
                return std::empty(t);
            }
        };
        if (auto result = apply(p, s); has_result(result) && !empty(get_result(result))) {
            return result;
        } else {
            using return_type = std::decay_t<decltype(get_result(result))>;
            return return_fail<return_type>();
        }
    });
}

/**
 * Make a parser that doesn't consume its input on failure
 */
template <typename Parser>
inline constexpr auto try_parser(Parser p) {
    return parser([=](auto &s) {
        auto old_position = s.position;
        auto result = apply(p, s);
        if (!has_result(result)) {
            s.position = old_position;
        }
        return result;
    });
}

/**
 * Make a parser non consuming
 */
template <typename Parser>
inline constexpr auto no_consume(Parser p) {
    return parser([=](auto &s) {
        auto old_position = s.position;
        auto result = apply(p, s);
        s.position = old_position;
        return result;
    });
}

/**
 * Constrain a parser.
 * Takes in addition to a parser a predicate that takes the resulting type of the parser as argument.
 */
template <typename Parser, typename Predicate>
inline constexpr auto constrain(Parser p, Predicate pred) {
    return parser([=](auto &s) {
        auto result = apply(p, s);
        if (has_result(result) && pred(get_result(result))) {
            return result;
        } else {
            return return_fail<decltype(get_result(result))>();
        }
    });
}

/**
 * Recursive helper for `get_parsed`
 */
template <typename State, typename Parser, typename... Parsers>
static inline constexpr auto get_parsed_recursive(State &s, size_t original_position, Parser p, Parsers... ps) {
    if (const auto &res = apply(p, s); has_result(res)) {
        if constexpr (sizeof...(Parsers) == 0) {
            size_t size = s.position - original_position;
            return return_success(s.substr(original_position, size));
        } else {
            return get_parsed_recursive(s, original_position, ps...);
        }
    } else {
        return return_fail<std::string_view>(get_error(res));
    }
}

/**
 * Make a parser that evaluates an arbitrary number of parsers, and returns the successfully parsed text upon
 * success.
 */
template <typename Parser, typename ... Parsers>
inline constexpr auto get_parsed(Parser p, Parsers ... ps) {
    return parser([=](auto &s) {
        auto original_position = s.position;
        return get_parsed_recursive(s, original_position, p, ps...);
    });
}

/**
 * Make a parser that evaluates two parsers, and returns the successfully parsed text upon success.
 */
template <typename Parser1, typename Parser2>
inline constexpr auto operator+(Parser1 p1, Parser2 p2) {
    return get_parsed(p1, p2);
}

/**
 * Combine two parsers so that the second will be tried before failing.
 * If the two parsers return different types the return value will instead be `true`.
 */
template <typename Parser1, typename Parser2>
inline constexpr auto operator||(Parser1 p1, Parser2 p2) {
    return parser([=](auto &s) {
        using R1 = decltype(get_result(apply(p1, s)));
        using R2 = decltype(get_result(apply(p2, s)));
        auto original_position = s.position;
        if constexpr (std::is_same_v<R1, R2>) {
            if (auto result1 = apply(p1, s); has_result(result1)) {
                return result1;
            } else {
                s.position = original_position;
                return apply(p2, s);
            }
        } else {
            if (has_result(apply(p1, s))) {
                return return_success(true);
            } else {
                s.position = original_position;
                return has_result(apply(p2, s)) ? return_success(true) : return_fail<bool>();
            }
        }
    });
}

/**
 * Modify the supplied user state.
 * Will use the returned value from the user supplied function as result,
 * or `true` if return type is `void`.
 */
template <typename Fun>
inline constexpr auto modify_state(Fun f) {
    return parser([=](auto &s) {
        using result_type = std::decay_t<decltype(f(s.user_state))>;
        if constexpr (std::is_void<result_type>::value) {
            f(s.user_state);
            return return_success(true);
        } else {
            return_success(f(s.user_state));
        }
    });
}

/**
 * Set a value in the state accessed by `Accessor` from the result of the parser.
 * Note: Make sure that a reference is returned by `acc`.
 */
template <typename Parser, typename Accessor>
inline constexpr auto set_in_state(Parser p, Accessor acc) {
    return parser([=](auto &s) {
        auto result = apply(p, s);
        if (has_result(result)) {
            acc(s.user_state) = get_result(result);
        }
        return result;
    });
}

/**
 * Apply a provided function to the state after evaluating a number of parsers in sequence.
 * The function provided shall have the state as its first argument (by reference) and
 * then a number of arguments that matches the number of parsers (or variadic arguments)
 */
template <typename Fun, typename... Parsers>
inline constexpr auto apply_to_state(Fun f, Parsers...ps) {
    return parser([=](auto &s) {
        auto &state = s.user_state;
        auto to_apply = [f, &state] (auto&&...vals) {
            return f(state, std::forward<decltype(vals)>(vals)...);
        };
        return monad::lift(to_apply, ps...)(s);
    });
}

/**
 * Emplace a value in the state with `emplace` with the results of a number of parsers evaluated in sequence
 * to the user supplied state accessed by `acc`.
 */
template <typename Accessor, typename... Parsers>
inline constexpr auto emplace_to_state(Accessor acc, Parsers... ps) {
    return apply_to_state([acc](auto &s, auto&&...args) {return acc(s).emplace(std::forward<decltype(args)>(args)...);}, ps...);
}

/**
 * Emplace a value in the state with `emplace` with the results of a number of parsers evaluated in sequence
 * to the user supplied state.
 */
template <typename... Parsers>
inline constexpr auto emplace_to_state_direct(Parsers... ps) {
    return emplace_to_state([](auto &s) -> auto& {return s;}, ps...);
}

/**
 * Emplace a value in the state with `emplace_back` with the results of a number of parsers evaluated in sequence
 * to the user supplied state accessed by `acc`.
 */
template <typename Accessor, typename... Parsers>
inline constexpr auto emplace_back_to_state(Accessor acc, Parsers... ps) {
    return apply_to_state([acc](auto &s, auto&&...args) {return acc(s).emplace_back(std::forward<decltype(args)>(args)...);}, ps...);
}

/**
 * Emplace a value in the state with `emplace_back` with the results of a number of parsers evaluated in sequence
 * to the user supplied state.
 */
template <typename... Parsers>
inline constexpr auto emplace_back_to_state_direct(Parsers... ps) {
    return emplace_back_to_state([](auto &s) -> auto& {return s;}, ps...);
}

/**
 * General helper for functions that saves the results in collections.
 */
template <typename Container, typename State, typename Inserter, typename Parser>
inline constexpr auto many_internal(State &s, Inserter insert, Parser p) {
    Container c;
    for (auto result = apply(p, s); has_result(result); result = apply(p, s)) {
        insert(c, std::move(get_result(result)));
    }
    return return_success(c);
}

/**
 * Create a parser that applies a parser until it fails and returns the result in a vector.
 */
template <typename Parser>
inline constexpr auto many_to_vector(Parser p) {
    return parser([=](auto &s) {
        using result_type = std::decay_t<decltype(get_result(apply(p, s)))>;
        return many_internal<std::vector<result_type>>(s, [](auto &v, auto &&r){
            v.emplace_back(std::forward<decltype(r)>(r));
        }, p);
    });
}

/**
 * Create a parser that applies a parser until it fails and returns the result in a vector.
 */
template <typename Parser>
inline constexpr auto many_to_unordered_map(Parser p) {
    return parser([=](auto &s) {
        using result_type = std::decay_t<decltype(get_result(apply(p, s)))>;
        using key = typename result_type::first_type;
        using value = typename result_type::second_type;
        return many_internal<std::unordered_map<key, value>>(s, [](auto &m, auto &&r) {
            m.insert(std::forward<decltype(r)>(r));
        }, p);
    });
}

/**
 * Create a parser that applies a parser multiple times and returns the number it succeeds.
 * Useful when applying parses to state.
 */
template <typename Parser>
inline constexpr auto many_simple(Parser p) {
    return parser([=](auto &s) {
        size_t successes = 0;
        while (has_result(apply(p, s))) { ++successes; }
        return return_success(successes);
    });
}

/**
 * Create a parser that applies a parser until it fails and stores the result in
 * a container in the user provided state which is accessed by the function `acc`.
 * The number of added elements is then returned.
 * Note that this parser always succeeds, so you need to check the resulting number whether a
 * parse succeeded or not (or use parse::not_empty).
 * Note: The container in the state must have an `emplace_back` function.
 */
template <typename Accessor, typename Parser>
inline constexpr auto many_to_state(Accessor acc, Parser p) {
    return parser([=](auto &s) {
        size_t number_of_results = 0;
        auto &c = acc(s.user_state);
        for (auto result = apply(p, s); has_result(result); result = apply(p, s)) {
            c.emplace_back(std::move(get_result(result)));
            ++number_of_results;
        }
        return return_success(number_of_results);
    });
}

/**
 * Create a parser that applies a parser until it fails and stores the result in the user
 * provided state.
 * Note that the user provided state needs to be a container.
 */
template <typename Parser>
inline constexpr auto many_to_state(Parser p) {
    return many_to_state([](auto &s) -> auto& {return s;}, p);
}

// Compile time recursive resolver for lifting of arbitrary number of parsers
template <typename State, typename F, typename Parser, typename... Parsers>
static inline constexpr auto lift_or_rec(State &s, F f, Parser p, Parsers... ps) {
    if (auto result = apply(p, s); has_result(result)) {
        return return_success(f(get_result(result)));
    } else if constexpr (sizeof...(ps) > 0) {
        return lift_or_rec(s, f, ps...);
    } else {
        // All parsers failed
        using result_type = std::decay_t<decltype(f(get_result(result)))>;
        return return_fail<result_type>();
    }
}

/**
 * Lift the result of a unary functor to the parser monad after applying it to the first successful
 * parser's result.
 * The lifted functor must provide an overload for every parser result type.
 */
template <typename F, typename Parser, typename... Parsers>
inline constexpr auto lift_or(F f, Parser p, Parsers... ps) {
    return parser([=](auto &s) {
        return lift_or_rec(s, f, p, ps...);
    });
}

/**
 * Lift a type to the parser monad after applying the first successful parser's result to its constructor.
 * The constructor must provide an overload for every parser result type.
 */
template <typename Fun, typename Parser, typename... Parsers>
inline constexpr auto lift_or_state(Fun f, Parser p, Parsers... ps) {
    return parser([=](auto &s) {
        auto to_apply = [f, &s] (auto &&val) {
             return f(s.user_state, std::forward<decltype(val)>(val));
        };
        return lift_or_rec(s, to_apply, p, ps...);
    });
}

/**
 * Lift a type to the parser monad after applying the first successful parser's result to its constructor.
 * The constructor must provide an overload for every parser result type.
 */
template <typename T, typename Parser, typename... Parsers>
inline constexpr auto lift_or_value(Parser p, Parsers... ps) {
    return parser([=](auto &s) {
        constexpr auto construct = [](auto&& arg) {return T(std::forward<decltype(arg)>(arg));};
        return lift_or_rec(s, construct, p, ps...);
    });
}

/**
 * Lift a type to the parser monad after applying the first successful parser's result to its constructor.
 * The constructor must provide an overload for every parser result type.
 * This version applies the constructor to a lazy argument
 */
template <typename T, typename Parser, typename... Parsers>
inline constexpr auto lift_or_value_from_lazy(Parser p, Parsers... ps) {
    return parser([=](auto &s) {
        constexpr auto construct = [](auto arg) {
            return T(arg());
        };
        return lift_or_rec(s, construct, p, ps...);
    });
}

/**
 * Create a parser that parses the result of `p1` with `p2`.
 * Useful for parsing the content between brackets.
 */
template <typename Parser1, typename Parser2>
inline constexpr auto parse_result(Parser1 p1, Parser2 p2) {
    return parser([=](auto &s) {
        auto result = apply(p1, s);
        if (has_result(result)) {
            auto result_text = get_result(result);
            using state_type = std::decay_t<decltype(s)>;
            state_type new_state(result_text, s);
            auto new_result = apply(p2, new_state);
            return new_result;
        } else {
            return return_fail<decltype(get_result(apply(p2, s)))>();
        }
    });
}

/**
 * Parse all text until the supplied parser succeeds.
 * Use template parameter `Eat` to specify whether to include the parsed string in
 * the result or not.
 * Note: For parsing until a certain string or token, use functions
 * `until_token` and `until_string` instead, as they are more efficient.
 */
template <bool Eat = true, typename Parser>
inline constexpr auto until(Parser p) {
    return parser([=](auto &s) {
        auto position_start = s.position;
        auto position_end = position_start;
        while (!has_result(apply(p, s))) {
            s.advance(1);
            position_end = s.position;
        }
        if constexpr (Eat) {
            return return_success(s.substr(position_start, position_end - position_start));
        } else {
            return return_success(s.substr(position_start, s.position - position_start));
        }
    });
}

}

#endif // PARSER_COMBINATORS_H
