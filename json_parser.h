#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <variant>
#include <map>
#include <functional>
#include "parser.h"
#include "parser_state.h"

struct json_value;

using json_object = std::map<std::string, json_value>;
using json_object_pair = std::pair<std::string, json_value>;

using json_array = std::vector<json_value>;

struct json_value {
    std::variant<
    std::nullptr_t,
    bool,
    std::string,
    int,
//    double,
    json_object,
    json_array
    > val;
    template <typename T>
    json_value(T &&t) : val(std::forward<T>(t)) {}
    auto &operator*() {return val;}
};

template <typename Parser>
constexpr auto eat(Parser p) {
    return parse::whitespace() >> p;
}

constexpr auto string_parser = eat(monad::lift_value<std::string>(parse::between_items('"', '"')));
constexpr auto integer_parser = eat(parse::integer());
//constexpr auto double_parser = eat(parse::number<double>());
constexpr auto bool_parser = eat((parse::sequence("true") >= true) || (parse::sequence("false") >= false));
constexpr auto null_parser = eat(parse::sequence("null") >= nullptr);

//template <typename Iterator>
//parse::type<json_value, void, void, Iterator> value_parser() {
//auto array_parser = eat(parse::item('[') >> parse::many_to_vector(eat(value_parser<Iterator>), parse::item(',')) << parse::item(']'));
//    return monad::lift_value<json_value>(parse::first(string_parser, integer_parser,
//                        bool_parser, null_parser,
//                        array_parser));
//}

constexpr auto value_parser = parse::parser([](auto &s) {
    auto rec = [&s](auto self) {
        auto res = parse::apply(parse::lift_or_value<json_value>(null_parser, bool_parser, string_parser), s);

        auto return_it = [](auto r) {
            return parse::parser([=](auto &) {
                return r;
            });
        };
        if (res) {
            return return_it(res);
        }

        auto p = parse::item('[') >>= [=](auto &) {
            return monad::lift_value<json_value>(parse::many_to_vector(self(self), parse::item(','))) << parse::item(']');
        };
        res = apply(p, s);
        return return_it(res);
    };
    return apply(rec(rec), s);
});

template <typename Iterator>
auto parse_json(Iterator begin, Iterator end) {
    auto p = value_parser;
    return p.parse(begin, end);
}

auto value_parser2 = parse::recursive<json_value, void>([](auto parser) {
        auto array_parser = eat(parse::item('[') >> parse::many_to_vector(eat(parser), parse::item(',')) << parse::item(']'));
        return monad::lift_value<json_value>(parse::first(string_parser, integer_parser,
                                                          bool_parser, null_parser,
                                                          array_parser));
});
//    parse::recurse<int, void>([](auto r) {
//        return parse::integer() || (parse::item('#') >> r);
//    });

//template <typename T>
//auto get_pair_parser() {
//    monad::lift_value<std::pair<std::string, json_value>>(string_parser, eat(parse::item(':') >> eat(value_parser)));
//}

//template <typename T>
//auto get_object_parser() {
//    return parse::token('{') >> parse::many_to_unordered_map<json_array>(eat(parse::succeed(parse::token(','))) >> get_value_parser()) << eat(parse::token('}'));
//}


//template <typename T>

//auto array_parser = parse::token('[') >>
//                    parse::many<json_array>(eat(parse::succeed(parse::token(','))) >> value_parser) << eat(parse::token(']'));


//constexpr auto object_map_parser = basic_json_parser(monad::lift<json_object_pair>(string_parser, ));
//constexpr auto object_parser = basic_json_parser(monad   parse::between_tokens('{', '}'));

//template <typename StringType>
//inline constexpr auto parse_json(StringType &s) {

//}

#endif // JSON_PARSER_H
