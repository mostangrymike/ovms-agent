#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M304_PROFILE_PATH_MAX 256U

static FILE *m304_profile_fopen(const char *path,
                                const char *mode)
{
    FILE *file;
    const char *directory;
    const char *name;
    size_t length;
    int written;
    char resolved[M304_PROFILE_PATH_MAX];

    if (path == NULL || mode == NULL) {
        return NULL;
    }

    file = fopen(path, mode);
    if (file != NULL || strcmp(mode, "r") != 0) {
        return file;
    }

    if (strncmp(path, "[.LANGUAGE]", 12U) == 0) {
        name = path + 12;
    } else if (strncmp(path, "LANGUAGE/", 9U) == 0) {
        name = path + 9;
    } else {
        return NULL;
    }

    if (*name == '\0' || strchr(name, '/') != NULL ||
        strchr(name, ']') != NULL || strchr(name, '>') != NULL) {
        return NULL;
    }

    directory = getenv("OVMS_AGENT_LANGUAGE_DIR");
    if (directory == NULL || *directory == '\0') {
        return NULL;
    }

    length = strlen(directory);
    if (length > 0U &&
        (directory[length - 1U] == ']' ||
         directory[length - 1U] == '>' ||
         directory[length - 1U] == ':' ||
         directory[length - 1U] == '/')) {
        written = snprintf(resolved, sizeof(resolved),
                           "%s%s", directory, name);
    } else {
        written = snprintf(resolved, sizeof(resolved),
                           "%s/%s", directory, name);
    }

    if (written < 0 || (size_t)written >= sizeof(resolved)) {
        return NULL;
    }

    return fopen(resolved, mode);
}

#define fopen m304_profile_fopen
#include "M289_BUILD_PROFILE.C"
#undef fopen
