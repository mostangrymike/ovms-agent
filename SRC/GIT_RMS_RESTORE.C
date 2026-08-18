#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git_rms_restore.h"
#include "rms_write.h"

#define GIT_RMS_CMD_FILE "OVMS_AGENT_GIT_RESTORE.COM"
#define GIT_RMS_HEAD_FILE "OVMS_AGENT_GIT_HEAD.TMP"
#define GIT_RMS_MAX_TEXT 65536U

static void git_rms_remove_all(const char *path)
{
    if (path == NULL) {
        return;
    }

    while (remove(path) == 0) {
    }
}

static int git_rms_path_safe(const char *path)
{
    const unsigned char *cursor;

    if (path == NULL || *path == '\0' || path[0] == '-' ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    cursor = (const unsigned char *)path;
    while (*cursor != '\0') {
        if (!((*cursor >= (unsigned char)'A' &&
               *cursor <= (unsigned char)'Z') ||
              (*cursor >= (unsigned char)'a' &&
               *cursor <= (unsigned char)'z') ||
              (*cursor >= (unsigned char)'0' &&
               *cursor <= (unsigned char)'9') ||
              *cursor == (unsigned char)'_' ||
              *cursor == (unsigned char)'-' ||
              *cursor == (unsigned char)'.' ||
              *cursor == (unsigned char)'/' ||
              *cursor == (unsigned char)'$')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static char *git_rms_read_text(const char *path)
{
    FILE *file;
    char *text;
    size_t used;
    int ch;

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    text = (char *)malloc(GIT_RMS_MAX_TEXT + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    used = 0U;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\r') {
            continue;
        }

        if (used >= GIT_RMS_MAX_TEXT) {
            free(text);
            (void)fclose(file);
            return NULL;
        }

        text[used++] = (char)ch;
    }

    if (ferror(file)) {
        free(text);
        (void)fclose(file);
        return NULL;
    }

    if (fclose(file) != 0) {
        free(text);
        return NULL;
    }

    text[used] = '\0';
    return text;
}

static char *git_rms_capture_head(const char *path)
{
    FILE *command;
    char dcl[256];
    int status;
    char *text;

    if (!git_rms_path_safe(path)) {
        return NULL;
    }

    git_rms_remove_all(GIT_RMS_CMD_FILE);
    git_rms_remove_all(GIT_RMS_HEAD_FILE);

    command = fopen(GIT_RMS_CMD_FILE, "w");
    if (command == NULL) {
        return NULL;
    }

    if (fprintf(command,
            "$ SET NOON\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"show\" \"HEAD:%s\"\n"
            "$ EXIT $STATUS\n",
            GIT_RMS_HEAD_FILE,
            path) < 0 ||
        fclose(command) != 0) {
        git_rms_remove_all(GIT_RMS_CMD_FILE);
        return NULL;
    }

    (void)snprintf(dcl, sizeof(dcl), "@%s", GIT_RMS_CMD_FILE);
    status = system(dcl);
    git_rms_remove_all(GIT_RMS_CMD_FILE);

    if ((status & 1) == 0) {
        git_rms_remove_all(GIT_RMS_HEAD_FILE);
        return NULL;
    }

    text = git_rms_read_text(GIT_RMS_HEAD_FILE);
    git_rms_remove_all(GIT_RMS_HEAD_FILE);
    return text;
}

int git_rms_restore_head(const char *path)
{
    char *head_text;
    char *current_text;
    int ok;

    if (!git_rms_path_safe(path)) {
        return 0;
    }

    head_text = git_rms_capture_head(path);
    if (head_text == NULL) {
        return 0;
    }

    ok = rms_replace_text_file(path, head_text);
    if (!ok) {
        free(head_text);
        return 0;
    }

    current_text = git_rms_read_text(path);
    if (current_text == NULL) {
        free(head_text);
        return 0;
    }

    ok = strcmp(current_text, head_text) == 0;
    free(current_text);
    free(head_text);
    return ok;
}
