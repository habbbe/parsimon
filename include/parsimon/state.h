#ifndef PARSIMON_STATE_H
#define PARSIMON_STATE_H

#include <iterator>
#include "parsimon/result.h"
#include "parsimon/internal/algorithm.h"

namespace parsimon {

/**
 * Class for the parser state.
 */
template <typename Iterator, typename Settings>
struct parser_state_simple {

    /// The current position of the parser
    Iterator position;

    /// The end of the range to be parsed
    const Iterator end;

    using settings = Settings;
    constexpr static bool error_messages = Settings::error_messages;
    using error_type = std::conditional_t<error_messages, const char*, void>;

    using default_result_type = decltype(settings::conversion_function(std::declval<Iterator>(), std::declval<Iterator>()));

    constexpr parser_state_simple(Iterator begin, Iterator end, Settings) :
        position{begin}, end{end} {}

    template<typename State>
    constexpr parser_state_simple(Iterator begin, Iterator end, const State&) :
        parser_state_simple(begin, end, typename State::settings()){}


    constexpr auto has_at_least(size_t n) const {
        return algorithm::contains_elements(position, end, n);
    }

    constexpr bool empty() const {return position == end;}
    constexpr const auto& get_at(const Iterator& it) const {return *it;}
    constexpr const auto& front() const {return get_at(position);}

    constexpr static auto convert(Iterator begin, Iterator end) {return settings::conversion_function(begin, end);}
    constexpr static auto convert(Iterator begin, size_t size) {return convert(begin, std::next(begin, size));}

    constexpr auto convert(Iterator end) const {return convert(position, end);}
    constexpr auto convert(size_t size) const {return convert(std::next(position, size));}
    constexpr void set_position(Iterator p) {position = p;}
    constexpr void advance(size_t n) {std::advance(position, n);}

    // Convenience function for returning a succesful parse.
    template <typename Res, typename... Args>
    constexpr static auto return_success_emplace(Args&&... args) {
        using T = std::decay_t<Res>;
        if constexpr (error_messages) {
            return result<T, default_error_type>(std::in_place_index<1>, std::forward<Args>(args)...);
        } else {
            return result<T, void>(std::in_place, std::forward<Args>(args)...);
        }
    }

    // Convenience function for returning a succesful parse.
    template <typename Res>
    constexpr static auto return_success(Res&& res) {
        return return_success_emplace<Res>(std::forward<Res>(res));
    }

    // Convenience function for returning a failed parse with state and type of result.
    template <typename Res, typename Error>
    constexpr static auto return_fail_error(Error&& error) {
        using T = std::decay_t<Res>;
        if constexpr (error_messages) {
            return result<T, std::decay_t<Error>>(std::in_place_index<0>, std::forward<Error>(error));
        } else {
            return result<T, void>();
        }
    }

    template <typename Error>
    constexpr static auto return_fail_error_default(Error&& error) { return return_fail_error<default_result_type>(std::forward<Error>(error)); }

    template <typename Res, typename Res2, typename Error>
    constexpr static auto return_fail_change_result(const result<Res2, Error>& res) {
        using T = std::decay_t<Res>;
        if constexpr (error_messages) {
            return result<T, Error>(std::in_place_index<0>, res.error());
        } else {
            return result<T, void>();
        }
    }

    template <typename Res, typename Error>
    constexpr static auto return_fail_result_default(const result<Res, Error>& res) {
        return return_fail_change_result<default_result_type>(res);
    }

    template <typename Res, typename Error>
    constexpr static auto return_fail_result(const result<Res, Error>& res) {
        return return_fail_change_result<Res>(res);
    }

    template <typename Res>
    constexpr static auto return_fail() {
        return return_fail_error<Res>("Parsing error");
    }

    constexpr static auto return_fail()  { return return_fail<default_result_type>(); }
};

/**
 * Class for the parser state. Contains the user provided state
 */
template <typename Iterator, typename Settings, typename UserState>
struct parser_state: public parser_state_simple<Iterator, Settings> {

    /// The user provided state
    UserState user_state;

    constexpr parser_state(Iterator begin, Iterator end, UserState&& state, Settings settings)
        : parser_state_simple<Iterator, Settings>{begin, end, settings},
          user_state{std::forward<UserState>(state)} {}

    constexpr parser_state(Iterator begin, Iterator end, parser_state& other)
        : parser_state_simple<Iterator, Settings>{begin, end, typename parser_state::settings()},
          user_state{other.user_state} {}
};
template <typename Iterator, typename Settings, typename UserState>
parser_state(Iterator, Iterator, UserState&&, Settings) ->
parser_state<Iterator, Settings, UserState>;

}

#endif // PARSIMON_STATE_H
