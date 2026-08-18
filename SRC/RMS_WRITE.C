#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unixio.h>
#include <stat.h>
#include <rms.h>
#include <errno.h>

#include "rms_write.h"

#define RMS_WRITE_COMMAND_SIZE 1024U
#define RMS_WRITE_PATH_SIZE 512U
#define RMS_WRITE_RECORD_SIZE 32767U
#define RMS_RUN_MAX_FILES 16U

extern int fgetname(FILE *, char *);

typedef struct rms_run_file {
    char path[RMS_WRITE_PATH_SIZE];
    char original_spec[RMS_WRITE_PATH_SIZE];
    unsigned int existed_before;
    unsigned int written;
} rms_run_file;

static rms_run_file rms_run_files[RMS_RUN_MAX_FILES];
static unsigned int rms_run_count = 0U;
static int rms_run_active = 0;

static int rms_write_case_equal(
    const char *left,
    const char *right)
{
    while (*left != '\0' && *right != '\0') {
        int a;
        int b;

        a = *left >= 'a' && *left <= 'z' ?
            *left - ('a' - 'A') : *left;
        b = *right >= 'a' && *right <= 'z' ?
            *right - ('a' - 'A') : *right;

        if (a != b) {
            return 0;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int rms_run_get_spec(
    const char *path,
    char *spec,
    size_t spec_size)
{
    FILE *file;
    int status;

    if (path == NULL || spec == NULL || spec_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    status = fgetname(file, spec);
    (void)fclose(file);

    if (status == 0) {
        return 0;
    }

    spec[spec_size - 1U] = '\0';
    return 1;
}

static int rms_run_find(const char *path)
{
    unsigned int index;

    for (index = 0U; index < rms_run_count; ++index) {
        if (rms_write_case_equal(rms_run_files[index].path, path)) {
            return (int)index;
        }
    }

    return -1;
}

static int rms_run_capture(const char *path)
{
    rms_run_file *file;
    int index;

    if (!rms_run_active) {
        return 1;
    }

    index = rms_run_find(path);
    if (index >= 0) {
        return 1;
    }

    if (rms_run_count >= RMS_RUN_MAX_FILES ||
        strlen(path) >= RMS_WRITE_PATH_SIZE) {
        return 0;
    }

    file = &rms_run_files[rms_run_count];
    (void)memset(file, 0, sizeof(*file));
    (void)strcpy(file->path, path);

    if (rms_run_get_spec(
            path,
            file->original_spec,
            sizeof(file->original_spec))) {
        file->existed_before = 1U;
    } else {
        file->existed_before = 0U;
        file->original_spec[0] = '\0';
    }

    ++rms_run_count;
    return 1;
}

static void rms_run_mark(const char *path)
{
    int index;

    if (!rms_run_active) {
        return;
    }

    index = rms_run_find(path);
    if (index >= 0) {
        rms_run_files[index].written = 1U;
    }
}

static int rms_run_result(const char *path, int success)
{
    if (success) {
        rms_run_mark(path);
    }

    return success;
}

void rms_run_begin(void)
{
    (void)memset(rms_run_files, 0, sizeof(rms_run_files));
    rms_run_count = 0U;
    rms_run_active = 1;
}

void rms_run_commit(void)
{
    (void)memset(rms_run_files, 0, sizeof(rms_run_files));
    rms_run_count = 0U;
    rms_run_active = 0;
}

int rms_run_has_writes(void)
{
    unsigned int index;

    for (index = 0U; index < rms_run_count; ++index) {
        if (rms_run_files[index].written) {
            return 1;
        }
    }

    return 0;
}

int rms_run_rollback(void)
{
    unsigned int index;
    int success;

    if (!rms_run_active) {
        return 1;
    }

    success = 1;
    index = rms_run_count;

    while (index > 0U) {
        rms_run_file *file;
        char current_spec[RMS_WRITE_PATH_SIZE];

        --index;
        file = &rms_run_files[index];

        if (!file->written) {
            continue;
        }

        if (file->existed_before) {
            for (;;) {
                if (!rms_run_get_spec(
                        file->path,
                        current_spec,
                        sizeof(current_spec))) {
                    success = 0;
                    break;
                }

                if (strcmp(current_spec, file->original_spec) == 0) {
                    file->written = 0U;
                    break;
                }

                if (remove(current_spec) != 0) {
                    success = 0;
                    break;
                }
            }
        } else {
            while (rms_run_get_spec(
                       file->path,
                       current_spec,
                       sizeof(current_spec))) {
                if (remove(current_spec) != 0) {
                    success = 0;
                    break;
                }
            }

            if (success) {
                file->written = 0U;
            }
        }
    }

    rms_run_active = 0;
    return success;
}

static int rms_write_has_extension(
    const char *path,
    const char *extension)
{
    const char *dot;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');

    if (dot == NULL) {
        return 0;
    }

    return rms_write_case_equal(dot, extension);
}

int rms_path_requires_record_writer(
    const char *path)
{
    return rms_write_has_extension(path, ".OPT") ||
           rms_write_has_extension(path, ".COM") ||
           rms_write_has_extension(path, ".CLD");
}

static int rms_write_emit_record(
    int file_descriptor,
    const char *record,
    size_t length)
{
    if (length > RMS_WRITE_RECORD_SIZE) {
        return 0;
    }

    if (length == 0U) {
        return write(file_descriptor, "", 0U) >= 0;
    }

    return write(
        file_descriptor,
        record,
        length
    ) == (int)length;
}

static int rms_write_records(
    int file_descriptor,
    const char *text)
{
    const char *start;
    const char *position;

    if (text == NULL) {
        return 0;
    }

    start = text;
    position = text;

    while (*position != '\0') {
        if (*position == '\n') {
            size_t length;

            length = (size_t)(position - start);

            if (length > 0U &&
                start[length - 1U] == '\r') {
                --length;
            }

            if (!rms_write_emit_record(
                    file_descriptor,
                    start,
                    length)) {
                return 0;
            }

            ++position;
            start = position;
        } else {
            ++position;
        }
    }

    if (position != start) {
        size_t length;

        length = (size_t)(position - start);

        if (length > 0U &&
            start[length - 1U] == '\r') {
            --length;
        }

        if (!rms_write_emit_record(
                file_descriptor,
                start,
                length)) {
            return 0;
        }
    }

    return 1;
}

int rms_write_text_file(
    const char *path,
    const char *text)
{
    int file_descriptor;
    int ok;

    if (path == NULL || text == NULL) {
        return 0;
    }

    file_descriptor = creat(
        path,
        0,
        "rat=cr",
        "rfm=var"
    );

    if (file_descriptor < 0) {
        return 0;
    }

    ok = rms_write_records(
        file_descriptor,
        text);

    if (!ok) {
        (void)close(file_descriptor);
        return 0;
    }

    return close(file_descriptor) == 0;
}

int rms_replace_text_file(
    const char *path,
    const char *text)
{
    if (path == NULL || text == NULL) {
        return 0;
    }

    if (!rms_run_capture(path)) {
        return 0;
    }

    if (!rms_path_requires_record_writer(path)) {
        FILE *file;

        file = fopen(path, "w");

        if (file == NULL) {
            return 0;
        }

        if (fputs(text, file) == EOF) {
            (void)fclose(file);
            return 0;
        }

        return rms_run_result(path, fclose(file) == 0);
    } else {
        struct stat stat_buffer;
        int have_stat;
        int file_descriptor;
        int ok;

        errno = 0;
        have_stat = stat(path, &stat_buffer);

        if (have_stat == 0) {
            switch (stat_buffer.st_fab_rfm) {
            case FAB$C_STM:
            case FAB$C_STMLF:
            case FAB$C_STMCR:
                {
                    FILE *file;

                    file = fopen(path, "w");

                    if (file == NULL) {
                        return 0;
                    }

                    if (fputs(text, file) == EOF) {
                        (void)fclose(file);
                        return 0;
                    }

                    return rms_run_result(
                        path,
                        fclose(file) == 0
                    );
                }

            default:
                file_descriptor = creat(
                    path,
                    0,
                    "rat=cr",
                    "rfm=var"
                );

                if (file_descriptor < 0) {
                    return 0;
                }

                ok = rms_write_records(
                    file_descriptor,
                    text);

                if (!ok) {
                    (void)close(file_descriptor);
                    return 0;
                }

                return rms_run_result(
                    path,
                    close(file_descriptor) == 0
                );
            }
        } else {
            if (errno == ENOENT) {
                file_descriptor = creat(
                    path,
                    0,
                    "rat=cr",
                    "rfm=var"
                );

                if (file_descriptor < 0) {
                    return 0;
                }

                ok = rms_write_records(
                    file_descriptor,
                    text);

                if (!ok) {
                    (void)close(file_descriptor);
                    return 0;
                }

                return rms_run_result(
                    path,
                    close(file_descriptor) == 0
                );
            }

            return 0;
        }
    }
}
