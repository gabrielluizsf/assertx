#ifndef ASSERTX_SOURCE_SET_H
#define ASSERTX_SOURCE_SET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abs_path.h"
#include "read_file.h"

/* The sources a test is compiled from: the test file and every file it pulls
   in with #include "...", followed through the headers' own includes.

   assertx compiles one translation unit per test, so whatever the test can
   call has its body somewhere in here — usually a header-only library, or a
   .c file the test includes. <...> includes are system headers and are left
   alone.

   An include is looked for where the compiler would: beside the file that
   includes it, then in the test directory (the -I assertx passes). Each file
   is read once, so headers that include each other cannot loop, and the
   number and size of files are capped. Nothing read here is run or printed;
   it is only scanned for calls. */

#define SOURCE_SET_MAX_FILES 256
#define SOURCE_SET_MAX_BYTES (64L * 1024 * 1024)
#define SOURCE_PATH_MAX 1024

typedef struct
{
    char *text[SOURCE_SET_MAX_FILES];
    size_t len[SOURCE_SET_MAX_FILES];
    char path[SOURCE_SET_MAX_FILES][SOURCE_PATH_MAX];
    int count;
    size_t bytes;
} source_set;

static void source_set_free(source_set *s)
{
    int i;

    for (i = 0; i < s->count; i++)
        free(s->text[i]);

    s->count = 0;
    s->bytes = 0;
}

/* Returns 1 when added, 0 when already there or over a cap, -1 when the file
   cannot be read — which, for an include, means "look somewhere else". */
static int source_set_add(source_set *s, const char *path)
{
    char abs[SOURCE_PATH_MAX];
    char *text;
    size_t len = 0;
    int i;

    if (!absolute_path(path, abs, sizeof(abs)))
        return -1;

    for (i = 0; i < s->count; i++)
        if (strcmp(s->path[i], abs) == 0)
            return 0;

    if (s->count >= SOURCE_SET_MAX_FILES)
        return 0;

    text = read_text_file(abs, &len);

    if (!text)
        return -1;

    if (s->bytes + len > (size_t)SOURCE_SET_MAX_BYTES)
    {
        free(text);
        return 0;
    }

    memcpy(s->path[s->count], abs, strlen(abs) + 1);
    s->text[s->count] = text;
    s->len[s->count] = len;
    s->bytes += len;
    s->count++;
    return 1;
}

static int source_is_absolute(const char *name)
{
    return name[0] == '/' || name[0] == '\\' ||
           (name[0] != '\0' && name[1] == ':');
}

/* The directory part of `path`, without the separator. */
static void source_dir_of(const char *path, char *out, size_t out_size)
{
    size_t cut = 0;
    size_t i;

    for (i = 0; path[i]; i++)
        if (path[i] == '/' || path[i] == '\\')
            cut = i;

    if (cut >= out_size)
        cut = 0;

    memcpy(out, path, cut);
    out[cut] = '\0';
}

/* Finds an include where the compiler would and adds it. */
static void source_set_include(source_set *s, const char *from, const char *incdir,
                               const char *name)
{
    char dir[SOURCE_PATH_MAX];
    char candidate[SOURCE_PATH_MAX * 2 + 2];
    int n;

    if (source_is_absolute(name))
    {
        source_set_add(s, name);
        return;
    }

    source_dir_of(from, dir, sizeof(dir));
    n = snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);

    if (n > 0 && (size_t)n < sizeof(candidate) && source_set_add(s, candidate) >= 0)
        return;

    n = snprintf(candidate, sizeof(candidate), "%s/%s", incdir, name);

    if (n > 0 && (size_t)n < sizeof(candidate))
        source_set_add(s, candidate);
}

/* Every #include "name" in one text. An include written inside a comment is
   followed too: at worst that reads one more file, never one fewer. */
static void source_set_scan_includes(source_set *s, int index, const char *incdir)
{
    const char *p = s->text[index];
    const char *end = p + s->len[index];
    char name[SOURCE_PATH_MAX];

    while (p < end)
    {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        const char *q = p;

        if (!line_end)
            line_end = end;

        while (q < line_end && (*q == ' ' || *q == '\t'))
            q++;

        if (q < line_end && *q == '#')
        {
            q++;
            while (q < line_end && (*q == ' ' || *q == '\t'))
                q++;

            if ((size_t)(line_end - q) > 7 && memcmp(q, "include", 7) == 0)
            {
                q += 7;
                while (q < line_end && (*q == ' ' || *q == '\t'))
                    q++;

                if (q < line_end && *q == '"')
                {
                    const char *start = ++q;

                    while (q < line_end && *q != '"')
                        q++;

                    if (q < line_end && q > start &&
                        (size_t)(q - start) < sizeof(name))
                    {
                        memcpy(name, start, (size_t)(q - start));
                        name[q - start] = '\0';
                        source_set_include(s, s->path[index], incdir, name);
                    }
                }
            }
        }

        if (line_end == end)
            break;

        p = line_end + 1;
    }
}

/* Reads the test file and everything it includes. 0 if the test file itself
   cannot be read. */
static int source_set_collect(source_set *s, const char *test_path, const char *incdir)
{
    int i;

    s->count = 0;
    s->bytes = 0;

    if (source_set_add(s, test_path) != 1)
        return 0;

    /* s->count grows as includes are found, so this walks them all. */
    for (i = 0; i < s->count; i++)
        source_set_scan_includes(s, i, incdir);

    return 1;
}

#endif
