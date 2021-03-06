#include <string_view>
#include <catch2/catch.hpp>
#include "anpa/parsers.h"

using namespace anpa;

TEST_CASE("success") {
    static_assert(success().parse("").second);
}

TEST_CASE("fail") {
    static_assert(!fail().parse("").second);
}

TEST_CASE("cond") {
    using namespace std::literals;
    constexpr auto number = integer() >>= [](auto i) {
        return cond(i <= 100)  >> seq("small") ||
               cond(i <= 1000) >> seq("big")   ||
               seq("very big");
    };
    static_assert(number.parse("1small").second.get_value() == "small");
    static_assert(number.parse("10small").second.get_value() == "small");
    static_assert(number.parse("500big").second.get_value() == "big");
    static_assert(number.parse("1001very big").second.get_value() == "very big");
    static_assert(number.parse("100100very big").second.get_value() == "very big");
}


TEST_CASE("empty") {
    static_assert(empty().parse("").second);
    static_assert(!empty().parse(" ").second);
}

TEST_CASE("any_item") {
    static_assert(*any_item().parse("a").second == 'a');
    static_assert(!any_item().parse("").second);
}

TEST_CASE("item") {
    static_assert(*item('a').parse("a").second == 'a');
    static_assert(!item('b').parse("a").second);
}

TEST_CASE("item_if") {
    constexpr std::string_view str("abc");
    constexpr auto p = item_if([](auto& c) {return c == 'a';});
    constexpr auto res = p.parse(str);
    static_assert(*res.second == 'a');
    static_assert(res.first.position == str.begin() + 1);
    constexpr std::string_view strFail("bbc");
    constexpr auto resFail = p.parse(strFail);
    static_assert(!resFail.second);
    static_assert(resFail.first.position == strFail.begin());
}

TEST_CASE("item_if_not") {
    constexpr std::string_view str("abc");
    constexpr auto p = item_if_not([](auto& c) {return c == 'b';});
    constexpr auto res = p.parse(str);
    static_assert(*res.second == 'a');
    static_assert(res.first.position == str.begin() + 1);
    constexpr std::string_view strFail("bbc");
    constexpr auto resFail = p.parse(strFail);
    static_assert(!resFail.second);
    static_assert(resFail.first.position == strFail.begin());
}

TEST_CASE("custom") {
    constexpr auto parser = [](auto begin, auto end) {
        using type = std::pair<std::decay_t<decltype(begin)>, std::optional<int>>;
        if (begin == end) {
            return type(end, {});
        } else if (*begin == 'a') {
            return type{++begin, 1};
        } else if (*begin == 'b') {
            return type{++begin, 2};
        } else {
            return type(begin, {});
        }
    };
    constexpr std::string_view strA("a");
    constexpr std::string_view strB("b");
    constexpr std::string_view strC("c");
    constexpr std::string_view strEmpty("");

    constexpr auto resA = custom(parser).parse(strA);
    constexpr auto resB = custom(parser).parse(strB);
    constexpr auto resC = custom(parser).parse(strC);
    constexpr auto resEmpty = custom(parser).parse(strEmpty);

    static_assert(*resA.second == 1);
    static_assert(resA.first.position == strA.begin() + 1);

    static_assert(*resB.second == 2);
    static_assert(resB.first.position == strB.begin() + 1);

    static_assert(!resC.second);
    static_assert(resC.first.position == strC.begin());

    static_assert(!resEmpty.second);
    static_assert(resEmpty.first.position == strEmpty.end());
}

TEST_CASE("custom_state") {
    constexpr auto parser = [](auto begin, auto, auto& state) {
        using type = std::pair<std::decay_t<decltype(begin)>, std::optional<int>>;
        state = 3;
        return type(begin, 3);
    };
    constexpr std::string_view str("a");
    constexpr auto res = custom_with_state(parser).parse_with_state(str, 0);
    static_assert(res.second);
    static_assert(*res.second == 3);
    static_assert(res.first.user_state == 3);
}

TEST_CASE("sequence") {
    constexpr std::string_view str("abcde");
    constexpr auto resSuccess = seq("abc").parse(str);
    static_assert(resSuccess.second);
    static_assert(*resSuccess.second == "abc");
    static_assert(resSuccess.first.position == str.begin() + 3);

    constexpr auto resPartialFail = seq("abce").parse(str);
    static_assert(!resPartialFail.second);
    static_assert(resPartialFail.first.position == str.begin());

    constexpr auto resTooLong = seq("abcdef").parse(str);
    static_assert(!resTooLong.second);
    static_assert(resTooLong.first.position == str.begin());

    constexpr auto resFail = seq("b").parse(str);
    static_assert(!resFail.second);
    static_assert(resFail.first.position == str.begin());
}

TEST_CASE("consume") {
    constexpr std::string_view str("abcde");
    constexpr auto resSuccess = consume(3).parse(str);
    static_assert(resSuccess.second);
    static_assert(*resSuccess.second == "abc");
    static_assert(resSuccess.first.position == str.begin() + 3);

    constexpr auto resFail = consume(6).parse(str);
    static_assert(!resFail.second);
    static_assert(resFail.first.position == str.begin());
}

TEST_CASE("until_item eat no include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_item('c').parse(str);
    static_assert(res.second);
    static_assert(*res.second == "ab");
    static_assert(res.first.position == str.begin() + 3);
}

TEST_CASE("until_item no eat no include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_item<options::dont_eat>('c').parse(str);
    static_assert(res.second);
    static_assert(*res.second == "ab");
    static_assert(res.first.position == str.begin() + 2);
}

TEST_CASE("until_item eat include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_item<options::include>('c').parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abc");
    static_assert(res.first.position == str.begin() + 3);
}

TEST_CASE("until_item no eat include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_item<options::include | options::dont_eat>('c').parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abc");
    static_assert(res.first.position == str.begin() + 2);
}

TEST_CASE("until_sequence eat no include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_seq("cd").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "ab");
    static_assert(res.first.position == str.begin() + 4);

    constexpr auto resFail = until_seq("cdf").parse(str);
    static_assert(!resFail.second);
    static_assert(resFail.first.position == str.begin());
}

TEST_CASE("until_sequence no eat no include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_seq<options::dont_eat>("cd").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "ab");
    static_assert(res.first.position == str.begin() + 2);
}

TEST_CASE("until_sequence eat include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_seq<options::include>("cd").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abcd");
    static_assert(res.first.position == str.begin() + 4);
}

TEST_CASE("until_sequence no eat include") {
    constexpr std::string_view str("abcde");
    constexpr auto res = until_seq<options::include | options::dont_eat>("cd").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abcd");
    static_assert(res.first.position == str.begin() + 2);
}

TEST_CASE("rest") {
    constexpr std::string_view str("abcde");
    constexpr auto res = rest().parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abcde");
    static_assert(res.first.position == str.end());

    constexpr std::string_view strEmpty;
    constexpr auto resEmpty = rest().parse(strEmpty);
    static_assert(resEmpty.second);
    static_assert(*resEmpty.second == "");
    static_assert(resEmpty.first.position == strEmpty.begin());
    static_assert(resEmpty.first.position == strEmpty.end());
}

TEST_CASE("while_if") {
    auto pred = [](auto& x) {
        return x == 'a' || x == 'b';
    };

    constexpr std::string_view str("aabbcc");
    constexpr auto res = while_if(pred).parse(str);
    static_assert(res.second);
    static_assert(*res.second == "aabb");
    static_assert(res.first.position == str.begin() + 4);

    constexpr std::string_view strNoMatch("cbbaa");
    constexpr auto resNoMatch = while_if(pred).parse(strNoMatch);
    static_assert(resNoMatch.second);
    static_assert(*resNoMatch.second == "");
    static_assert(resNoMatch.first.position == strNoMatch.begin());
}

TEST_CASE("while_in") {
    constexpr std::string_view str("aabbcc");
    constexpr auto res = while_in("abc").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "aabbcc");
    static_assert(res.first.position == str.end());

    constexpr auto resNoMatch = while_in("def").parse(str);
    static_assert(resNoMatch.second);
    static_assert(*resNoMatch.second == "");
    static_assert(resNoMatch.first.position == str.begin());
}

TEST_CASE("between_sequences") {
    constexpr std::string_view str("beginabcdeend");
    constexpr auto res = between_sequences("begin", "end").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abcde");
    static_assert(res.first.position == str.end());

    constexpr auto resInclude = between_sequences<options::include>("begin", "end").parse(str);
    static_assert(resInclude.second);
    static_assert(*resInclude.second == "beginabcdeend");
    static_assert(resInclude.first.position == str.end());
}

TEST_CASE("between_sequences nested") {
    constexpr std::string_view str("beginbeginabcdeendend");
    constexpr auto res = between_sequences<options::nested>("begin", "end").parse(str);
    static_assert(res.second);
    static_assert(*res.second == "beginabcdeend");
    static_assert(res.first.position == str.end());

    constexpr auto resInclude = between_sequences<options::nested | options::include>("begin", "end").parse(str);
    static_assert(resInclude.second);
    static_assert(*resInclude.second == "beginbeginabcdeendend");
    static_assert(resInclude.first.position == str.end());

    constexpr std::string_view strNonClosing("beginbeginabcdeend");
    constexpr auto resNonClosing = between_sequences<options::nested>("begin", "end").parse(strNonClosing);
    static_assert(!resNonClosing.second);
    static_assert(resNonClosing.first.position == strNonClosing.begin());
}

TEST_CASE("between_items") {
    constexpr std::string_view str("{abcde}");
    constexpr auto res = between_items('{', '}').parse(str);
    static_assert(res.second);
    static_assert(*res.second == "abcde");
    static_assert(res.first.position == str.end());
}

TEST_CASE("integer") {
    constexpr std::string_view str("42abcde");
    constexpr auto res = integer().parse(str);
    static_assert(res.second);
    static_assert(*res.second == 42);
    static_assert(res.first.position == str.begin() + 2);

    constexpr std::string_view str2("-42abcde");
    constexpr auto res2 = integer().parse(str2);
    static_assert(res2.second);
    static_assert(*res2.second == -42);
    static_assert(res2.first.position == str2.begin() + 3);

    constexpr std::string_view str3("-42abcde");
    constexpr auto res3 = integer<unsigned int>().parse(str3);
    static_assert(!res3.second);
    static_assert(res3.first.position == str3.begin());

    constexpr std::string_view str4("42abcde");
    constexpr auto res4 = integer<unsigned int>().parse(str4);
    static_assert(res4.second);
    static_assert(*res4.second == 42);
    static_assert(res4.first.position == str4.begin() + 2);

    constexpr std::string_view str5("-");
    constexpr auto res5 = integer<int>().parse(str5);
    static_assert(!res5.second);
    static_assert(res5.first.position == str5.begin());

    constexpr std::string_view str6("+");
    constexpr auto res6 = integer<int>().parse(str6);
    static_assert(!res6.second);
    static_assert(res6.first.position == str6.begin());

    constexpr std::string_view str7("01");
    constexpr auto res7 = integer<int>().parse(str7);
    static_assert(res7.second);
    static_assert(res7.first.position == str7.end());

    constexpr std::string_view str8("01");
    constexpr auto res8 = integer<int, options::no_leading_zero>().parse(str8);
    static_assert(!res8.second);
    static_assert(res8.first.position == str8.begin());

    constexpr std::string_view str9("01");
    constexpr auto res9 = integer<unsigned, options::no_leading_zero>().parse(str9);
    static_assert(!res9.second);
    static_assert(res9.first.position == str9.begin());

    constexpr std::string_view str10("0");
    constexpr auto res10 = integer<int, options::no_leading_zero>().parse(str10);
    static_assert(res10.second);
    static_assert(res10.first.position == str10.end());
}

TEST_CASE("floating") {
    constexpr auto p = floating();
#define floating_test_(s,b) { constexpr std::string_view str(s); \
        constexpr auto res = p.parse(str); \
        static_assert(res.second); \
        static_assert(*res.second == b); \
        static_assert(res.first.position == str.end());}
    floating_test_("123", 123.0);
    floating_test_("-123", -123.0);
    floating_test_("123.321", 123.321);
    floating_test_("-123.321", -123.321);
    floating_test_("123.0", 123.0);
    floating_test_("-123.0", -123.0);
    floating_test_("123e0", 123);
    floating_test_("123e1", 1230);
    floating_test_("123e3", 123e3);
    floating_test_("123e+3", 123e+3);
    floating_test_("-123e3", -123e3);
    floating_test_("123e-3", 123e-3);
    floating_test_("-123e-3", -123e-3);
    floating_test_("123.321e3", 123.321e3);
    floating_test_("-123.321e3", -123.321e3);
    floating_test_("123.321e-3", 123.321e-3);
    floating_test_("-123.321e-3", -123.321e-3);
}
