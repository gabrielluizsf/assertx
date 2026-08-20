#include "./src/assertx_runner.h"

/* A run happens in two passes rather than one.
 *
 * The old shape was: find a test, compile it, run it, on to the next. Simple,
 * but compiling is nearly all of the wall clock time and the compiles have
 * nothing to do with each other, so the machine spent the run with one core
 * busy and the rest idle.
 *
 * Now every test file is collected first, the ones that actually need
 * compiling are built together, and then they are run in order. Running stays
 * one at a time: it costs almost nothing, and interleaved results would be
 * unreadable.
 */

int main(int argc, char *argv[])
{
    static test_job jobs[MAX_TEST_FILES];
    int job_count = 0;
    int total = 0;
    int passed = 0;
    int to_build = 0;
    int i;
    assertx_settings settings;
    const char *dir_path;

    if (argc < 2 ||
        strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "--help") == 0)
    {
        show_help(argv[0]);
        return 0;
    }

    dir_path = argv[1];
    settings = default_settings();

    if (!parse_settings(argc, argv, &settings))
        return 1;

    if (!ensure_cache_dir(dir_path, settings.cache_dir))
        return 1;

    printf("🔎 Searching tests in %s\n", dir_path);
    printf("📦 Cache: %s\n\n", g_cache_dir);

    /* ---- pass one: find the tests and work out what has changed ---- */

#ifdef _WIN32
    {
        char search_path[512];
        WIN32_FIND_DATA find_data;
        HANDLE hFind;

        snprintf(search_path, sizeof(search_path), "%s\\*.*", dir_path);

        hFind = FindFirstFile(search_path, &find_data);

        if (hFind == INVALID_HANDLE_VALUE)
        {
            perror("Failed to open directory");
            return 1;
        }

        do
        {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                ends_with(find_data.cFileName, "_test.c") &&
                job_count < MAX_TEST_FILES)
            {
                if (prepare_job(&jobs[job_count], dir_path, find_data.cFileName,
                                &settings))
                    job_count++;
                else
                    printf("⚠️ No test_ functions found in %s\n\n",
                           find_data.cFileName);
            }

        } while (FindNextFile(hFind, &find_data));

        FindClose(hFind);
    }
#else
    {
        DIR *dir = opendir(dir_path);
        struct dirent *entry;

        if (!dir)
        {
            perror("Failed to open directory");
            return 1;
        }

        while ((entry = readdir(dir)) != NULL)
        {
            if (!ends_with(entry->d_name, "_test.c"))
                continue;

            if (job_count >= MAX_TEST_FILES)
                break;

            if (prepare_job(&jobs[job_count], dir_path, entry->d_name, &settings))
                job_count++;
            else
                printf("⚠️ No test_ functions found in %s\n\n", entry->d_name);
        }

        closedir(dir);
    }
#endif

    if (job_count == 0)
    {
        printf("No tests found in %s\n", dir_path);
        return 0;
    }

    for (i = 0; i < job_count; i++)
        if (jobs[i].needs_build)
            to_build++;

    /* ---- pass two: build what needs it, then run everything ---- */

    if (to_build == 0)
    {
        printf("🔨 Nothing to compile, %d test file%s already up to date\n\n",
               job_count, job_count == 1 ? "" : "s");
    }
    else
    {
        printf("🔨 Compiling %d of %d test file%s", to_build, job_count,
               job_count == 1 ? "" : "s");

        if (settings.parallel > 1 && to_build > 1)
            printf(" (%d at a time)", settings.parallel);

        printf("\n\n");
    }

    build_all(jobs, job_count, &settings);
    run_jobs(jobs, job_count, &total, &passed);

    printf("====================================\n");
    printf("Tests: %d | Passed: %d | Failed: %d\n",
           total, passed, total - passed);

    return (passed == total) ? 0 : 1;
}
