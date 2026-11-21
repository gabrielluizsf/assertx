#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/test_filter.h"
#include "xassert.h"

/* Exactly its length, no '\0' after it: see c_tokens_test.c. */
static char *exact_copy(const char *s, size_t *len)
{
    char *copy;

    *len = strlen(s);
    copy = (char *)malloc(*len ? *len : 1);
    memcpy(copy, s, *len);
    return copy;
}

/* Whether `test`, defined somewhere in `header` + `src`, is picked by
   --uses `name`. `header` stands for a file the test includes. */
static int picks_with(const char *header, const char *src, const char *test,
                      const char *name)
{
    size_t lens[2];
    char *texts[2];
    const char *funcs[1];
    const char *names[1];
    unsigned char selected[1];

    texts[0] = exact_copy(header, &lens[0]);
    texts[1] = exact_copy(src, &lens[1]);
    funcs[0] = test;
    names[0] = name;

    select_tests_in((const char *const *)texts, lens, 2, funcs, 1, names, 1,
                    selected);

    free(texts[0]);
    free(texts[1]);
    return selected[0];
}

static int picks(const char *src, const char *test, const char *name)
{
    return picks_with("", src, test, name);
}

/* =========================
   direct calls
========================= */

void test_filter_direct_call()
{
    const char *src =
        "void test_a() { assert_equal(sum(1, 2), 3, \"x\"); }\n"
        "void test_b() { assert_equal(div(4, 2), 2, \"y\"); }\n";

    assert_true(picks(src, "test_a", "sum"), "test_a calls sum");
    assert_false(picks(src, "test_b", "sum"), "test_b does not call sum");
    assert_true(picks(src, "test_b", "div"), "test_b calls div");
}

void test_filter_several_names()
{
    const char *src =
        "void test_a() { ml_lerp(0, 1, 0.5); }\n"
        "void test_b() { other(); }\n";
    size_t len;
    char *text = exact_copy(src, &len);
    const char *texts[1];
    const char *funcs[2] = {"test_a", "test_b"};
    const char *names[3] = {"sum", "div", "ml_lerp"};
    unsigned char selected[2];
    int picked;

    texts[0] = text;
    picked = select_tests_in(texts, &len, 1, funcs, 2, names, 3, selected);
    free(text);

    assert_equal(picked, 1, "one test uses any of sum, div, ml_lerp");
    assert_true(selected[0], "test_a calls ml_lerp");
    assert_false(selected[1], "test_b calls none of them");
}

void test_filter_whole_name_only()
{
    assert_false(picks("void test_a() { xsum(1, 2); }", "test_a", "sum"),
                 "xsum is not sum");
    assert_false(picks("void test_a() { sum2(1, 2); }", "test_a", "sum"),
                 "sum2 is not sum");
}

void test_filter_variable_is_not_a_call()
{
    assert_false(picks("void test_a() { int sum = 0; sum = sum + 1; }",
                       "test_a", "sum"),
                 "a variable called sum is not a call to sum");
}

void test_filter_member_is_not_a_call()
{
    assert_false(picks("void test_a() { s.sum(1); p->sum(2); }", "test_a", "sum"),
                 "s.sum() and p->sum() are not the function sum");
}

void test_filter_comments_and_strings_ignored()
{
    assert_false(picks("void test_a() { /* sum(1) */ // sum(2)\n"
                       " puts(\"sum(3)\"); char c = 's'; }",
                       "test_a", "sum"),
                 "sum in comments and strings is not a call");
}

void test_filter_prototype_has_no_body()
{
    const char *src =
        "void test_a();\n"
        "int sum(int a, int b);\n"
        "void test_b() { sum(1, 2); }\n";

    assert_false(picks(src, "test_a", "sum"), "a prototype calls nothing");
    assert_true(picks(src, "test_b", "sum"), "the test after it is still read");
}

void test_filter_braces_stay_balanced()
{
    const char *src =
        "void test_a() { char c = '{'; if (x) { while (y) { } } }\n"
        "void test_b() { sum(1); }\n";

    assert_false(picks(src, "test_a", "sum"),
                 "test_a does not take test_b's body as its own");
    assert_true(picks(src, "test_b", "sum"), "test_b calls sum");
}

void test_filter_nested_parameters()
{
    assert_true(picks("void test_a(int (*f)(int)) { sum(1); }", "test_a", "sum"),
                "parameters with parentheses inside are read through");
}

void test_filter_directive_with_brace()
{
    assert_true(picks("#define OPEN {\nvoid test_a() { sum(1); }", "test_a", "sum"),
                "a brace inside a #define does not throw the count off");
}

/* =========================
   calls through other functions
========================= */

void test_filter_through_a_helper()
{
    const char *src =
        "static void draw(void) { }\n"
        "static void render(void) { draw(); }\n"
        "void test_render() { render(); }\n"
        "void test_other() { other(); }\n";

    assert_true(picks(src, "test_render", "draw"),
                "render calls draw, so a test of render uses draw");
    assert_false(picks(src, "test_other", "draw"),
                 "a test that never reaches draw is not picked");
}

void test_filter_through_an_included_header()
{
    const char *header =
        "static int draw(int x) { return x; }\n"
        "static int render(int x) { return draw(x) + 1; }\n"
        "static int frame(void) { return render(1); }\n";
    const char *src = "void test_frame() { assert_true(frame(), \"f\"); }\n";

    assert_true(picks_with(header, src, "test_frame", "draw"),
                "test -> frame -> render -> draw, across files");
}

void test_filter_recursion_ends()
{
    const char *src =
        "static int fact(int n) { return n ? n * fact(n - 1) : 1; }\n"
        "static int ping(int n);\n"
        "static int pong(int n) { return ping(n - 1); }\n"
        "static int ping(int n) { return n ? pong(n) : draw(); }\n"
        "void test_fact() { fact(5); }\n"
        "void test_ping() { ping(3); }\n";

    assert_false(picks(src, "test_fact", "draw"),
                 "a function calling itself ends, and never reaches draw");
    assert_true(picks(src, "test_ping", "draw"),
                "ping and pong calling each other end, and reach draw");
    assert_true(picks(src, "test_fact", "fact"),
                "a recursive function is found by its own name");
}

/* f0 calls f1, f1 calls f2, ... and the last calls draw. Following that
   chain must not depend on how deep it goes. */
void test_filter_very_long_chain()
{
    const int depth = 20000;
    size_t cap = (size_t)depth * 48 + 64;
    char *src = (char *)malloc(cap);
    size_t used = 0;
    int i;

    for (i = 0; i < depth - 1; i++)
        used += (size_t)snprintf(src + used, cap - used,
                                 "static void f%d(void) { f%d(); }\n", i, i + 1);

    used += (size_t)snprintf(src + used, cap - used,
                             "static void f%d(void) { draw(); }\n"
                             "void test_deep() { f0(); }\n", depth - 1);

    assert_true(picks(src, "test_deep", "draw"),
                "a chain 20000 calls long is followed to the end");

    free(src);
}

/* =========================
   reading broken files safely
========================= */

void test_filter_unclosed_at_end()
{
    assert_true(picks("void test_a() { sum(1); /* never closed", "test_a", "sum"),
                "an unclosed comment at the end is read safely");
    assert_true(picks("void test_a() { sum(1); puts(\"never closed", "test_a", "sum"),
                "an unclosed string at the end is read safely");
    assert_true(picks("void test_a() { sum(1); '", "test_a", "sum"),
                "a lone quote at the end is read safely");
    assert_false(picks("void test_a() {", "test_a", "sum"),
                 "a body cut off at the end is read safely");
    assert_false(picks("void test_a(", "test_a", "sum"),
                 "a parameter list cut off at the end is read safely");
}

/* =========================
   is_function_name
========================= */

void test_function_name_accepts()
{
    assert_true(is_function_name("sum"), "sum");
    assert_true(is_function_name("ml_lerp"), "ml_lerp");
    assert_true(is_function_name("_draw2"), "_draw2");
}

void test_function_name_rejects()
{
    assert_false(is_function_name(""), "an empty name");
    assert_false(is_function_name("2d"), "a name starting with a digit");
    assert_false(is_function_name("sum("), "a name with a parenthesis");
    assert_false(is_function_name("sum;rm"), "a name with a ;");
    assert_false(is_function_name("$(id)"), "shell syntax");
    assert_false(is_function_name(NULL), "NULL");
}
