#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "git_rms_restore.h"
#include "rms_write.h"
#include "LLM_TOOL_ARGS_M275.H"

#define M275_NESTED_PATH "TEST/GAP008_RMS.TXT"
#define M275_NESTED_EXPECTED \
    "repository version\n" \
    "local newer version\n"
#define M275_NESTED_MODIFIED \
    "repository version\n" \
    "local newer version\n" \
    "M275 nested restore modification\n"

static int m275_read_text(const char *path,
                          char *text,
                          size_t text_size)
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

static int m275_current_spec(const char *path,
                             char *spec,
                             size_t spec_size)
{
    FILE *file;

    if (path == NULL || spec == NULL || spec_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    if (fgetname(file, spec) == 0) {
        (void)fclose(file);
        return 0;
    }

    (void)fclose(file);
    spec[spec_size - 1U] = '\0';
    return 1;
}

static int m275_cleanup_to_spec(const char *path,
                                const char *original_spec)
{
    char spec[512];

    for (;;) {
        if (!m275_current_spec(path, spec, sizeof(spec))) {
            return 0;
        }

        if (strcmp(spec, original_spec) == 0) {
            return 1;
        }

        if (remove(spec) != 0) {
            return 0;
        }
    }
}

static int m275_test_tool_args(void)
{
    char command[1024];
    char name[32];
    char rest[1024];
    char small[16];
    size_t used;
    size_t index;

    (void)strcpy(command, "PATCH ");
    used = strlen(command);

    for (index = 0U; index < 900U; ++index) {
        command[used++] = 'A';
    }

    command[used++] = '|';
    command[used++] = '|';
    command[used++] = '\t';
    command[used++] = 'Z';
    command[used] = '\0';

    if (!m275_tool_args_split(command,
                              name, sizeof(name),
                              rest, sizeof(rest))) {
        return 0;
    }

    if (strcmp(name, "PATCH") != 0 || strlen(rest) != 904U) {
        return 0;
    }

    if (rest[900] != '|' ||
        rest[901] != '|' ||
        rest[902] != '\t' ||
        rest[903] != 'Z') {
        return 0;
    }

    if (m275_tool_args_split(command,
                             name, sizeof(name),
                             small, sizeof(small))) {
        return 0;
    }

    return 1;
}

static int m275_test_nested_restore(void)
{
    char saved_dir[512];
    char original_spec[512];
    char text[1024];
    int ok;
    int cleanup_ok;

    if (getcwd(saved_dir, sizeof(saved_dir)) == NULL) {
        (void)puts("M275 #84 stage: initial getcwd failed.");
        return 0;
    }

    if (!m275_current_spec(M275_NESTED_PATH,
                           original_spec,
                           sizeof(original_spec))) {
        (void)puts("M275 #84 stage: original RMS filespec capture failed.");
        return 0;
    }

    if (!rms_write_text_file(M275_NESTED_PATH,
                             M275_NESTED_MODIFIED)) {
        (void)puts("M275 #84 stage: disposable RMS modification failed.");
        return 0;
    }

    ok = 0;
    if (chdir("TEST") != 0) {
        (void)puts("M275 #84 stage: chdir TEST failed.");
    } else {
        (void)puts("M275 #84 stage: chdir TEST succeeded.");
        ok = git_rms_restore_head("GAP008_RMS.TXT");
        if (ok) {
            (void)puts("M275 #84 stage: nested restore call succeeded.");
        } else {
            (void)puts("M275 #84 stage: nested restore call failed.");
        }
        if (chdir(saved_dir) != 0) {
            (void)puts("M275 #84 stage: return to saved directory failed.");
            return 0;
        }
        (void)puts("M275 #84 stage: return to saved directory succeeded.");
    }

    if (ok) {
        if (!m275_read_text(M275_NESTED_PATH,
                            text, sizeof(text))) {
            (void)puts("M275 #84 stage: restored content read failed.");
            ok = 0;
        } else if (strcmp(text, M275_NESTED_EXPECTED) != 0) {
            (void)puts("M275 #84 stage: restored content mismatch.");
            ok = 0;
        } else {
            (void)puts("M275 #84 stage: restored content verified.");
        }
    }

    cleanup_ok = m275_cleanup_to_spec(M275_NESTED_PATH,
                                      original_spec);
    if (cleanup_ok) {
        (void)puts("M275 #84 stage: RMS version cleanup succeeded.");
    } else {
        (void)puts("M275 #84 stage: RMS version cleanup failed.");
    }

    return ok && cleanup_ok;
}

int main(void)
{
    if (!m275_test_tool_args()) {
        (void)puts("M275: raw tool argument split regression failed.");
        return 2;
    }
    (void)puts("M275 #83 raw tool argument regression passed.");

    if (!m275_test_nested_restore()) {
        (void)puts("M275: nested RMS-aware Git restore regression failed.");
        return 2;
    }
    (void)puts("M275 #84 nested Git restore regression passed.");

    return 1;
}
