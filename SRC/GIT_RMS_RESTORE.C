#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git_rms_restore.h"
#include "rms_write.h"

#define GIT_RMS_CMD_FILE "SYS$LOGIN:OVMS_AGENT_GIT_RESTORE.COM"
#define GIT_RMS_HEAD_FILE "SYS$LOGIN:OVMS_AGENT_GIT_HEAD.TMP"
#define GIT_RMS_SIZE_FILE "SYS$LOGIN:OVMS_AGENT_GIT_SIZE.TMP"
#define GIT_RMS_PREFIX_FILE "SYS$LOGIN:OVMS_AGENT_GIT_PREFIX.TMP"
#define GIT_RMS_CWD_FILE "OVMS_AGENT_GIT_CWD.TMP"
#define GIT_RMS_MAX_TEXT 262144U
#define GIT_RMS_PATH_MAX 1024U

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

static int git_rms_read_size(
    const char *path,
    size_t *size_out)
{
    FILE *file;
    unsigned long value;
    char extra;

    if (path == NULL || size_out == NULL) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    if (fscanf(file, "%lu %c", &value, &extra) != 1 ||
        fclose(file) != 0 ||
        value > GIT_RMS_MAX_TEXT) {
        return 0;
    }

    *size_out = (size_t)value;
    return 1;
}

static int git_rms_current_default(char *output,
                                   size_t output_size)
{
    FILE *probe;
    char full[GIT_RMS_PATH_MAX];
    char *end;
    size_t length;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    git_rms_remove_all(GIT_RMS_CWD_FILE);
    probe = fopen(GIT_RMS_CWD_FILE, "w");
    if (probe == NULL) {
        return 0;
    }

    if (fgetname(probe, full, 1) == NULL) {
        (void)fclose(probe);
        git_rms_remove_all(GIT_RMS_CWD_FILE);
        return 0;
    }

    if (fclose(probe) != 0) {
        git_rms_remove_all(full);
        return 0;
    }
    git_rms_remove_all(full);

    end = strrchr(full, ']');
    if (end == NULL) {
        return 0;
    }

    length = (size_t)(end - full) + 1U;
    if (length >= output_size) {
        return 0;
    }

    (void)memcpy(output, full, length);
    output[length] = '\0';
    return 1;
}

static int git_rms_capture_prefix(char *prefix,
                                  size_t prefix_size)
{
    FILE *command;
    char dcl[256];
    char current[GIT_RMS_PATH_MAX];
    char *text;
    size_t length;
    int status;

    if (prefix == NULL || prefix_size == 0U ||
        !git_rms_current_default(current, sizeof(current))) {
        return 0;
    }

    git_rms_remove_all(GIT_RMS_CMD_FILE);
    git_rms_remove_all(GIT_RMS_PREFIX_FILE);

    command = fopen(GIT_RMS_CMD_FILE, "w");
    if (command == NULL) {
        return 0;
    }

    if (fprintf(command,
            "$ SET NOON\n"
            "$ SET DEFAULT %s\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"rev-parse\" \"--show-prefix\"\n"
            "$ EXIT $STATUS\n",
            current,
            GIT_RMS_PREFIX_FILE) < 0 ||
        fclose(command) != 0) {
        git_rms_remove_all(GIT_RMS_CMD_FILE);
        return 0;
    }

    (void)snprintf(dcl, sizeof(dcl), "@%s", GIT_RMS_CMD_FILE);
    status = system(dcl);
    git_rms_remove_all(GIT_RMS_CMD_FILE);

    if ((status & 1) == 0) {
        git_rms_remove_all(GIT_RMS_PREFIX_FILE);
        return 0;
    }

    text = git_rms_read_text(GIT_RMS_PREFIX_FILE);
    git_rms_remove_all(GIT_RMS_PREFIX_FILE);
    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\n' ||
            text[length - 1U] == '\r')) {
        text[--length] = '\0';
    }

    if (length >= prefix_size) {
        free(text);
        return 0;
    }

    (void)memcpy(prefix, text, length + 1U);
    free(text);
    return 1;
}

static int git_rms_make_git_path(const char *path,
                                 char *git_path,
                                 size_t git_path_size)
{
    char prefix[GIT_RMS_PATH_MAX];
    size_t prefix_length;
    size_t path_length;

    if (!git_rms_path_safe(path) ||
        git_path == NULL || git_path_size == 0U ||
        !git_rms_capture_prefix(prefix, sizeof(prefix))) {
        return 0;
    }

    prefix_length = strlen(prefix);
    path_length = strlen(path);
    if (prefix_length + path_length + 1U > git_path_size) {
        return 0;
    }

    (void)memcpy(git_path, prefix, prefix_length);
    (void)memcpy(git_path + prefix_length, path, path_length + 1U);

    return git_rms_path_safe(git_path);
}

static char *git_rms_capture_head(const char *path)
{
    FILE *command;
    char dcl[256];
    int status;
    char *text;
    size_t blob_size;

    if (!git_rms_path_safe(path)) {
        return NULL;
    }

    git_rms_remove_all(GIT_RMS_CMD_FILE);
    git_rms_remove_all(GIT_RMS_HEAD_FILE);
    git_rms_remove_all(GIT_RMS_SIZE_FILE);

    command = fopen(GIT_RMS_CMD_FILE, "w");
    if (command == NULL) {
        return NULL;
    }

    if (fprintf(command,
            "$ SET NOON\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"cat-file\" \"-p\" \"HEAD:%s\"\n"
            "$ IF .NOT. $STATUS THEN EXIT $STATUS\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"cat-file\" \"-s\" \"HEAD:%s\"\n"
            "$ EXIT $STATUS\n",
            GIT_RMS_HEAD_FILE,
            path,
            GIT_RMS_SIZE_FILE,
            path) < 0 ||
        fclose(command) != 0) {
        git_rms_remove_all(GIT_RMS_CMD_FILE);
        return NULL;
    }

    (void)snprintf(dcl, sizeof(dcl), "@%s", GIT_RMS_CMD_FILE);
    status = system(dcl);
    git_rms_remove_all(GIT_RMS_CMD_FILE);

    if ((status & 1) == 0 ||
        !git_rms_read_size(GIT_RMS_SIZE_FILE, &blob_size)) {
        git_rms_remove_all(GIT_RMS_HEAD_FILE);
        git_rms_remove_all(GIT_RMS_SIZE_FILE);
        return NULL;
    }

    text = git_rms_read_text(GIT_RMS_HEAD_FILE);
    git_rms_remove_all(GIT_RMS_HEAD_FILE);
    git_rms_remove_all(GIT_RMS_SIZE_FILE);

    if (text == NULL || strlen(text) != blob_size) {
        free(text);
        return NULL;
    }

    return text;
}

int git_rms_restore_head(const char *path)
{
    char git_path[GIT_RMS_PATH_MAX];
    char *head_text;
    char *current_text;
    int ok;

    if (!git_rms_make_git_path(path, git_path, sizeof(git_path))) {
        (void)fprintf(stderr,
                      "GITRESTORE: repository path resolution failed for %s.\n",
                      path != NULL ? path : "(null)");
        return 0;
    }

    head_text = git_rms_capture_head(git_path);
    if (head_text == NULL) {
        (void)fprintf(stderr,
                      "GITRESTORE: HEAD tree lookup failed for %s.\n",
                      git_path);
        return 0;
    }

    ok = rms_replace_text_file(path, head_text);
    if (!ok) {
        (void)fprintf(stderr,
                      "GITRESTORE: local RMS replace failed for %s.\n",
                      path);
        free(head_text);
        return 0;
    }

    current_text = git_rms_read_text(path);
    if (current_text == NULL) {
        (void)fprintf(stderr,
                      "GITRESTORE: post-write read failed for %s.\n",
                      path);
        free(head_text);
        return 0;
    }

    ok = strcmp(current_text, head_text) == 0;
    if (!ok) {
        (void)fprintf(stderr,
                      "GITRESTORE: post-write verification failed for %s.\n",
                      path);
    }
    free(current_text);
    free(head_text);
    return ok;
}
