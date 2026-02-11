#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUILD_DIR "build"
#define COMPILER "gcc"
#define CFLAGS "-Wall -Wextra -g"

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
    struct stat st = {0};
    if (stat(BUILD_DIR, &st) == -1)
    {
        mkdir(BUILD_DIR, 0700);
    }
}

/*
 * Extracts functions of the form:
 * - void test_function_name()
 * and writes the prototypes to the runner.
 */
int extract_test_functions(const char *source_path, FILE *runner_file, char functions[][256])
{
    FILE *src = fopen(source_path, "r");
    if (!src)
        return 0;

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), src))
    {
        if (strstr(line, "void test_") != NULL)
        {
            char func_name[256];
            if (sscanf(line, "void %255[^ (]", func_name) == 1)
            {
                if (count < 100)
                {
                    strncpy(functions[count], func_name, sizeof(functions[count]) - 1);
                    functions[count][sizeof(functions[count]) - 1] = '\0';

                    fprintf(runner_file, "void %s();\n", func_name);
                    count++;
                }
            }
        }
    }

    fclose(src);
    return count;
}

int main(int argc, char *argv[])
{
    const char *dir_path = "./src";

    if (argc > 1)
        dir_path = argv[1];

    DIR *dir = opendir(dir_path);
    if (!dir)
    {
        perror("Failed to open directory");
        return 1;
    }

    ensure_build_dir();

    struct dirent *entry;
    int total = 0;
    int passed = 0;

    printf("🔎 Searching tests in %s\n\n", dir_path);

    while ((entry = readdir(dir)) != NULL)
    {
        if (!ends_with(entry->d_name, "_test.c"))
            continue;

        total++;

        char source_path[512];
        char binary_path[512];
        char runner_path[512];

        if (snprintf(source_path, sizeof(source_path), "%s/%s",
                     dir_path, entry->d_name) >= sizeof(source_path))
        {
            printf("❌ Path too long\n");
            continue;
        }

        char test_name[256];
        size_t name_len = strlen(entry->d_name);

        if (name_len <= 2 || name_len - 2 >= sizeof(test_name))
        {
            printf("❌ Invalid test name\n");
            continue;
        }

        memcpy(test_name, entry->d_name, name_len - 2);
        test_name[name_len - 2] = '\0';

        if (snprintf(binary_path, sizeof(binary_path), "%s/%s",
                     BUILD_DIR, test_name) >= sizeof(binary_path))
        {
            printf("❌ Binary path too long\n");
            continue;
        }

        if (snprintf(runner_path, sizeof(runner_path), "%s/__runner_%s.c",
                     BUILD_DIR, test_name) >= sizeof(runner_path))
        {
            printf("❌ Runner path too long\n");
            continue;
        }

        FILE *runner = fopen(runner_path, "w");
        if (!runner)
        {
            printf("❌ Failed to create runner for %s\n", entry->d_name);
            continue;
        }

        fprintf(runner, "#include \"../%s\"\n\n", source_path);

        char functions[100][256];
        int test_count = extract_test_functions(source_path, runner, functions);

        if (test_count == 0)
        {
            printf("⚠️ No test_ functions found in %s\n\n", entry->d_name);
            fclose(runner);
            continue;
        }

        fprintf(runner, "\nint main() {\n");
        fprintf(runner, "    printf(\"Running %d tests...\\n\");\n", test_count);

        for (int i = 0; i < test_count; i++)
        {
            fprintf(runner, "    printf(\"→ %s\\n\");\n", functions[i]);
            fprintf(runner, "    %s();\n", functions[i]);
        }

        fprintf(runner, "    return 0;\n");
        fprintf(runner, "}\n");

        fclose(runner);

        printf("🔨 Compiling %s...\n", entry->d_name);

        int needed = snprintf(NULL, 0,
                              "%s %s %s -o %s",
                              COMPILER, CFLAGS, runner_path, binary_path);

        if (needed < 0)
        {
            printf("❌ Failed to calculate compile command size\n");
            continue;
        }

        char *compile_cmd = malloc(needed + 1);
        if (!compile_cmd)
        {
            printf("❌ Memory allocation failed\n");
            continue;
        }

        snprintf(compile_cmd, needed + 1,
                 "%s %s %s -o %s",
                 COMPILER, CFLAGS, runner_path, binary_path);

        if (system(compile_cmd) != 0)
        {
            printf("❌ Compile failed: %s\n\n", entry->d_name);
            free(compile_cmd);
            continue;
        }

        free(compile_cmd);

        printf("▶️ Running %s...\n", entry->d_name);

        if (system(binary_path) == 0)
        {
            printf("✅ Passed: %s\n\n", entry->d_name);
            passed++;
        }
        else
        {
            printf("❌ Failed: %s\n\n", entry->d_name);
        }

        if (remove(runner_path) != 0)
            perror("Failed to remove runner");

        if (remove(binary_path) != 0)
            perror("Failed to remove binary");
    }

    closedir(dir);

    printf("====================================\n");
    printf("Tests: %d | Passed: %d | Failed: %d\n",
           total, passed, total - passed);

    return (passed == total) ? 0 : 1;
}
