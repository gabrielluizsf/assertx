#include "./src/assertx_runner.h"

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
