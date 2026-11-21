#ifndef ASSERTX_READ_FILE_H
#define ASSERTX_READ_FILE_H

#include <stdio.h>
#include <stdlib.h>

/* A source file larger than this is not read: nobody writes one by hand, and
   reading it whole is the one way --uses could ask for a lot of memory. */
#define READ_FILE_MAX (16L * 1024 * 1024)

/* Reads a whole file, terminated, and says how long it is. The caller frees
   the result. NULL if it cannot be read or is too large. */
static char *read_text_file(const char *path, size_t *out_len)
{
    FILE *f;
    char *text;
    long size;

#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0)
        return NULL;
#else
    if ((f = fopen(path, "rb")) == NULL)
        return NULL;
#endif

    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
        size > READ_FILE_MAX || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }

    text = (char *)malloc((size_t)size + 1);

    if (text && fread(text, 1, (size_t)size, f) != (size_t)size)
    {
        free(text);
        text = NULL;
    }

    fclose(f);

    if (text)
    {
        text[size] = '\0';
        *out_len = (size_t)size;
    }

    return text;
}

#endif
