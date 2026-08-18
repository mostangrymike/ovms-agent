#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git_rms_restore.h"
#include "rms_write.h"

#define GAP008_PATH "TEST/GAP008_RMS.TXT"
#define GAP008_EXPECTED "repository version\nlocal newer version\n"
#define GAP008_OPT_PATH "TEST/GAP008_RECORD.OPT"
#define GAP008_OPT_TEXT \
    "[.BUILD]OPENAI_REPAIR.OBJ\n" \
    "[.BUILD]GIT_RMS_RESTORE.OBJ\n"
#define GAP008_COM_PATH "TEST/GAP008_RECORD.COM"
#define GAP008_COM_TEXT \
    "$ WRITE SYS$OUTPUT \"GAP008\"\n" \
    "$ EXIT\n"

static int read_text(const char *path, char *text, size_t text_size)
{
    FILE *file;
    size_t used;
    int ch;

    if (path == NULL || text == NULL || text_size < 2U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    used = 0U;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\r') {
            continue;
        }
        if (used + 1U >= text_size) {
            (void)fclose(file);
            return 0;
        }
        text[used++] = (char)ch;
    }

    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }

    text[used] = '\0';
    return fclose(file) == 0;
}

static int test_record_restore(const char *path,
                               const char *expected)
{
    char modified[1024];
    char text[1024];
    int written;

    written = snprintf(
        modified,
        sizeof(modified),
        "%sGAP008 local record modification\n",
        expected
    );
    if (written < 0 || (size_t)written >= sizeof(modified)) {
        return 0;
    }

    if (!rms_write_text_file(path, modified)) {
        return 0;
    }

    if (!git_rms_restore_head(path)) {
        return 0;
    }

    return read_text(path, text, sizeof(text)) &&
           strcmp(text, expected) == 0;
}

int main(void)
{
    FILE *file;
    FILE *history;
    char original_spec[512];
    char restored_spec[512];
    char text[1024];

    file = fopen(GAP008_PATH, "r");
    if (file == NULL) {
        (void)puts("M265: unable to open tracked GAP-008 fixture.");
        return 2;
    }

    if (fgetname(file, original_spec) == 0) {
        (void)fclose(file);
        (void)puts("M265: unable to resolve original RMS version.");
        return 2;
    }
    (void)fclose(file);

    file = fopen(GAP008_PATH, "a");
    if (file == NULL ||
        fputs("GAP008 local modification after commit\n", file) == EOF ||
        fclose(file) != 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        (void)puts("M265: unable to create local fixture modification.");
        return 2;
    }

    if (!git_rms_restore_head(GAP008_PATH)) {
        (void)puts("M265: RMS-aware Git restore failed.");
        return 2;
    }

    if (!read_text(GAP008_PATH, text, sizeof(text)) ||
        strcmp(text, GAP008_EXPECTED) != 0) {
        (void)puts("M265: restored contents do not match HEAD.");
        return 2;
    }

    file = fopen(GAP008_PATH, "r");
    if (file == NULL || fgetname(file, restored_spec) == 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        (void)puts("M265: unable to resolve restored RMS version.");
        return 2;
    }
    (void)fclose(file);

    if (strcmp(original_spec, restored_spec) == 0) {
        (void)puts("M265: restore did not create a new RMS version.");
        return 2;
    }

    history = fopen(original_spec, "r");
    if (history == NULL) {
        (void)puts("M265: prior RMS version was destroyed.");
        return 2;
    }
    (void)fclose(history);

    if (!test_record_restore(GAP008_OPT_PATH, GAP008_OPT_TEXT)) {
        (void)puts("M265: RMS-aware OPT restore failed.");
        return 2;
    }

    if (!test_record_restore(GAP008_COM_PATH, GAP008_COM_TEXT)) {
        (void)puts("M265: RMS-aware COM restore failed.");
        return 2;
    }

    (void)puts("M265 GAP-008 RMS-aware Git restore regression passed.");
    return 1;
}
