#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include <sys/stat.h>

#define PATH_SEP "/"

#define fopen_safe(fp, path, mode) ((fp) = fopen(path, mode)) == NULL
#define sscanf_safe sscanf
#define strncpy_safe(dest, destsz, src) strncpy(dest, src, destsz)

#endif

#define BUILD_DIR "build"
#define COMPILER "gcc"
#define CFLAGS "-Wall -Wextra -g"


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
    printf("  %s <test_directory>\n", program);
    printf("\n");

    printf("Example:\n");
    printf("  %s ./tests\n", program);
    printf("\n");

    printf("Description:\n");
    printf("  Automatically finds and runs all *_test.c files\n");
    printf("  inside the specified directory.\n");
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

void ensure_build_dir()
{
#ifdef _WIN32
    _mkdir(BUILD_DIR);
#else
    struct stat st = {0};
    if (stat(BUILD_DIR, &st) == -1)
        mkdir(BUILD_DIR, 0700);
#endif
}


/* =========================
   EXTRACT TEST FUNCTIONS
========================= */

int extract_test_functions(const char *source_path, FILE *runner_file, char functions[][256])
{
    FILE *src;

    if (fopen_safe(src, source_path, "r"))
        return 0;

    char line[512];
    int count = 0;

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
                if (count < 100)
                {
                    strncpy_safe(functions[count], sizeof(functions[count]), func_name);

#ifndef _WIN32
                    functions[count][sizeof(functions[count]) - 1] = '\0';
#endif

                    fprintf(runner_file, "void %s();\n", functions[count]);

                    count++;
                }
            }
        }
    }

    fclose(src);

    return count;
}


/* =========================
   RUN TEST FILE
========================= */

void run_test_file(const char *dir_path, const char *filename,
                   int *total, int *passed)
{
    if (!ends_with(filename, "_test.c"))
        return;

    (*total)++;

    char source_path[512];
    char binary_path[512];
    char runner_path[512];

    snprintf(source_path, sizeof(source_path),
             "%s%s%s", dir_path, PATH_SEP, filename);

    char test_name[256];
    size_t name_len = strlen(filename);

    if (name_len < 3)
        return;

    memcpy(test_name, filename, name_len - 2);
    test_name[name_len - 2] = '\0';

#ifdef _WIN32
    snprintf(binary_path, sizeof(binary_path),
             "%s%s%s.exe", BUILD_DIR, PATH_SEP, test_name);
#else
    snprintf(binary_path, sizeof(binary_path),
             "%s%s%s", BUILD_DIR, PATH_SEP, test_name);
#endif

    snprintf(runner_path, sizeof(runner_path),
             "%s%s__runner_%s.c", BUILD_DIR, PATH_SEP, test_name);

    FILE *runner;

    if (fopen_safe(runner, runner_path, "w"))
    {
        printf("❌ Failed to create runner for %s\n", filename);
        return;
    }

    fprintf(runner, "#include \"../%s\"\n\n", source_path);

    char functions[100][256];

    int test_count = extract_test_functions(source_path, runner, functions);

    if (test_count == 0)
    {
        printf("⚠️ No test_ functions found in %s\n\n", filename);

        fclose(runner);
        remove(runner_path);

        return;
    }

    fprintf(runner, "\nint main() {\n");
    fprintf(runner, "    printf(\"Running %d tests...\\n\");\n", test_count);

    for (int i = 0; i < test_count; i++)
    {
        fprintf(runner, "    printf(\"→ %s\\n\");\n", functions[i]);
        fprintf(runner, "    %s();\n", functions[i]);
    }

    fprintf(runner, "    test_summary();\n");
    fprintf(runner, "    return 0;\n");
    fprintf(runner, "}\n");

    fclose(runner);

    printf("🔨 Compiling %s...\n", filename);

    int needed = snprintf(NULL, 0,
                          "%s %s \"%s\" -o \"%s\"",
                          COMPILER, CFLAGS, runner_path, binary_path);

    char *compile_cmd = malloc((size_t)needed + 1);

    if (!compile_cmd)
    {
        printf("❌ Memory allocation failed\n\n");
        remove(runner_path);
        return;
    }

    snprintf(compile_cmd, (size_t)needed + 1,
             "%s %s \"%s\" -o \"%s\"",
             COMPILER, CFLAGS, runner_path, binary_path);

    int compile_result = system(compile_cmd);

    free(compile_cmd);

    if (compile_result != 0)
    {
        printf("❌ Compile failed: %s\n\n", filename);
        remove(runner_path);
        return;
    }

    printf("▶️ Running %s...\n", filename);

    if (system(binary_path) == 0)
    {
        printf("✅ Passed: %s\n\n", filename);
        (*passed)++;
    }
    else
    {
        printf("❌ Failed: %s\n\n", filename);
    }

    remove(runner_path);
    remove(binary_path);
}


/* =========================
   MAIN
========================= */

int main(int argc, char *argv[])
{
    /* SHOW HELP IF NO PARAMETER */
    if (argc < 2)
    {
        show_help(argv[0]);
        return 0;
    }

    const char *dir_path = argv[1];

    ensure_build_dir();

    int total = 0;
    int passed = 0;

    printf("🔎 Searching tests in %s\n\n", dir_path);

#ifdef _WIN32

    char search_path[512];

    snprintf(search_path, sizeof(search_path),
             "%s\\*.*", dir_path);

    WIN32_FIND_DATA find_data;

    HANDLE hFind = FindFirstFile(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        perror("Failed to open directory");
        return 1;
    }

    do
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            run_test_file(dir_path, find_data.cFileName, &total, &passed);
        }

    } while (FindNextFile(hFind, &find_data));

    FindClose(hFind);

#else

    DIR *dir = opendir(dir_path);

    if (!dir)
    {
        perror("Failed to open directory");
        return 1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        run_test_file(dir_path, entry->d_name, &total, &passed);
    }

    closedir(dir);

#endif

    printf("====================================\n");
    printf("Tests: %d | Passed: %d | Failed: %d\n",
           total, passed, total - passed);

    return (passed == total) ? 0 : 1;
}
