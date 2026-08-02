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

    /*
     * On OpenVMS record files, one write() call emits one RMS record.
     */
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

        return fclose(file) == 0;
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

                    return fclose(file) == 0;
                }

            default:
                file_descriptor = creat(
                    path,
                    0
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

                return close(file_descriptor) == 0;
            }

            return 0;
        }
    }
}
