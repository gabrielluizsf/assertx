#ifndef ASSERTX_CALL_GRAPH_H
#define ASSERTX_CALL_GRAPH_H

#include <stdlib.h>
#include <string.h>

#include "c_tokens.h"

/* Who calls whom, across every source a test is compiled from.

   One node per distinct name. For each call `callee(...)` written inside the
   body of `caller`, an edge is kept from callee back to caller, so that
   starting from the functions --uses names and following the edges finds
   every function that reaches them — render, because it calls draw, and the
   test, because it calls render.

   Names point into the source texts, which have to outlive the graph. When
   the same name is defined twice (an #ifdef _WIN32 / #else pair, or two
   static helpers in different headers) the calls of both are kept: running a
   test too many is better than missing one. */

typedef struct
{
    const char **name;
    size_t *len;
    int *callers;          /* first edge into this node, or -1 */
    unsigned char *marked;
    int count;
    int cap;

    int *table;            /* open addressing: node id + 1, 0 when empty */
    size_t table_cap;      /* a power of two */

    int *edge_caller;
    int *edge_next;
    int edge_count;
    int edge_cap;

    int failed;            /* ran out of memory; the graph is not usable */
} call_graph;

static void cg_init(call_graph *g)
{
    memset(g, 0, sizeof(*g));
}

static void cg_free(call_graph *g)
{
    free(g->name);
    free(g->len);
    free(g->callers);
    free(g->marked);
    free(g->table);
    free(g->edge_caller);
    free(g->edge_next);
    cg_init(g);
}

/* realloc that keeps the old block when it fails. */
static int cg_resize(void **p, size_t count, size_t size)
{
    void *bigger = realloc(*p, count * size);

    if (!bigger)
        return 0;

    *p = bigger;
    return 1;
}

static size_t cg_hash(const char *s, size_t len)
{
    size_t h = 2166136261u;
    size_t i;

    for (i = 0; i < len; i++)
        h = (h ^ (unsigned char)s[i]) * 16777619u;

    return h;
}

static void cg_place(call_graph *g, int id)
{
    size_t mask = g->table_cap - 1;
    size_t h = cg_hash(g->name[id], g->len[id]) & mask;

    while (g->table[h])
        h = (h + 1) & mask;

    g->table[h] = id + 1;
}

/* Keeps the table at most half full. */
static int cg_grow_table(call_graph *g)
{
    size_t cap = g->table_cap ? g->table_cap * 2 : 256;
    int *table = (int *)calloc(cap, sizeof(int));
    int i;

    if (!table)
        return 0;

    free(g->table);
    g->table = table;
    g->table_cap = cap;

    for (i = 0; i < g->count; i++)
        cg_place(g, i);

    return 1;
}

static int cg_new_node(call_graph *g, const char *name, size_t len)
{
    if (g->count == g->cap)
    {
        int cap = g->cap ? g->cap * 2 : 256;

        if (!cg_resize((void **)&g->name, (size_t)cap, sizeof(*g->name)) ||
            !cg_resize((void **)&g->len, (size_t)cap, sizeof(*g->len)) ||
            !cg_resize((void **)&g->callers, (size_t)cap, sizeof(*g->callers)) ||
            !cg_resize((void **)&g->marked, (size_t)cap, sizeof(*g->marked)))
            return -1;

        g->cap = cap;
    }

    g->name[g->count] = name;
    g->len[g->count] = len;
    g->callers[g->count] = -1;
    g->marked[g->count] = 0;

    return g->count++;
}

/* The node for a name, made when `create` is set. -1 when there is none. */
static int cg_node(call_graph *g, const char *name, size_t len, int create)
{
    size_t mask;
    size_t h;
    int id;

    if (g->failed)
        return -1;

    if (create && (size_t)(g->count + 1) * 2 > g->table_cap &&
        !cg_grow_table(g))
    {
        g->failed = 1;
        return -1;
    }

    if (g->table_cap == 0)
        return -1;

    mask = g->table_cap - 1;
    h = cg_hash(name, len) & mask;

    while (g->table[h])
    {
        id = g->table[h] - 1;

        if (g->len[id] == len && memcmp(g->name[id], name, len) == 0)
            return id;

        h = (h + 1) & mask;
    }

    if (!create)
        return -1;

    id = cg_new_node(g, name, len);

    if (id < 0)
    {
        g->failed = 1;
        return -1;
    }

    g->table[h] = id + 1;
    return id;
}

static void cg_add_edge(call_graph *g, int callee, int caller)
{
    int head = g->callers[callee];

    /* A body calling the same thing over and over needs one edge. */
    if (head >= 0 && g->edge_caller[head] == caller)
        return;

    if (g->edge_count == g->edge_cap)
    {
        int cap = g->edge_cap ? g->edge_cap * 2 : 1024;

        if (!cg_resize((void **)&g->edge_caller, (size_t)cap, sizeof(int)) ||
            !cg_resize((void **)&g->edge_next, (size_t)cap, sizeof(int)))
        {
            g->failed = 1;
            return;
        }

        g->edge_cap = cap;
    }

    g->edge_caller[g->edge_count] = caller;
    g->edge_next[g->edge_count] = head;
    g->callers[callee] = g->edge_count++;
}

/* Reads one source text and adds the calls made by every function defined in
   it. Returns 0 when memory ran out. */
static int cg_add_source(call_graph *g, const char *text, size_t len)
{
    ctok_reader r;
    ctok t;
    ctok last_ident;      /* the identifier just before a top level "(" */
    ctok candidate;       /* and the one whose parameters were read last */
    ctok callee;          /* the identifier just read inside a body */
    int have_ident = 0;
    int have_candidate = 0;
    int have_callee = 0;
    int after_params = 0; /* a top level ")" was the last thing read */
    int parens = 0;
    int depth = 0;        /* braces */
    int current = -1;     /* the function whose body is being read */
    int member = 0;       /* the last token was . or -> */

    ctok_init(&r, text, len);
    last_ident.start = candidate.start = callee.start = text;
    last_ident.len = candidate.len = callee.len = 0;

    while ((t = ctok_next(&r)).kind != CTOK_END && !g->failed)
    {
        if (depth > 0)
        {
            /* name( is a call, unless it was s.name( or p->name( */
            if (t.kind == CTOK_OPEN_PAREN && have_callee && current >= 0)
            {
                int id = cg_node(g, callee.start, callee.len, 1);

                if (id >= 0)
                    cg_add_edge(g, id, current);
            }

            have_callee = (t.kind == CTOK_IDENT && !member);
            if (have_callee)
                callee = t;

            member = (t.kind == CTOK_MEMBER);

            if (t.kind == CTOK_OPEN_BRACE)
                depth++;
            else if (t.kind == CTOK_CLOSE_BRACE && --depth == 0)
                current = -1;

            continue;
        }

        /* Top level: look for name ( ... ) { — a function definition. A
           prototype ends in ";" instead and an initializer starts with "=",
           so neither is taken for one. */
        switch (t.kind)
        {
        case CTOK_IDENT:
            if (parens == 0)
            {
                last_ident = t;
                have_ident = 1;
                after_params = 0;
                continue;
            }
            break;

        case CTOK_OPEN_PAREN:
            if (parens == 0)
            {
                candidate = last_ident;
                have_candidate = have_ident;
            }
            parens++;
            after_params = 0;
            break;

        case CTOK_CLOSE_PAREN:
            if (parens > 0)
                parens--;
            after_params = (parens == 0 && have_candidate);
            break;

        case CTOK_OPEN_BRACE:
            current = (parens == 0 && after_params)
                          ? cg_node(g, candidate.start, candidate.len, 1)
                          : -1;
            depth = 1;
            have_callee = 0;
            member = 0;
            after_params = 0;
            break;

        default:
            after_params = 0;
            break;
        }

        have_ident = 0;
    }

    return !g->failed;
}

/* Marks the named functions and everything that calls them, however
   indirectly. This walks a queue, not the call stack, so no chain is too
   deep for it; and recursion in the code being read — f calling f, or a and
   b calling each other — cannot make it loop, because a function is queued
   at most once. The queue never holds more than the number of functions.
   Returns 0 when memory ran out. */
static int cg_mark_callers_of(call_graph *g, const char *const *names, int count)
{
    int *queue;
    int head = 0;
    int tail = 0;
    int i;

    if (g->failed)
        return 0;

    if (g->count == 0)
        return 1;

    queue = (int *)malloc((size_t)g->count * sizeof(int));

    if (!queue)
        return 0;

    for (i = 0; i < count; i++)
    {
        int id = cg_node(g, names[i], strlen(names[i]), 0);

        if (id >= 0 && !g->marked[id])
        {
            g->marked[id] = 1;
            queue[tail++] = id;
        }
    }

    while (head < tail)
    {
        int e;

        for (e = g->callers[queue[head++]]; e >= 0; e = g->edge_next[e])
        {
            int caller = g->edge_caller[e];

            if (!g->marked[caller])
            {
                g->marked[caller] = 1;
                queue[tail++] = caller;
            }
        }
    }

    free(queue);
    return 1;
}

static int cg_is_marked(call_graph *g, const char *name)
{
    int id = cg_node(g, name, strlen(name), 0);

    return id >= 0 && g->marked[id];
}

#endif
