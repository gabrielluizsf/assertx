#ifndef ASSERTX_C_TOKENS_H
#define ASSERTX_C_TOKENS_H

#include <stddef.h>

/* Just enough of a C tokenizer to tell code from everything that only looks
   like code: comments, string literals, character constants and preprocessor
   lines are skipped, so a brace inside '{' or a call written in a comment
   cannot be mistaken for the real thing.

   Every read checks `end` first. The text is not assumed to be terminated,
   and an unclosed comment, string or character constant stops at the end of
   the text instead of reading past it. */

typedef enum
{
    CTOK_END,
    CTOK_IDENT,
    CTOK_OPEN_PAREN,
    CTOK_CLOSE_PAREN,
    CTOK_OPEN_BRACE,
    CTOK_CLOSE_BRACE,
    CTOK_MEMBER,       /* . or ->, so s.sum() is not read as a call to sum */
    CTOK_OTHER
} ctok_kind;

typedef struct
{
    ctok_kind kind;
    const char *start;
    size_t len;
} ctok;

typedef struct
{
    const char *p;
    const char *end;
    int line_start;    /* nothing but blanks since the last newline */
} ctok_reader;

static void ctok_init(ctok_reader *r, const char *text, size_t len)
{
    r->p = text;
    r->end = text + len;
    r->line_start = 1;
}

static int ctok_is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int ctok_is_ident_char(char c)
{
    return ctok_is_ident_start(c) || (c >= '0' && c <= '9');
}

/* Skips a preprocessor line, with its backslash continuations. Whatever a
   macro holds is not code where it is written, and a #define holding an
   unmatched brace would otherwise throw the brace count off for the rest of
   the file. */
static void ctok_skip_directive(ctok_reader *r)
{
    while (r->p < r->end && *r->p != '\n')
    {
        if (*r->p == '\\' && r->p + 1 < r->end &&
            (r->p[1] == '\n' || r->p[1] == '\r'))
        {
            r->p += 2;
            if (r->p < r->end && r->p[-1] == '\r' && *r->p == '\n')
                r->p++;
            continue;
        }

        r->p++;
    }
}

/* Skips blanks and comments. Returns 1 when it moved. */
static int ctok_skip_space(ctok_reader *r)
{
    const char *p = r->p;
    const char *end = r->end;

    if (p >= end)
        return 0;

    if (*p == '\n')
    {
        r->line_start = 1;
        r->p = p + 1;
        return 1;
    }

    if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\f' || *p == '\v')
    {
        r->p = p + 1;
        return 1;
    }

    if (*p == '/' && p + 1 < end && p[1] == '/')
    {
        p += 2;
        while (p < end && *p != '\n')
            p++;
        r->p = p;
        return 1;
    }

    if (*p == '/' && p + 1 < end && p[1] == '*')
    {
        p += 2;
        while (p + 1 < end && !(p[0] == '*' && p[1] == '/'))
            p++;
        r->p = (p + 1 < end) ? p + 2 : end;   /* unclosed: up to the end */
        return 1;
    }

    if (*p == '#' && r->line_start)
    {
        ctok_skip_directive(r);
        return 1;
    }

    return 0;
}

/* A string literal or a character constant, from its opening quote. A
   newline also ends it: one left open by mistake should cost the rest of its
   line, not the rest of the file. */
static void ctok_skip_quoted(ctok_reader *r)
{
    char quote = *r->p++;

    while (r->p < r->end && *r->p != quote && *r->p != '\n')
    {
        if (*r->p == '\\' && r->p + 1 < r->end)
            r->p++;

        r->p++;
    }

    if (r->p < r->end && *r->p == quote)
        r->p++;
}

static ctok ctok_next(ctok_reader *r)
{
    ctok t;
    char c;

    while (ctok_skip_space(r))
        ;

    t.start = r->p;
    t.len = 0;

    if (r->p >= r->end)
    {
        t.kind = CTOK_END;
        return t;
    }

    r->line_start = 0;
    c = *r->p;

    if (c == '"' || c == '\'')
    {
        ctok_skip_quoted(r);
        t.kind = CTOK_OTHER;
    }
    else if (ctok_is_ident_start(c))
    {
        while (r->p < r->end && ctok_is_ident_char(*r->p))
            r->p++;
        t.kind = CTOK_IDENT;
    }
    else if (c >= '0' && c <= '9')
    {
        /* 1e10 and 0xff are numbers, not the identifiers e10 and xff. */
        while (r->p < r->end && (ctok_is_ident_char(*r->p) || *r->p == '.'))
            r->p++;
        t.kind = CTOK_OTHER;
    }
    else if (c == '-' && r->p + 1 < r->end && r->p[1] == '>')
    {
        r->p += 2;
        t.kind = CTOK_MEMBER;
    }
    else
    {
        r->p++;

        switch (c)
        {
        case '(': t.kind = CTOK_OPEN_PAREN;  break;
        case ')': t.kind = CTOK_CLOSE_PAREN; break;
        case '{': t.kind = CTOK_OPEN_BRACE;  break;
        case '}': t.kind = CTOK_CLOSE_BRACE; break;
        case '.': t.kind = CTOK_MEMBER;      break;
        default:  t.kind = CTOK_OTHER;       break;
        }
    }

    t.len = (size_t)(r->p - t.start);
    return t;
}

#endif
