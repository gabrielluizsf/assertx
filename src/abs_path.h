#ifndef ASSERTX_ABS_PATH_H
#define ASSERTX_ABS_PATH_H

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* The absolute form of `path`, or 0 if it cannot be worked out. Outside
   Windows the file has to exist, since realpath resolves it on disk. */
static int absolute_path(const char *path, char *out, size_t out_size)
{
#ifdef _WIN32
    DWORD n = GetFullPathNameA(path, (DWORD)out_size, out, NULL);

    return n > 0 && (size_t)n < out_size;
#else
    char *resolved = realpath(path, NULL);

    if (!resolved)
        return 0;

    if (strlen(resolved) >= out_size)
    {
        free(resolved);
        return 0;
    }

    strcpy(out, resolved);
    free(resolved);
    return 1;
#endif
}

#endif
