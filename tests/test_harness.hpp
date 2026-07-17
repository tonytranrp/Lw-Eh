#pragma once
#ifndef LWEH_TEST_HARNESS_HPP_INCLUDED
#define LWEH_TEST_HARNESS_HPP_INCLUDED

// Minimal, zero-dependency, hand-rolled test harness for Lw-Eh's host
// correctness suite. Test-only code: unlike include/lweh/*, this file is
// never shipped as part of the library and is free to use ordinary hosted
// C++. CTest already supplies the runner/parallelism/pass-fail aggregation,
// so this only needs to contribute assertion macros + a file:line message
// (see Research/ARCHITECTURE.md "hand-rolled test harness, not Catch2/
// GoogleTest/doctest" decision).

#include <cstdio>
#include <cstdlib>

namespace lweh_test {

inline int& failure_count() {
    static int count = 0;
    return count;
}

inline void fail(const char* file, int line, const char* expr) {
    std::fprintf(stderr, "FAIL: %s:%d: %s\n", file, line, expr);
    ++failure_count();
}

inline int finish() {
    if (failure_count() == 0) {
        std::printf("OK\n");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failure_count());
    return EXIT_FAILURE;
}

} // namespace lweh_test

#define LWEH_EXPECT(cond) \
    do { if (!(cond)) { ::lweh_test::fail(__FILE__, __LINE__, #cond); } } while (0)

#define LWEH_EXPECT_EQ(a, b) LWEH_EXPECT((a) == (b))

#endif // LWEH_TEST_HARNESS_HPP_INCLUDED
