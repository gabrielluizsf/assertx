#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define make_test_dir(path) _mkdir(path)
#define remove_test_dir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define remove_test_dir(path) rmdir(path)
#endif

#include "../src/test_filter.h"
#include "xassert.h"

/* A scratch directory in the system temporary directory, with a few files
   written into it, removed again at the end of each test. Outside Windows it
   gets a fresh, private name each time: a fixed name in a shared /tmp could
   be made first by someone else, with links in it pointing elsewhere. */
static char g_dir[1024];

static void scratch_open(const char *name)
{
    const char *tmp = getenv("TMPDIR");

#ifdef _WIN32
    if (!tmp || !*tmp) tmp = getenv("TEMP");
    if (!tmp || !*tmp) tmp = ".";

    snprintf(g_dir, sizeof(g_dir), "%s/%s", tmp, name);
    make_test_dir(g_dir);
#else
    if (!tmp || !*tmp) tmp = "/tmp";

    snprintf(g_dir, sizeof(g_dir), "%s/%s.XXXXXX", tmp, name);

    if (!mkdtemp(g_dir))
        g_dir[0] = '\0';
#endif
}

static void scratch_write(const char *file, const char *content)
{
    char path[2048];
    FILE *f;

    if (!g_dir[0])
        return;

    snprintf(path, sizeof(path), "%s/%s", g_dir, file);
    f = fopen(path, "w");

    if (!f)
        return;

    fputs(content, f);
    fclose(f);
}

static void scratch_remove(const char *file)
{
    char path[2048];

    snprintf(path, sizeof(path), "%s/%s", g_dir, file);
    remove(path);
}

static int scratch_picks(const char *test, const char *name)
{
    char path[2048];
    const char *funcs[1];
    const char *names[1];
    unsigned char selected[1] = {0};

    snprintf(path, sizeof(path), "%s/main_test.c", g_dir);
    funcs[0] = test;
    names[0] = name;

    if (select_tests_calling(path, g_dir, funcs, 1, names, 1, selected) < 0)
        return -1;

    return selected[0];
}

void test_sources_follow_includes()
{
    scratch_open("assertx_sources_follow");
    scratch_write("draw.h", "static int draw(void) { return 1; }\n");
    scratch_write("render.h",
                  "#include \"draw.h\"\n"
                  "static int render(void) { return draw(); }\n");
    scratch_write("main_test.c",
                  "#include <stdio.h>\n"
                  "#include \"render.h\"\n"
                  "#include \"missing.h\"\n"
                  "void test_render() { render(); }\n"
                  "void test_plain() { puts(\"x\"); }\n");

    assert_equal(scratch_picks("test_render", "draw"), 1,
                 "test -> render (render.h) -> draw (draw.h, included by it)");
    assert_equal(scratch_picks("test_plain", "draw"), 0,
                 "a test that never reaches draw is not picked");

    scratch_remove("draw.h");
    scratch_remove("render.h");
    scratch_remove("main_test.c");
    remove_test_dir(g_dir);
}

void test_sources_include_cycle_ends()
{
    scratch_open("assertx_sources_cycle");
    scratch_write("a.h",
                  "#include \"b.h\"\n"
                  "static int from_a(void) { return draw(); }\n");
    scratch_write("b.h",
                  "#include \"a.h\"\n"
                  "static int from_b(void) { return from_a(); }\n");
    scratch_write("main_test.c",
                  "#include \"a.h\"\n"
                  "void test_b() { from_b(); }\n");

    assert_equal(scratch_picks("test_b", "draw"), 1,
                 "headers including each other are each read once");

    scratch_remove("a.h");
    scratch_remove("b.h");
    scratch_remove("main_test.c");
    remove_test_dir(g_dir);
}

void test_sources_unreadable_test_file()
{
    scratch_open("assertx_sources_missing");

    assert_equal(scratch_picks("test_a", "draw"), -1,
                 "a test file that is not there is reported, not guessed");

    remove_test_dir(g_dir);
}
