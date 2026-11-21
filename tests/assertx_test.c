#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif


#include "../src/assertx_runner.h"
#include "xassert.h"

/* =========================
   ends_with
========================= */

void test_ends_with_true()
{
    assert_true(ends_with("math_test.c", "_test.c"),
                "math_test.c should end with _test.c");
}

void test_ends_with_false()
{
    assert_false(ends_with("math.c", "_test.c"),
                 "math.c should not end with _test.c");
}

void test_ends_with_null()
{
    assert_false(ends_with(NULL, "_test.c"),
                 "NULL string should return false");
}

/* =========================
   the cache directory
========================= */

void test_ensure_cache_dir()
{
    char tmp[ASSERTX_PATH_MAX];
    char first[ASSERTX_PATH_MAX];

    assert_true(ensure_cache_dir(".", NULL),
                "cache directory should be created");

#ifdef _WIN32
    assert_true(_access(g_cache_dir, 0) == 0,
                "cache directory should exist (windows)");
#else
    assert_true(access(g_cache_dir, F_OK) == 0,
                "cache directory should exist (unix)");
#endif

    temp_dir(tmp, sizeof(tmp));

    assert_equal(strncmp(g_cache_dir, tmp, strlen(tmp)), 0,
                 "cache should live in the system temporary directory");

    strcpy(first, g_cache_dir);

    /* Two projects that both have a foo_test.c must not share a binary. */
    assert_true(ensure_cache_dir("./src", NULL),
                "a second directory should get a cache too");

    assert_false(strcmp(first, g_cache_dir) == 0,
                 "two test directories should not share a cache");

    /* And the same one has to come back to the same place, or nothing would
       ever be found again and the cache would never hit. */
    assert_true(ensure_cache_dir(".", NULL),
                "the first directory should still work");

    assert_equal(strcmp(first, g_cache_dir), 0,
                 "the same test directory should get the same cache");
}

void test_cache_dir_override()
{
    assert_true(ensure_cache_dir(".", "assertx_cache_test"),
                "--cache-dir should be used when it is given");

    assert_equal(strncmp(g_cache_dir, "assertx_cache_test",
                         strlen("assertx_cache_test")), 0,
                 "the named directory should be where the cache goes");

#ifdef _WIN32
    _rmdir(g_cache_dir);
    _rmdir("assertx_cache_test");
#else
    rmdir(g_cache_dir);
    rmdir("assertx_cache_test");
#endif
}

/* =========================
   extract_test_functions
========================= */

void test_extract_test_functions()
{
    const char *fake_file = "temp_test_file.c";

    FILE *f = fopen(fake_file, "w");
    fprintf(f,
            "void test_one(){}\n"
            "void test_two(){}\n"
            "void not_a_test(){}\n");
    fclose(f);

    FILE *runner = fopen("temp_runner.c", "w");

    char functions[100][256];
    int count = extract_test_functions(fake_file, runner, functions);

    fclose(runner);

    assert_equal(count, 2,
                 "Should extract 2 test_ functions");

    assert_equal(strcmp(functions[0], "test_one"), 0,
                 "First function should be test_one");

    assert_equal(strcmp(functions[1], "test_two"), 0,
                 "Second function should be test_two");

    remove(fake_file);
    remove("temp_runner.c");
}

/* =========================
   run_test_file
========================= */

void test_run_test_file_invalid()
{
    int total = 0;
    int passed = 0;

    run_test_file(".", "notatestfile.c", &total, &passed);

    assert_equal(total, 0,
                 "Non _test.c file should not increment total");
}

/* =========================
   --uses
========================= */

void test_parse_uses()
{
    char *argv[] = {"assertx", "./tests", "--uses", "sum", "div", "-j", "2", NULL};
    assertx_settings s = default_settings();

    assert_true(parse_settings(7, argv, &s), "--uses sum div -j 2 parses");
    assert_equal(s.uses_count, 2, "two function names were given");
    assert_equal(strcmp(s.uses[0], "sum"), 0, "the first is sum");
    assert_equal(strcmp(s.uses[1], "div"), 0, "the second is div");
    assert_equal(s.parallel, 2, "the option after the names still applies");
}

void test_parse_uses_rejects()
{
    char *bad_name[] = {"assertx", "./tests", "--uses", "sum;id", NULL};
    char *no_names[] = {"assertx", "./tests", "--uses", NULL};
    char *option_next[] = {"assertx", "./tests", "--uses", "-j", "2", NULL};
    assertx_settings s = default_settings();

    assert_false(parse_settings(4, bad_name, &s),
                 "a name that is not a C identifier is turned away");

    s = default_settings();
    assert_false(parse_settings(3, no_names, &s), "--uses needs a name");

    s = default_settings();
    assert_false(parse_settings(5, option_next, &s),
                 "--uses followed by another option has no names");
}

/* Builds and runs a real test file. test_other fails on purpose, so the run
   only passes when --uses really keeps it from running — which shows the
   runner obeys the tests it is given, and that render reaching draw from a
   header is enough to pick test_render. */
void test_uses_runs_only_picked_tests()
{
    char dir[ASSERTX_PATH_MAX];
    char tmp[ASSERTX_PATH_MAX];
    char path[ASSERTX_PATH_MAX * 2];
    FILE *f;
    assertx_settings s;
    int total = 0;
    int passed = 0;

    temp_dir(tmp, sizeof(tmp));
    snprintf(dir, sizeof(dir), "%s%sassertx_uses_e2e", tmp, PATH_SEP);

    /* Strict: this directory's files are compiled and run, so in a shared
       /tmp it has to be ours alone, not one somebody else made first. */
    if (!make_dir(dir, 1))
    {
        assert_true(0, "the scratch directory should be ours alone");
        return;
    }

    snprintf(path, sizeof(path), "%s%slib.h", dir, PATH_SEP);
    f = fopen(path, "w");
    fprintf(f,
            "static int draw(void) { return 1; }\n"
            "static int render(void) { return draw(); }\n");
    fclose(f);

    snprintf(path, sizeof(path), "%s%spick_test.c", dir, PATH_SEP);
    f = fopen(path, "w");
    fprintf(f,
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include \"lib.h\"\n"
            "static void test_summary(void) { exit(0); }\n"
            "void test_other() { exit(1); }\n"
            "void test_render() { if (!render()) exit(1); }\n");
    fclose(f);

    s = default_settings();
    s.parallel = 1;
    s.uses[0] = "draw";
    s.uses_count = 1;

    run_test_file_with(dir, "pick_test.c", &s, &total, &passed);

    assert_equal(total, 1, "the file has a test that uses draw");
    assert_equal(passed, 1, "only test_render ran, so the file passed");

    total = passed = 0;
    s.uses[0] = "nothing_calls_this";
    run_test_file_with(dir, "pick_test.c", &s, &total, &passed);

    assert_equal(total, 0, "a file with no test using the name is skipped");

    total = passed = 0;
    s.uses_count = 0;
    run_test_file_with(dir, "pick_test.c", &s, &total, &passed);

    assert_equal(passed, 0, "without --uses test_other runs and fails");

    remove(path);
    snprintf(path, sizeof(path), "%s%slib.h", dir, PATH_SEP);
    remove(path);
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}
