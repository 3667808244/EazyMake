#pragma once
// include/ezmk/test_assert.h
//
// 1.1.0-dev.6: Lightweight assertion macros for the ezmk built-in test framework.
// Users can optionally include this header to get assertion helpers.
// Usage:
//   #include <ezmk/test_assert.h>
//   int main() {
//       EZMK_ASSERT(1 + 1 == 2);
//       EZMK_ASSERT_EQ(some_func(), 42);
//       return 0;  // 0 = all tests passed
//   }

#include <cstdio>

// Basic assertion: if condition fails, print file+line and return 1.
#define EZMK_ASSERT(cond)                                             \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::fprintf(stderr, "ASSERT FAIL: %s:%d: %s\n",          \
                         __FILE__, __LINE__, #cond);                   \
            return 1;                                                  \
        }                                                              \
    } while (0)

// Equality assertion: if a != b, print both values and return 1.
#define EZMK_ASSERT_EQ(a, b)                                          \
    do {                                                               \
        if ((a) != (b)) {                                             \
            std::fprintf(stderr, "ASSERT FAIL: %s:%d: %s != %s\n",    \
                         __FILE__, __LINE__, #a, #b);                 \
            return 1;                                                  \
        }                                                              \
    } while (0)

// Inequality assertion: if a == b, print both values and return 1.
#define EZMK_ASSERT_NEQ(a, b)                                         \
    do {                                                               \
        if ((a) == (b)) {                                             \
            std::fprintf(stderr, "ASSERT FAIL: %s:%d: %s == %s\n",    \
                         __FILE__, __LINE__, #a, #b);                 \
            return 1;                                                  \
        }                                                              \
    } while (0)
