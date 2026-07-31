#include "openai_internal.h"

int openai_confirm_restore(const char *path)
{
    char answer[32];

    (void)printf(
        "Build failed after patching %s.\n"
        "Restore the previous OpenVMS file version [y/N]? ",
        path
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return 0;
    }

    return answer[0] == 'y' || answer[0] == 'Y';
}

int openai_restore_previous_version(const char *path)
{
    char previous_path[1024];
    char destination_path[1024];
    unsigned char buffer[8192];
    FILE *source;
    FILE *destination;
    size_t count;
    int written;

    if (path == NULL || *path == '\0') {
        (void)puts("Unable to restore an empty path.");
        return 0;
    }

    written = snprintf(
        previous_path,
        sizeof(previous_path),
        "%s;-1",
        path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(previous_path)) {
        (void)puts("Previous-version path is too long.");
        return 0;
    }

    /*
     * A trailing semicolon requests a new highest OpenVMS file version.
     * Without it, RMS may try to reuse the source version number.
     */
    written = snprintf(
        destination_path,
        sizeof(destination_path),
        "%s;",
        path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(destination_path)) {
        (void)puts("Restore destination path is too long.");
        return 0;
    }

    source = fopen(previous_path, "rb");

    if (source == NULL) {
        (void)printf(
            "Unable to open previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        return 0;
    }

    destination = fopen(destination_path, "wb");

    if (destination == NULL) {
        (void)printf(
            "Unable to create restored version %s: %s\n",
            destination_path,
            strerror(errno)
        );
        (void)fclose(source);
        return 0;
    }

    while ((count = fread(
                buffer,
                1U,
                sizeof(buffer),
                source)) > 0U) {
        if (fwrite(buffer, 1U, count, destination) != count) {
            (void)printf(
                "Unable to write restored version %s: %s\n",
                destination_path,
                strerror(errno)
            );
            (void)fclose(source);
            (void)fclose(destination);
            return 0;
        }
    }

    if (ferror(source)) {
        (void)printf(
            "Unable to read previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        (void)fclose(source);
        (void)fclose(destination);
        return 0;
    }

    if (fclose(source) != 0) {
        (void)printf(
            "Unable to close previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        (void)fclose(destination);
        return 0;
    }

    if (fclose(destination) != 0) {
        (void)printf(
            "Unable to close restored version %s: %s\n",
            destination_path,
            strerror(errno)
        );
        return 0;
    }

    (void)puts(
        "Previous file contents restored as a new OpenVMS version."
    );
    return 1;
}
