#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define bool int
#define string const char *
#define true 1
#define false 0

static int __test_failures = 0;
static int __test_assertions = 0;

void assertx(bool condition, string message)
{
    __test_assertions++;

    if (condition)
        printf("   ✅ %s\n", message);
    else
    {
        printf("   ❌ %s\n", message);
        __test_failures++;
    }
}

/* ===============================
   Funções internas por tipo
   =============================== */

static inline int __eq_int(int a, int b) { return a == b; }
static inline int __eq_long(long a, long b) { return a == b; }
static inline int __eq_float(float a, float b) { return a == b; }
static inline int __eq_double(double a, double b) { return a == b; }
static inline int __eq_char(char a, char b) { return a == b; }
static inline int __eq_string(string a, string b)
{
    if (a == NULL || b == NULL)
        return a == b;
    return strcmp(a, b) == 0;
}
static inline int __eq_ptr(void *a, void *b) { return a == b; }

/* ===============================
   Macro inteligente
   =============================== */

#define assert_equal(a, b, message)    \
    assertx(                           \
        _Generic((a),                  \
            int: __eq_int,             \
            long: __eq_long,           \
            float: __eq_float,         \
            double: __eq_double,       \
            char: __eq_char,           \
            const char *: __eq_string, \
            char *: __eq_string,       \
            default: __eq_ptr)(a, b),  \
        message)

/* =============================== */

#define assert_null(v, message) assertx((v) == NULL, message)

void assert_true(bool condition, string message) { assertx(condition, message); }

void assert_false(bool condition, string message) { assertx(!condition, message); }

void test_summary()
{
    printf("\n----------------------------------\n");
    printf("Assertions: %d\n", __test_assertions);
    printf("Failures  : %d\n", __test_failures);
    printf("----------------------------------\n");

    exit(__test_failures > 0);
}

#endif
