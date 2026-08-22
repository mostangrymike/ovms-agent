static int execute_save_build_to(
    const char *path,
    const char *build_output)
{
    FILE *file;
    int success;

    if (path == NULL ||
        *path == '\0' ||
        build_output == NULL) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fputs(build_output, file) != EOF;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}
