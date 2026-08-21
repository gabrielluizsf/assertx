#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>

#define mkdir(path, mode) _mkdir(path)
#define PATH_SEP "\\"

#define fopen_safe(fp, path, mode) fopen_s(&(fp), path, mode)
#define sscanf_safe sscanf_s
#define strncpy_safe(dest, destsz, src) strncpy_s(dest, destsz, src, _TRUNCATE)

#else
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>

#define PATH_SEP "/"

#define fopen_safe(fp, path, mode) ((fp) = fopen(path, mode)) == NULL
#define sscanf_safe sscanf
#define strncpy_safe(dest, destsz, src) strncpy(dest, src, destsz)

#endif

#define DEFAULT_COMPILER "gcc"
#define DEFAULT_CFLAGS "-Wall -Wextra -g"

/* The directory made inside the system temporary directory. */
#define CACHE_ROOT_NAME "assertx"

#define ASSERTX_PATH_MAX 1024
#define ASSERTX_CMD_MAX 4096

#define MAX_TEST_FILES 256
#define MAX_TEST_FUNCS 100
#define MAX_PARALLEL 64


/* =========================
   SETTINGS
========================= */

/* Everything a run can be told, from the command line or the environment.
   The defaults are what assertx has always done. */
typedef struct
{
    const char *compiler;
    const char *cflags;
    const char *cache_dir; /* NULL for the system temporary directory */
    int parallel;      /* how many compiles at once */
    int use_cache;     /* 0 with --no-cache         */
} assertx_settings;


/* One test file, from finding it through to reporting it. */
typedef struct
{
    char source[ASSERTX_PATH_MAX];      /* tests/foo_test.c               */
    char abs_source[ASSERTX_PATH_MAX];  /* what the runner includes       */
    char incdir[ASSERTX_PATH_MAX];      /* the test directory, absolute   */
    char runner[ASSERTX_PATH_MAX];      /* <cache>/__runner_foo_test.c    */
    char binary[ASSERTX_PATH_MAX];      /* <cache>/foo_test               */
    char depfile[ASSERTX_PATH_MAX];     /* <cache>/foo_test.d             */
    char logfile[ASSERTX_PATH_MAX];     /* <cache>/foo_test.log           */
    char filename[256];                 /* foo_test.c                     */
    int  test_count;
    int  needs_build;
    int  build_ok;
    int  cached;         /* the compile was skipped    */
} test_job;


/* =========================
   HELP / COPYRIGHT
========================= */

void show_help(const char *program)
{
    printf("\n");
    printf("assertx - C Test Runner\n");
    printf("Copyright (c) Gabriel Luiz\n");
    printf("https://github.com/gabrielluizsf\n");
    printf("\n");

    printf("Usage:\n");
    printf("  %s <test_directory> [options]\n", program);
    printf("\n");

    printf("Example:\n");
    printf("  %s ./tests\n", program);
    printf("  %s ./tests -j 8\n", program);
    printf("\n");

    printf("Options:\n");
    printf("  -j <n>        compile up to n test files at once\n");
    printf("                (default: one per processor)\n");
    printf("  --no-cache    rebuild everything, even what has not changed\n");
    printf("  --cc <name>   use this compiler instead of %s\n", DEFAULT_COMPILER);
    printf("  --cache-dir <path>\n");
    printf("                keep the cache here instead of in the system\n");
    printf("                temporary directory\n");
    printf("  -h, --help    print this\n");
    printf("\n");

    printf("Environment:\n");
    printf("  CC              same as --cc\n");
    printf("  ASSERTX_CFLAGS  replaces the default flags (%s)\n", DEFAULT_CFLAGS);
    printf("  ASSERTX_JOBS    same as -j\n");
    printf("  ASSERTX_CACHE_DIR  same as --cache-dir\n");
    printf("\n");

    printf("Description:\n");
    printf("  Automatically finds and runs all *_test.c files\n");
    printf("  inside the specified directory.\n");
    printf("\n");
    printf("  Compiling is nearly all of the time a run takes, so it is done\n");
    printf("  in parallel, and a test whose sources have not changed since it\n");
    printf("  was last built is not compiled again.\n");
    printf("\n");
    printf("  The compiled tests are kept in the system temporary directory,\n");
    printf("  under one directory per project, so nothing is left beside the\n");
    printf("  sources and the system clears the cache on its own.\n");
    printf("\n");
}


/* =========================
   UTILS
========================= */

int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix > lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


/* =========================
   WHERE THE CACHE LIVES
========================= */

/* Compiled tests are kept in the system temporary directory rather than in a
   build folder beside the sources. Nothing in there is worth keeping — all of
   it can be produced again from the test files — and every operating system
   already empties its temporary directory on its own, so the cache is cleared
   without anybody having to remember to do it, and a checkout stays clean.

   Each test directory gets its own subdirectory below that, named after the
   project and a hash of the directory's absolute path, so two projects that
   both have a foo_test.c never write over each other's binaries. */

static char g_cache_dir[ASSERTX_PATH_MAX];

/* Builds a path, and says whether all of it fitted. A truncated path is worse
   than no path: it does not fail, it names something else. */
static int path_printf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(out, out_size, fmt, args);
    va_end(args);

    return n >= 0 && (size_t)n < out_size;
}

/* The absolute form of `path`, or 0 if it cannot be worked out. */
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

/* The system temporary directory, without a trailing separator. */
static void temp_dir(char *out, size_t out_size)
{
    size_t len;

#ifdef _WIN32
    /* Honours TMP, then TEMP, then the user profile, then the Windows
       directory — the same order everything else on the system uses. */
    DWORD n = GetTempPathA((DWORD)out_size, out);

    if (n == 0 || (size_t)n >= out_size)
        snprintf(out, out_size, ".");
#else
    /* TMPDIR is what macOS sets to the per user directory it cleans up, and
       what a sandbox or a CI runner sets when /tmp is not the right place. */
    const char *env = getenv("TMPDIR");

    if (!env || !*env || strlen(env) >= out_size)
        env = "/tmp";

    snprintf(out, out_size, "%s", env);
#endif

    len = strlen(out);

    while (len > 1 && (out[len - 1] == '/' || out[len - 1] == '\\'))
        out[--len] = '\0';
}

/* FNV-1a. Short, needs nothing, and is only ever used to keep one project's
   cache away from another's. */
static unsigned long path_key(const char *s)
{
    unsigned long h = 2166136261UL;

    for (; *s; s++)
    {
        unsigned char c = (unsigned char)*s;

#ifdef _WIN32
        if (c >= 'A' && c <= 'Z')          /* paths here are case insensitive */
            c = (unsigned char)(c - 'A' + 'a');
#endif

        h = (h ^ c) * 16777619UL;
    }

    return h & 0xffffffffUL;
}

/* A readable name for the cache directory: the project the tests belong to,
   which is the directory holding the test directory, since "tests" on its own
   would tell nobody anything. Only there to make the cache legible to a
   person looking at it; the hash is what keeps it unique. */
static void project_label(const char *abs_dir, char *out, size_t out_size)
{
    const char *last = abs_dir;
    const char *prev = NULL;
    const char *p;
    size_t n = 0;

    for (p = abs_dir; *p; p++)
        if (*p == '/' || *p == '\\')
        {
            prev = last;
            last = p + 1;
        }

    if (prev && *prev && *prev != '/' && *prev != '\\')
        last = prev;

    for (p = last; *p && *p != '/' && *p != '\\' && n + 1 < out_size; p++)
    {
        char c = *p;
        int plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';

        out[n++] = plain ? c : '_';
    }

    out[n] = '\0';

    if (n == 0)
        snprintf(out, out_size, "tests");
}

/* Makes one directory. `strict` is for the directories assertx creates for
   itself: a shared /tmp is the one place where finding the directory already
   there means something, because assertx writes programs into it and then
   runs them, so it has to belong to this user and to nobody else. Directories
   the user named, or that were already on the way there, are their business
   and are only checked for being directories at all. */
static int make_dir(const char *path, int strict)
{
#ifdef _WIN32
    DWORD attr;

    (void)strict;

    if (_mkdir(path) == 0)
        return 1;

    attr = GetFileAttributesA(path);

    return attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;

    if (mkdir(path, 0700) == 0)
        return 1;

    if (!strict)
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);

    /* lstat, not stat: a symbolic link left in place of the cache would send
       the compiler's output somewhere else entirely. */
    if (lstat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode) &&
           st.st_uid == getuid() &&
           (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
#endif
}

/* Makes `path` and anything missing above it. Only the last directory is held
   to the strict check; the ones on the way there are the system's. */
static int make_dir_path(const char *path, int strict)
{
    char work[ASSERTX_PATH_MAX];
    size_t i;

    if (strlen(path) >= sizeof(work))
        return 0;

    strncpy_safe(work, sizeof(work), path);

    for (i = 1; work[i]; i++)
    {
        if (work[i] == '/' || work[i] == '\\')
        {
            char sep = work[i];

            work[i] = '\0';

            if (work[i - 1] != ':' && !make_dir(work, 0))   /* "C:" is not one */
                return 0;

            work[i] = sep;
        }
    }

    return make_dir(work, strict);
}

static int cache_path_too_long(void)
{
    printf("❌ The path to the cache is longer than %d characters\n",
           ASSERTX_PATH_MAX);
    return 0;
}

/* Works out where this test directory's cache belongs and makes sure it is
   there. Returns 0, with the reason printed, if it cannot be. */
static int ensure_cache_dir(const char *dir_path, const char *cache_root)
{
    char abs_dir[ASSERTX_PATH_MAX];
    char root[ASSERTX_PATH_MAX];
    char label[64];
    int strict;

    if (!absolute_path(dir_path, abs_dir, sizeof(abs_dir)))
        snprintf(abs_dir, sizeof(abs_dir), "%s", dir_path);

    strict = (cache_root == NULL || *cache_root == '\0');

    if (strict)
    {
        char tmp[ASSERTX_PATH_MAX];

        temp_dir(tmp, sizeof(tmp));

        if (!path_printf(root, sizeof(root), "%s%s%s", tmp, PATH_SEP, CACHE_ROOT_NAME))
            return cache_path_too_long();
    }
    else if (!path_printf(root, sizeof(root), "%s", cache_root))
    {
        return cache_path_too_long();
    }

    project_label(abs_dir, label, sizeof(label));

    if (!path_printf(g_cache_dir, sizeof(g_cache_dir), "%s%s%s-%08lx",
                     root, PATH_SEP, label, path_key(abs_dir)))
        return cache_path_too_long();

    /* Every file below is this directory plus a separator, at most
       "__runner_", a test file name and ".log". Refusing here is clearer than
       finding out one name at a time. */
    if (strlen(g_cache_dir) + sizeof("__runner_") + 256 + sizeof(".log") >=
        ASSERTX_PATH_MAX)
        return cache_path_too_long();

    if (!make_dir_path(root, strict) || !make_dir(g_cache_dir, strict))
    {
        printf("❌ Cannot use the cache directory %s\n", g_cache_dir);
        return 0;
    }

    return 1;
}

/* Whole seconds are not enough to decide whether a file changed: a build and
   an edit that land in the same second would compare equal, and the edit
   would be missed until something else forced a rebuild. Where the system
   records a finer time, use it; where it does not, treat the same second as
   changed and pay for one extra rebuild rather than run a stale binary. */
#if defined(__linux__) || defined(__CYGWIN__)
#  define ASSERTX_SUBSECOND_TIME 1
#  define ASSERTX_NSEC(st) ((long long)(st).st_mtim.tv_nsec)
#elif defined(__APPLE__)
#  define ASSERTX_SUBSECOND_TIME 1
#  define ASSERTX_NSEC(st) ((long long)(st).st_mtimespec.tv_nsec)
#else
#  define ASSERTX_SUBSECOND_TIME 0
#  define ASSERTX_NSEC(st) ((void)(st), 0LL)
#endif

/* When a file was last written, in nanoseconds, or -1 if it is not there. */
static long long file_mtime(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return -1;

    return (long long)st.st_mtime * 1000000000LL + ASSERTX_NSEC(st);
}

/* Did `source` change at or after `built`? With a coarse clock the test has
   to include equality, or an edit in the same second slips through. */
static int is_newer(long long source, long long built)
{
#if ASSERTX_SUBSECOND_TIME
    return source > built;
#else
    return source >= built;
#endif
}

/* How many compiles can usefully run at once. */
static int processor_count(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}


/* =========================
   GENERATING THE RUNNER
========================= */

/* Builds the generated runner in memory rather than writing it straight out,
   so it can be compared with what is already there. Rewriting an identical
   file would move its timestamp and defeat the rebuild check below. */
/* Collects the test_ functions in a file. When `runner_file` is given, their
   declarations are written to it as they are found; passing NULL just
   collects the names. */
int extract_test_functions(const char *source_path, FILE *runner_file,
                           char functions[][256])
{
    FILE *src;
    char line[512];
    int count = 0;

    if (fopen_safe(src, source_path, "r"))
        return 0;

    while (fgets(line, sizeof(line), src))
    {
        if (strstr(line, "void test_") != NULL)
        {
            char func_name[256] = {0};

#ifdef _WIN32
            if (sscanf_safe(line, "void %255[^ (]", func_name, (unsigned)_countof(func_name)) == 1)
#else
            if (sscanf_safe(line, "void %255[^ (]", func_name) == 1)
#endif
            {
                if (count < MAX_TEST_FUNCS)
                {
                    strncpy_safe(functions[count], sizeof(functions[count]), func_name);
#ifndef _WIN32
                    functions[count][sizeof(functions[count]) - 1] = '\0';
#endif
                    if (runner_file)
                        fprintf(runner_file, "void %s();\n", functions[count]);

                    count++;
                }
            }
        }
    }

    fclose(src);

    return count;
}

/* Writes `in` so it can be read back as a C string: a Windows path is full of
   backslashes, and an #include of one that has not been escaped would either
   not compile or quietly mean a different file. */
static void escape_for_c_string(const char *in, char *out, size_t out_size)
{
    size_t n = 0;

    for (; *in && n + 2 < out_size; in++)
    {
        if (*in == '\\' || *in == '"')
            out[n++] = '\\';

        out[n++] = *in;
    }

    out[n] = '\0';
}

/* Builds the generated runner in memory rather than writing it straight out,
   so it can be compared with what is already there. Rewriting an identical
   file would move its timestamp and defeat the rebuild check below.

   `include_path` is absolute: the runner no longer sits beside the tests, so
   there is no relative path from it that would keep working. A quoted include
   is resolved from the directory of the file holding it, so the test's own
   #include "xassert.h" still finds the header next to the test. */
static int build_runner_source(const char *source_path, const char *include_path,
                               char *out, size_t out_size, int *out_count)
{
    char include[ASSERTX_PATH_MAX * 2];
    char functions[MAX_TEST_FUNCS][256];
    int count;
    int i;
    size_t used = 0;

    *out_count = 0;

    count = extract_test_functions(source_path, NULL, functions);

    if (count == 0)
        return 0;

    escape_for_c_string(include_path, include, sizeof(include));

    used += (size_t)snprintf(out + used, out_size - used,
                             "#include \"%s\"\n\n", include);

    for (i = 0; i < count; i++)
        used += (size_t)snprintf(out + used, out_size - used,
                                 "void %s();\n", functions[i]);

    used += (size_t)snprintf(out + used, out_size - used,
                             "\nint main() {\n    printf(\"Running %d tests...\\n\");\n",
                             count);

    for (i = 0; i < count; i++)
    {
        used += (size_t)snprintf(out + used, out_size - used,
                                 "    printf(\"\u2192 %s\\n\");\n", functions[i]);
        used += (size_t)snprintf(out + used, out_size - used,
                                 "    %s();\n", functions[i]);
    }

    used += (size_t)snprintf(out + used, out_size - used,
                             "    test_summary();\n    return 0;\n}\n");

    if (used >= out_size)
        return 0;

    *out_count = count;
    return 1;
}

/* Writes the runner only when it would differ, so an unchanged test keeps its
   timestamp and stays cached. */
static int write_if_changed(const char *path, const char *content)
{
    FILE *existing;
    char *current;
    long size;
    int same = 0;
    FILE *out;

    /* fopen_safe is true when the open failed, and the macro has no outer
       parentheses, so it has to be used exactly this way round. */
    if (fopen_safe(existing, path, "rb"))
    {
        same = 0;                       /* nothing there to compare against */
    }
    else
    {
        fseek(existing, 0, SEEK_END);
        size = ftell(existing);
        rewind(existing);

        if (size >= 0 && (size_t)size == strlen(content))
        {
            current = (char *)malloc((size_t)size + 1);

            if (current)
            {
                if (fread(current, 1, (size_t)size, existing) == (size_t)size)
                {
                    current[size] = '\0';
                    same = (strcmp(current, content) == 0);
                }
                free(current);
            }
        }

        fclose(existing);
    }

    if (same)
        return 1;

    if (fopen_safe(out, path, "w"))
        return 0;

    fputs(content, out);
    fclose(out);
    return 1;
}


/* =========================
   DECIDING WHAT TO REBUILD
========================= */

/* Reads the .d file the compiler wrote last time and answers whether anything
   it lists has changed since. This is what makes editing one test recompile
   one test, while editing a shared header recompiles everything using it. */
static int sources_changed(const char *depfile, long long binary_time)
{
    FILE *f;
    char *text;
    long size;
    const char *cursor;
    char token[ASSERTX_PATH_MAX];
    int changed = 0;
    int past_target = 0;

    if (fopen_safe(f, depfile, "rb"))
        return 1;                    /* never built, or the record is gone */

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    if (size <= 0)
    {
        fclose(f);
        return 1;
    }

    text = (char *)malloc((size_t)size + 1);

    if (!text)
    {
        fclose(f);
        return 1;
    }

    if (fread(text, 1, (size_t)size, f) != (size_t)size)
    {
        free(text);
        fclose(f);
        return 1;
    }

    text[size] = '\0';
    fclose(f);

    cursor = text;

    /* The format is "target: a b \<newline> c d": everything up to the colon
       is what was built, and the rest is what it was built from.

       Splitting on whitespace is not enough. A space inside a path is written
       as "\ ", and now that the cache lives under the system temporary
       directory a space is no longer unusual — on Windows that path contains
       the account name. Getting this wrong would not break a run, it would
       just quietly rebuild everything every time. */
    while (*cursor)
    {
        size_t len = 0;

        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == '\r' || *cursor == '\n')
            cursor++;

        if (*cursor == '\\' && (cursor[1] == '\n' || cursor[1] == '\r'))
        {
            cursor += 2;                          /* a continued line */
            continue;
        }

        while (*cursor && *cursor != ' ' && *cursor != '\t' &&
               *cursor != '\r' && *cursor != '\n')
        {
            /* Only these three are escapes. A backslash anywhere else is part
               of a Windows path and belongs in the name. */
            if (*cursor == '\\' &&
                (cursor[1] == ' ' || cursor[1] == '\\' || cursor[1] == '#'))
                cursor++;

            if (len + 1 < sizeof(token))
                token[len++] = *cursor;

            cursor++;
        }

        token[len] = '\0';

        if (len == 0)
            continue;

        if (!past_target)
        {
            if (token[len - 1] == ':')
                past_target = 1;

            continue;
        }

        {
            long long t = file_mtime(token);

            /* Newer than the binary, or gone altogether: build it again. */
            if (t < 0 || is_newer(t, binary_time))
            {
                changed = 1;
                break;
            }
        }
    }

    free(text);
    return changed;
}


/* =========================
   COLLECTING THE WORK
========================= */

/* Works out one test file's paths, writes its runner, and decides whether it
   needs compiling. Returns 0 when the file holds no tests. */
static int prepare_job(test_job *job, const char *dir_path, const char *filename,
                       const assertx_settings *settings)
{
    static char runner_source[256 * 1024];
    char test_name[256];
    size_t name_len = strlen(filename);
    long long binary_time;
    int ok;

    if (name_len < 3 || name_len >= sizeof(test_name))
        return 0;

    memset(job, 0, sizeof(*job));
    strncpy_safe(job->filename, sizeof(job->filename), filename);
#ifndef _WIN32
    job->filename[sizeof(job->filename) - 1] = '\0';
#endif

    memcpy(test_name, filename, name_len - 2);
    test_name[name_len - 2] = '\0';

    ok  = path_printf(job->source, sizeof(job->source), "%s%s%s",
                      dir_path, PATH_SEP, filename);
    ok &= path_printf(job->runner, sizeof(job->runner), "%s%s__runner_%s.c",
                      g_cache_dir, PATH_SEP, test_name);
    ok &= path_printf(job->depfile, sizeof(job->depfile), "%s%s%s.d",
                      g_cache_dir, PATH_SEP, test_name);
    ok &= path_printf(job->logfile, sizeof(job->logfile), "%s%s%s.log",
                      g_cache_dir, PATH_SEP, test_name);
#ifdef _WIN32
    ok &= path_printf(job->binary, sizeof(job->binary), "%s%s%s.exe",
                      g_cache_dir, PATH_SEP, test_name);
#else
    ok &= path_printf(job->binary, sizeof(job->binary), "%s%s%s",
                      g_cache_dir, PATH_SEP, test_name);
#endif

    if (!ok)
        return 0;

    /* The runner is compiled from the cache directory, so everything it names
       has to be absolute. */
    if (!absolute_path(job->source, job->abs_source, sizeof(job->abs_source)) &&
        !path_printf(job->abs_source, sizeof(job->abs_source), "%s", job->source))
        return 0;

    if (!absolute_path(dir_path, job->incdir, sizeof(job->incdir)) &&
        !path_printf(job->incdir, sizeof(job->incdir), "%s", dir_path))
        return 0;

    if (!build_runner_source(job->source, job->abs_source, runner_source,
                             sizeof(runner_source), &job->test_count))
        return 0;

    if (!write_if_changed(job->runner, runner_source))
    {
        printf("❌ Failed to create runner for %s\n", filename);
        return 0;
    }

    binary_time = file_mtime(job->binary);

    if (!settings->use_cache || binary_time < 0)
        job->needs_build = 1;
    else
        job->needs_build = sources_changed(job->depfile, binary_time);

    return 1;
}


/* =========================
   COMPILING
========================= */

static void compile_command(const test_job *job, const assertx_settings *settings,
                            char *out, size_t out_size)
{
    /* -MMD records what this test was built from, so the next run can tell
       whether any of it has changed. -I is the test directory: the runner is
       compiled from the cache and would otherwise not find a header the test
       expects to sit beside it. Output goes to a log rather than the terminal,
       so parallel compiles do not interleave. */
    snprintf(out, out_size,
             "%s %s -I \"%s\" -MMD -MF \"%s\" \"%s\" -o \"%s\" > \"%s\" 2>&1",
             settings->compiler, settings->cflags, job->incdir,
             job->depfile, job->runner, job->binary, job->logfile);
}

static void print_compile_log(const test_job *job)
{
    FILE *log;
    char line[1024];

    if (fopen_safe(log, job->logfile, "r"))
        return;

    while (fgets(line, sizeof(line), log))
        fputs(line, stdout);

    fclose(log);
}

#ifndef _WIN32

/* Compiles everything that needs it, several at a time. Compiling is nearly
   all of the wall clock time and each file is independent of the others, so
   this is where a run gets shorter. */
static void build_all(test_job *jobs, int count, const assertx_settings *settings)
{
    pid_t running[MAX_PARALLEL];
    int slot_job[MAX_PARALLEL];
    int active = 0;
    int next = 0;
    int limit = settings->parallel;
    int i;

    if (limit < 1) limit = 1;
    if (limit > MAX_PARALLEL) limit = MAX_PARALLEL;

    for (i = 0; i < MAX_PARALLEL; i++)
    {
        running[i] = 0;
        slot_job[i] = -1;
    }

    while (next < count || active > 0)
    {
        while (active < limit && next < count)
        {
            test_job *job = &jobs[next];
            int slot;

            if (!job->needs_build)
            {
                job->build_ok = 1;
                job->cached = 1;
                next++;
                continue;
            }

            for (slot = 0; slot < limit; slot++)
                if (slot_job[slot] < 0)
                    break;

            if (slot >= limit)
                break;

            {
                char cmd[ASSERTX_CMD_MAX];
                pid_t pid;

                compile_command(job, settings, cmd, sizeof(cmd));
                pid = fork();

                if (pid == 0)
                {
                    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                    _exit(127);
                }

                if (pid < 0)
                {
                    job->build_ok = 0;
                    next++;
                    continue;
                }

                running[slot] = pid;
                slot_job[slot] = next;
                active++;
                next++;
            }
        }

        if (active == 0)
            continue;

        {
            int status = 0;
            pid_t done = wait(&status);
            int slot;

            if (done < 0)
                break;

            for (slot = 0; slot < limit; slot++)
            {
                if (running[slot] == done)
                {
                    test_job *job = &jobs[slot_job[slot]];

                    job->build_ok = (status == 0);
                    running[slot] = 0;
                    slot_job[slot] = -1;
                    active--;
                    break;
                }
            }
        }
    }
}

#else

/* Windows keeps the original one at a time path. The caching above is what
   matters most in day to day use and works the same everywhere; running
   compilers in parallel here needs process handling that could not be tested
   from where this was written, and a runner that does not build is worse than
   one that is slower. */
static void build_all(test_job *jobs, int count, const assertx_settings *settings)
{
    int i;

    for (i = 0; i < count; i++)
    {
        char cmd[ASSERTX_CMD_MAX];

        if (!jobs[i].needs_build)
        {
            jobs[i].build_ok = 1;
            jobs[i].cached = 1;
            continue;
        }

        compile_command(&jobs[i], settings, cmd, sizeof(cmd));
        jobs[i].build_ok = (system(cmd) == 0);
    }
}

#endif


/* =========================
   RUNNING
========================= */

static void run_jobs(test_job *jobs, int count, int *total, int *passed)
{
    int i;

    for (i = 0; i < count; i++)
    {
        test_job *job = &jobs[i];
        char command[ASSERTX_PATH_MAX + 8];

        (*total)++;

        if (!job->build_ok)
        {
            printf("❌ Compile failed: %s\n", job->filename);
            print_compile_log(job);
            printf("\n");
            continue;
        }

        printf("▶️ Running %s...%s\n", job->filename,
               job->cached ? " (cached)" : "");

        /* Quoted: the cache is under the system temporary directory, which on
           Windows goes through the account name and often has a space in it.
           cmd.exe strips the outer pair of quotes off the whole command, so
           the path it is meant to keep needs its own. */
#ifdef _WIN32
        snprintf(command, sizeof(command), "\"\"%s\"\"", job->binary);
#else
        snprintf(command, sizeof(command), "\"%s\"", job->binary);
#endif

        if (system(command) == 0)
        {
            printf("✅ Passed: %s\n\n", job->filename);
            (*passed)++;
        }
        else
        {
            printf("❌ Failed: %s\n\n", job->filename);
        }
    }
}


/* =========================
   SETTINGS
========================= */

#ifdef _WIN32
/* MSVC deprecates getenv. getenv_s replaces it by copying into a buffer of
   the caller's, so there is nothing to free; `buf` has to outlive the
   settings that end up pointing at it. NULL when the variable is not set, or
   when its value is longer than the buffer. */
static const char *env_value(const char *name, char *buf, size_t size)
{
    size_t len = 0;

    if (getenv_s(&len, buf, size, name) != 0 || len == 0)
        return NULL;

    return buf;
}

#define ENV_VALUE(name, buf) env_value(name, buf, sizeof(buf))
#else
/* getenv already points into the environment, which lasts the whole run, so
   the buffer is not needed and never named in the expansion. */
#define ENV_VALUE(name, buf) getenv(name)
#endif

static assertx_settings default_settings(void)
{
    assertx_settings s;
    const char *env;
#ifdef _WIN32
    /* One per variable that is kept: the settings hold the pointers for the
       rest of the run. */
    static char cc_buf[ASSERTX_PATH_MAX];
    static char cflags_buf[ASSERTX_CMD_MAX];
    static char cache_buf[ASSERTX_PATH_MAX];
    static char jobs_buf[32];
#endif

    s.compiler = DEFAULT_COMPILER;
    s.cflags = DEFAULT_CFLAGS;
    s.cache_dir = NULL;
    s.parallel = processor_count();
    s.use_cache = 1;

    env = ENV_VALUE("CC", cc_buf);
    if (env && *env) s.compiler = env;

    env = ENV_VALUE("ASSERTX_CFLAGS", cflags_buf);
    if (env && *env) s.cflags = env;

    env = ENV_VALUE("ASSERTX_CACHE_DIR", cache_buf);
    if (env && *env) s.cache_dir = env;

    env = ENV_VALUE("ASSERTX_JOBS", jobs_buf);
    if (env && *env)
    {
        int n = atoi(env);
        if (n > 0) s.parallel = n;
    }

    return s;
}

#undef ENV_VALUE

/* Returns 1 to carry on, 0 to stop (help was asked for, or an argument was
   not understood). */
static int parse_settings(int argc, char *argv[], assertx_settings *settings)
{
    int i;

    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-j") == 0)
        {
            if (++i >= argc) { printf("-j needs a number\n"); return 0; }
            settings->parallel = atoi(argv[i]);
            if (settings->parallel < 1) settings->parallel = 1;
        }
        else if (strcmp(argv[i], "--no-cache") == 0)
        {
            settings->use_cache = 0;
        }
        else if (strcmp(argv[i], "--cc") == 0)
        {
            if (++i >= argc) { printf("--cc needs a compiler\n"); return 0; }
            settings->compiler = argv[i];
        }
        else if (strcmp(argv[i], "--cache-dir") == 0)
        {
            if (++i >= argc) { printf("--cache-dir needs a path\n"); return 0; }
            settings->cache_dir = argv[i];
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            return 0;
        }
    }

    return 1;
}


/* =========================
   ONE FILE AT A TIME
========================= */

/* The original single file path: prepare, compile and run one test. main()
   no longer uses it — it batches the work instead — but it is the smallest
   unit of what assertx does, and it stays because it is worth being able to
   call and to test on its own. */
void run_test_file(const char *dir_path, const char *filename,
                   int *total, int *passed)
{
    static test_job job;
    assertx_settings settings;

    if (!ends_with(filename, "_test.c"))
        return;

    settings = default_settings();
    settings.parallel = 1;

    if (!ensure_cache_dir(dir_path, settings.cache_dir))
        return;

    if (!prepare_job(&job, dir_path, filename, &settings))
    {
        printf("⚠️ No test_ functions found in %s\n\n", filename);
        return;
    }

    build_all(&job, 1, &settings);
    run_jobs(&job, 1, total, passed);
}
