#ifndef CALC_H
#define CALC_H

#include "parsimon/parsimon.h"

using namespace parsimon;

template <typename T>
constexpr auto const_pow(T a, T b) {
    T result = 1;
    for (int i = 0; i<b; ++i) result *= a;
    return result;
}

constexpr auto expr = recursive<int>([](auto p) {
    constexpr auto ops = [](auto c) {
        return [c](auto a, auto b) {
            switch (c) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return a / b;
            case '^': return const_pow(a, b);
            default: return 0; // This can't happen
            }
        };
    };

    constexpr auto addOp = lift(ops, item<'+'>() || item<'-'>());
    constexpr auto mulOp = lift(ops, item<'*'>() || item<'/'>());
    constexpr auto expOp = lift(ops, item<'^'>());

    auto atom = integer() || (item<'('>() >> p << item<')'>());
    auto exp = chain(atom, expOp);
    auto factor = chain(exp, mulOp);
    return chain(factor, addOp);
});

#endif //CALC_H
