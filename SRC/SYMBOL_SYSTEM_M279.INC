/* M279 layout-aware refactor object invalidation. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int m279_obj_delete_cmd(const char *command,
                               char *fallback,
                               size_t fallback_size)
{
    static const char prefix[] = "DELETE [.BUILD]";
    static const char suffix[] = ".OBJ;*";
    const char *name;
    const char *end;
    size_t length;

    if (command == NULL || fallback == NULL || fallback_size == 0U ||
        strncmp(command, prefix, sizeof(prefix) - 1U) != 0) {
        return 0;
    }

    name = command + sizeof(prefix) - 1U;
    end = strstr(name, suffix);
    if (end == NULL || end[sizeof(suffix) - 1U] != '\0') {
        return 0;
    }

    length = (size_t)(end - name);
    if (length == 0U || length + 16U >= fallback_size) {
        return 0;
    }

    if (snprintf(fallback, fallback_size,
                 "DELETE []%.*s.OBJ;*",
                 (int)length, name) < 0) {
        fallback[0] = '\0';
        return 0;
    }

    return 1;
}

static int m279_obj_exists(const char *path)
{
    struct stat status;

    errno = 0;
    return path != NULL && stat(path, &status) == 0;
}

static int m279_symbol_system(const char *command)
{
    char fallback[512];
    char object_name[256];
    char build_path[512];
    char root_path[512];
    const char *name;
    const char *end;
    size_t length;
    int status;

    fallback[0] = '\0';
    if (!m279_obj_delete_cmd(command, fallback, sizeof(fallback))) {
        return system(command);
    }

    name = command + strlen("DELETE [.BUILD]");
    end = strstr(name, ".OBJ;*");
    if (end == NULL) {
        return system(command);
    }

    length = (size_t)(end - name);
    if (length == 0U || length >= sizeof(object_name)) {
        return system(command);
    }

    (void)memcpy(object_name, name, length);
    object_name[length] = '\0';

    if (snprintf(build_path, sizeof(build_path),
                 "BUILD/%s.OBJ", object_name) < 0 ||
        snprintf(root_path, sizeof(root_path),
                 "%s.OBJ", object_name) < 0) {
        return system(command);
    }

    if (m279_obj_exists(build_path)) {
        return system(command);
    }

    if (m279_obj_exists(root_path)) {
        status = system(fallback);
        return status;
    }

    /* No object exists in either supported layout: already invalidated. */
    return 0;
}

/* Keep libc system() visible inside the wrapper above, then redirect callers. */
#define system m279_symbol_system
