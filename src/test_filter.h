#ifndef ASSERTX_TEST_FILTER_H
#define ASSERTX_TEST_FILTER_H

#include <stdlib.h>
#include <string.h>

#include "call_graph.h"
#include "source_set.h"

/* Picking the tests that use given functions.

   `assertx ./tests --uses draw` runs only the test_ functions that call draw,
   directly or through other functions: when render calls draw, a test that
   calls render runs too. The chain is followed through the test file and the
   headers it includes, however long it is.

   A name counts when it is written as a call — draw(...) — so a variable
   that happens to be called draw does not pick a test, and neither does
   s.draw(), a comment or a string that mentions it. A function only passed
   around by pointer, or called from inside a macro, is not seen. */

#define TEST_FILTER_MAX_NAMES 64

/* Whether `s` is a C identifier, the only thing --uses accepts. Anything else
   is a typo, and is turned away rather than matching nothing in silence. */
int is_function_name(const char *s)
{
    if (!s || !ctok_is_ident_start(*s))
        return 0;

    for (s++; *s; s++)
        if (!ctok_is_ident_char(*s))
            return 0;

    return 1;
}

/* Sets selected[i] for each funcs[i] that reaches one of `names` in the given
   source texts, and clears the others. Returns how many were selected, or -1
   when memory ran out. The texts need not be terminated. */
int select_tests_in(const char *const *texts, const size_t *lens, int text_count,
                    const char *const *funcs, int func_count,
                    const char *const *names, int name_count,
                    unsigned char *selected)
{
    call_graph g;
    int picked = 0;
    int i;

    for (i = 0; i < func_count; i++)
        selected[i] = 0;

    cg_init(&g);

    for (i = 0; i < text_count; i++)
        cg_add_source(&g, texts[i], lens[i]);

    if (!cg_mark_callers_of(&g, names, name_count))
    {
        cg_free(&g);
        return -1;
    }

    for (i = 0; i < func_count; i++)
        if (cg_is_marked(&g, funcs[i]))
        {
            selected[i] = 1;
            picked++;
        }

    cg_free(&g);
    return picked;
}

/* The same, reading the test file at `test_path` and what it includes.
   `incdir` is the test directory. -1 when the test file cannot be read. */
int select_tests_calling(const char *test_path, const char *incdir,
                         const char *const *funcs, int func_count,
                         const char *const *names, int name_count,
                         unsigned char *selected)
{
    source_set *s = (source_set *)malloc(sizeof(source_set));
    int picked;

    if (!s)
        return -1;

    if (!source_set_collect(s, test_path, incdir))
    {
        free(s);
        return -1;
    }

    picked = select_tests_in((const char *const *)s->text, s->len, s->count,
                             funcs, func_count, names, name_count, selected);

    source_set_free(s);
    free(s);
    return picked;
}

#endif
