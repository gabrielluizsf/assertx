#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c_tokens.h"
#include "xassert.h"

/* The text is copied into a buffer of exactly its length, with no '\0' after
   it, so a read past the end lands outside the allocation — where a memory
   checker (ASSERTX_CFLAGS="-g -fsanitize=address") catches it. */
static char *exact_copy(const char *s, size_t *len)
{
    char *copy;

    *len = strlen(s);
    copy = (char *)malloc(*len ? *len : 1);
    memcpy(copy, s, *len);
    return copy;
}

/* How many tokens of `kind` the text holds, reading it to the end. */
static int count_kind(const char *src, ctok_kind kind)
{
    size_t len;
    char *text = exact_copy(src, &len);
    ctok_reader r;
    ctok t;
    int n = 0;
    int guard = 0;

    ctok_init(&r, text, len);

    while ((t = ctok_next(&r)).kind != CTOK_END && guard++ < 10000)
        if (t.kind == kind)
            n++;

    free(text);
    return n;
}

void test_tokens_identifiers()
{
    assert_equal(count_kind("int sum = add(a, b);", CTOK_IDENT), 5,
                 "int, sum, add, a and b are identifiers");
}

void test_tokens_numbers_are_not_identifiers()
{
    assert_equal(count_kind("x = 1e10 + 0xff;", CTOK_IDENT), 1,
                 "1e10 and 0xff are numbers, only x is an identifier");
}

void test_tokens_skip_comments_and_quotes()
{
    assert_equal(count_kind("/* { */ // {\n char c = '{'; s = \"{\";",
                            CTOK_OPEN_BRACE), 0,
                 "braces in comments, strings and chars are not braces");
}

void test_tokens_skip_directives()
{
    assert_equal(count_kind("#define OPEN {\n#define X \\\n {\n{", CTOK_OPEN_BRACE), 1,
                 "a brace in a #define, continued lines too, is skipped");
}

void test_tokens_member_access()
{
    assert_equal(count_kind("s.a(); p->b(); x - > y;", CTOK_MEMBER), 2,
                 ". and -> are member access, '- >' is not");
}

void test_tokens_unclosed_comment_stops_at_end()
{
    assert_equal(count_kind("a /* never closed", CTOK_IDENT), 1,
                 "an unclosed comment ends at the end of the text");
    assert_equal(count_kind("a /", CTOK_IDENT), 1,
                 "a lone / at the end is read safely");
    assert_equal(count_kind("a /*", CTOK_IDENT), 1,
                 "/* as the last two bytes is read safely");
}

void test_tokens_unclosed_quotes_stop_at_end()
{
    assert_equal(count_kind("a \"never closed", CTOK_IDENT), 1,
                 "an unclosed string ends at the end of the text");
    assert_equal(count_kind("a 'x", CTOK_IDENT), 1,
                 "an unclosed char ends at the end of the text");
    assert_equal(count_kind("a \"ends in a backslash\\", CTOK_IDENT), 1,
                 "a backslash as the last byte is read safely");
    assert_equal(count_kind("a '", CTOK_IDENT), 1,
                 "a quote as the last byte is read safely");
}

void test_tokens_unclosed_quote_costs_one_line()
{
    assert_equal(count_kind("printf(\"don't);\nb c", CTOK_IDENT), 3,
                 "an unclosed quote stops at the newline, not at the end");
}

void test_tokens_directive_at_end()
{
    assert_equal(count_kind("a\n#define X \\", CTOK_IDENT), 1,
                 "a directive ending in a backslash at the end is read safely");
}
