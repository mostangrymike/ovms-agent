#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "project.h"

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

void project_list(const agent_state *state)
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
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            (void)printf("%s\n", entry->d_name);
        }
    }

    if (closedir(directory) != 0) {
        (void)printf("Warning: unable to close directory: %s\n",
                     strerror(errno));
    }
}
