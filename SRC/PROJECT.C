#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "project.h"

#define READ_BUFFER_SIZE 1024

static int path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (strstr(path, "..") != NULL) {
        return 0;
    }

    if (strchr(path, ':') != NULL) {
        return 0;
    }

    if (*path == '/') {
        return 0;
    }

    if (strchr(path, '[') != NULL &&
        strncmp(path, "[.", 2U) != 0) {
        return 0;
    }

    return 1;
}

static int is_old_version(const char *name)
{
    const char *semicolon;
    const char *position;

    semicolon = strrchr(name, ';');

    if (semicolon == NULL || semicolon[1] == '\0') {
        return 0;
    }

    position = semicolon + 1;

    while (*position != '\0') {
        if (!isdigit((unsigned char)*position)) {
            return 0;
        }
        ++position;
    }

    return strcmp(semicolon, ";1") != 0;
}

static int hide_default_entry(const char *name)
{
    if (strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 1;
    }

    if (strncmp(name, "^.git", 5U) == 0) {
        return 1;
    }

    if (is_old_version(name)) {
        return 1;
    }

    return 0;
}

void project_show_root(const agent_state *state)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)printf("Project root: %s\n", state->project_root);
}

void project_show_status(const agent_state *state)
{
    if (state == NULL) {
        (void)puts("Agent state is unavailable.");
        return;
    }

    (void)printf("Version:       %s\n", OVMS_AGENT_VERSION);
    (void)printf("Project root:  %s\n",
        state->project_root != NULL &&
        *state->project_root != '\0'
            ? state->project_root
            : "not defined");
    (void)printf("API key:       %s\n",
        state->api_key_defined ? "defined" : "not defined");
    (void)printf("Write access:  %s\n",
        state->write_enabled ? "enabled" : "disabled");
    (void)printf("DCL execution: %s\n",
        state->dcl_enabled ? "enabled" : "disabled");
}

void project_list(const agent_state *state, int show_all)
{
    DIR *directory;
    struct dirent *entry;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    directory = opendir(state->project_root);

    if (directory == NULL) {
        (void)printf("Unable to open project root: %s\n",
                     state->project_root);
        (void)printf("Reason: %s\n", strerror(errno));
        return;
    }

    (void)printf("Directory: %s\n\n", state->project_root);

    while ((entry = readdir(directory)) != NULL) {
        if (show_all || !hide_default_entry(entry->d_name)) {
            (void)printf("%s\n", entry->d_name);
        }
    }

    if (closedir(directory) != 0) {
        (void)printf("Warning: unable to close directory: %s\n",
                     strerror(errno));
    }
}

void project_read(const agent_state *state, const char *path)
{
    FILE *file;
    char buffer[READ_BUFFER_SIZE];
    unsigned long line_number;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!path_is_safe(path)) {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        (void)printf("Unable to read %s\n", path);
        (void)printf("Reason: %s\n", strerror(errno));
        return;
    }

    line_number = 1UL;

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        (void)printf("%6lu  %s", line_number, buffer);

        if (strchr(buffer, '\n') == NULL) {
            (void)putchar('\n');
        }

        ++line_number;
    }

    if (ferror(file)) {
        (void)printf("Read error: %s\n", strerror(errno));
    }

    if (fclose(file) != 0) {
        (void)printf("Warning: unable to close file: %s\n",
                     strerror(errno));
    }
}
