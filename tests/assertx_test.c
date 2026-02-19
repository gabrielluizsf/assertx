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
   ensure_build_dir
========================= */

void test_ensure_build_dir()
{
    ensure_build_dir();

#ifdef _WIN32
    assert_true(_access(BUILD_DIR, 0) == 0,
                "build directory should exist (windows)");
#else
    assert_true(access(BUILD_DIR, F_OK) == 0,
                "build directory should exist (unix)");
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
