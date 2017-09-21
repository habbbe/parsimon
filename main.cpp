#include <iostream>
#include <vector>
#include <fstream>
#include <streambuf>
#include <memory>
#include <variant>
#include "parser.h"

using namespace std;

struct row {
    row(std::string_view firstName, std::string_view lastName) : firstName{firstName}, lastName{lastName} {}

    std::string firstName;
    std::string lastName;
};

using command = std::pair<std::string, std::string>;

struct action {
    command com;
    action(command com) : com{com} {}
};

struct info {
    command com;
    info(command com) : com{com} {}
};

struct separator {};
struct space {};
struct syntax_error {
    syntax_error(std::string_view s) : description{s}{}
    std::string description;
};

using item = std::variant<
action,
info,
separator,
space,
syntax_error
>;

int main()
{
//    constexpr auto add_to_state = [] (auto &s, auto&&... args) {
//        s.emplace_back(args()...);
//        return true;
//    };
//    constexpr auto parse_name = parse::until_token('=');
//    constexpr auto parse_cmd = parse::not_empty(parse::rest());
//    constexpr auto parse_command = monad::lift_value_lazy_raw<command>(parse_name, parse_cmd);
//    constexpr auto parse_action = parse::try_parser(parse::string("Com:") >> monad::lift_value_lazy<action>(parse_command));
//    constexpr auto parse_info = parse::try_parser(parse::string("Info:") >> monad::lift_value_lazy<info>(parse_command));
//    constexpr auto parse_separator = parse::string("Separator") >> parse::empty() >> parse::mreturn(lazy::make_lazy_value_forward<separator>());
//    constexpr auto parse_space = parse::string("Space") >> parse::empty() >> parse::mreturn(lazy::make_lazy_value_forward<space>());
//    constexpr auto parse_error = monad::lift_value_lazy_raw<syntax_error>(parse::rest());
//    constexpr auto entry_parser = parse::lift_or_state(add_to_state, parse_action, parse_info, parse_separator, parse_space, parse_error);
//    std::vector<item> r;
//    std::ifstream t("hub");

    constexpr auto add_to_state = [] (auto &s, auto&&... args) {
        s.emplace_back(args...);
        return true;
    };
    constexpr auto entry_parser = parse::string("Entry:") >> parse::apply_to_state(add_to_state, parse::until_token(':'), parse::until_token(';'));
    std::vector<row> r;
    std::ifstream t("test");

    std::string line;
    while (std::getline(t, line)) {
        entry_parser.parse_with_state(std::string_view(line), r);
//        auto result = entry_parser.parse(std::string_view(line));
//        if (result.second) {
//            r.emplace_back(*result.second);
//        }
    }

    cout << "Size: " << r.size() << endl;

    return 0;
}
