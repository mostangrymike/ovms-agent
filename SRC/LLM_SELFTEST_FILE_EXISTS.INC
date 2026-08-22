static int llm_selftest_file_exists(const char *path)
{
    FILE *file;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}
