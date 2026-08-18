#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "openai_plan.h"
#include "openai_execute.h"

#define OPENAI_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define OPENAI_PLAN_MAX_BYTES 65536U
#define OPENAI_PLAN_MAX_FILES 32U
#define OPENAI_PLAN_PATH_SIZE 256U

#include "openai_plan_sensitive.inc"
static int plan_path_char(int ch)
{
    return isalnum(ch) ||
           ch == '_' ||
           ch == '-' ||
           ch == '.' ||
           ch == '/';
}

static int plan_path_safe(const char *path)
{
    if (path == NULL ||
        *path == '\0' ||
        *path == '/' ||
        strchr(path, ':') != NULL ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    return 1;
}

typedef struct plan_scope_file {
    char path[OPENAI_PLAN_PATH_SIZE];
    int expect_missing;
} plan_scope_file;

static int plan_scope_find(
    plan_scope_file files[],
    unsigned int count,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(files[index].path, path) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int plan_scope_add(
    plan_scope_file files[],
    unsigned int *count,
    const char *path,
    int expect_missing)
{
    struct stat file_status;
    int index;
    int status;

    if (files == NULL ||
        count == NULL ||
        path == NULL ||
        !plan_path_safe(path) ||
        strlen(path) >= OPENAI_PLAN_PATH_SIZE) {
        return 0;
    }

    index = plan_scope_find(files, *count, path);

    if (index >= 0) {
        return files[index].expect_missing == expect_missing;
    }

    if (*count >= OPENAI_PLAN_MAX_FILES) {
        return 0;
    }

    errno = 0;
    status = stat(path, &file_status);

    if (expect_missing) {
        if (status == 0 || errno != ENOENT) {
            return 0;
        }
    } else if (status != 0) {
        return 0;
    }

    (void)strcpy(files[*count].path, path);
    files[*count].expect_missing = expect_missing;
    ++(*count);
    return 1;
}

static int plan_line_value(
    const char *start,
    const char *end,
    const char *prefix,
    char *value,
    size_t value_size)
{
    size_t prefix_length;
    size_t length;

    prefix_length = strlen(prefix);

    if ((size_t)(end - start) < prefix_length ||
        memcmp(start, prefix, prefix_length) != 0) {
        return 0;
    }

    start += prefix_length;
    length = (size_t)(end - start);

    if (length == 0U || length >= value_size) {
        return -1;
    }

    (void)memcpy(value, start, length);
    value[length] = '\0';
    return 1;
}

static int plan_collect_ops(
    const char *plan_text,
    plan_scope_file files[],
    unsigned int *count)
{
    const char *section;
    const char *cursor;
    const char *line_start;
    int in_operation;
    int in_payload;
    char type[32];
    char path[OPENAI_PLAN_PATH_SIZE];
    char target[OPENAI_PLAN_PATH_SIZE];

    section = strstr(plan_text, "operation_count=");

    if (section == NULL) {
        return -1;
    }

    cursor = section;
    line_start = section;
    in_operation = 0;
    in_payload = 0;
    type[0] = '\0';
    path[0] = '\0';
    target[0] = '\0';

    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            const char *end;
            size_t length;
            int result;

            end = cursor;
            if (end > line_start && end[-1] == '\r') {
                --end;
            }
            length = (size_t)(end - line_start);

            if (length == 15U &&
                memcmp(line_start, "BEGIN_OPERATION", 15U) == 0) {
                if (in_operation) {
                    return 0;
                }
                in_operation = 1;
                in_payload = 0;
                type[0] = '\0';
                path[0] = '\0';
                target[0] = '\0';
            } else if (length == 13U &&
                       memcmp(line_start, "END_OPERATION", 13U) == 0) {
                int missing;

                if (!in_operation || in_payload ||
                    type[0] == '\0' || path[0] == '\0') {
                    return 0;
                }

                if (strcmp(type, "create_file") == 0) {
                    missing = 1;
                } else if (strcmp(type, "replace_block") == 0 ||
                           strcmp(type, "delete_file") == 0 ||
                           strcmp(type, "rename_file") == 0 ||
                           strcmp(type, "move_file") == 0) {
                    missing = 0;
                } else {
                    return 0;
                }

                if (!plan_scope_add(files, count, path, missing)) {
                    return 0;
                }

                if (target[0] != '\0') {
                    if ((strcmp(type, "rename_file") != 0 &&
                         strcmp(type, "move_file") != 0) ||
                        !plan_scope_add(files, count, target, 1)) {
                        return 0;
                    }
                }

                in_operation = 0;
            } else if (in_operation) {
                if (length == 14U &&
                    (memcmp(line_start, "BEGIN_OLD_TEXT", 14U) == 0 ||
                     memcmp(line_start, "BEGIN_NEW_TEXT", 14U) == 0)) {
                    if (in_payload) {
                        return 0;
                    }
                    in_payload = 1;
                } else if (length == 12U &&
                           (memcmp(line_start, "END_OLD_TEXT", 12U) == 0 ||
                            memcmp(line_start, "END_NEW_TEXT", 12U) == 0)) {
                    if (!in_payload) {
                        return 0;
                    }
                    in_payload = 0;
                } else if (!in_payload) {
                    result = plan_line_value(
                        line_start, end, "type=",
                        type, sizeof(type));
                    if (result < 0) {
                        return 0;
                    }
                    if (result == 0) {
                        result = plan_line_value(
                            line_start, end, "path=",
                            path, sizeof(path));
                    }
                    if (result < 0) {
                        return 0;
                    }
                    if (result == 0) {
                        result = plan_line_value(
                            line_start, end, "target_path=",
                            target, sizeof(target));
                    }
                    if (result < 0) {
                        return 0;
                    }
                }
            }

            if (*cursor == '\0') {
                break;
            }

            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    return !in_operation && !in_payload;
}

static int plan_token_candidate(
    const char *candidate)
{
    return strchr(candidate, '/') != NULL ||
           strcmp(candidate, "BUILD.COM") == 0 ||
           strcmp(candidate, "OPENAI_MODULES.OPT") == 0 ||
           strcmp(candidate, "BUILD_OPENAI_MODULES.COM") == 0;
}

static int plan_collect_section(
    const char *start,
    const char *end,
    int expect_missing,
    plan_scope_file files[],
    unsigned int *count)
{
    const char *position;

    if (start == NULL) {
        return 1;
    }

    position = start;

    while (*position != '\0' &&
           (end == NULL || position < end)) {
        const char *token_start;
        size_t length;
        char candidate[OPENAI_PLAN_PATH_SIZE];

        if (!(isalpha((unsigned char)*position) ||
              *position == '.' ||
              *position == '_')) {
            ++position;
            continue;
        }

        token_start = position;

        while (*position != '\0' &&
               (end == NULL || position < end) &&
               (plan_path_char((unsigned char)*position) ||
                *position == ';' ||
                *position == '$')) {
            ++position;
        }

        length = (size_t)(position - token_start);

        while (length > 0U &&
               (token_start[length - 1U] == '.' ||
                token_start[length - 1U] == ',')) {
            --length;
        }

        if (length == 0U || length >= sizeof(candidate)) {
            continue;
        }

        (void)memcpy(candidate, token_start, length);
        candidate[length] = '\0';

        if (!plan_token_candidate(candidate)) {
            continue;
        }

        if (!plan_scope_add(
                files,
                count,
                candidate,
                expect_missing)) {
            return 0;
        }
    }

    return 1;
}

static int plan_collect_paths(
    const char *plan_text,
    plan_scope_file files[],
    unsigned int *count_out)
{
    const char *modify;
    const char *create;
    const char *ordered;
    int result;
    unsigned int count;

    if (plan_text == NULL ||
        files == NULL ||
        count_out == NULL) {
        return 0;
    }

    count = 0U;
    result = plan_collect_ops(plan_text, files, &count);

    if (result == 1) {
        *count_out = count;
        return 1;
    }

    if (result == 0) {
        return 0;
    }

    modify = strstr(plan_text, "Files to modify");
    create = strstr(plan_text, "Files to create");

    if (modify != NULL) {
        if (!plan_collect_section(
                modify,
                create,
                0,
                files,
                &count)) {
            return 0;
        }
    }

    if (create != NULL) {
        ordered = strstr(create, "Ordered edits");

        if (!plan_collect_section(
                create,
                ordered,
                1,
                files,
                &count)) {
            return 0;
        }
    }

    *count_out = count;
    return 1;
}

static int plan_write_contents(
    FILE *file,
    const char *goal,
    const char *plan_text,
    plan_scope_file files[],
    unsigned int path_count)
{
    time_t now;
    struct tm *local_time;
    char timestamp[32];
    unsigned int index;

    now = time(NULL);
    local_time = localtime(&now);

    if (local_time != NULL &&
        strftime(timestamp,
                 sizeof(timestamp),
                 "%Y-%m-%dT%H:%M:%S",
                 local_time) > 0U) {
        /* timestamp is ready */
    } else {
        (void)strcpy(timestamp, "unknown-time");
    }

    if (fprintf(
            file,
            "format=1\n"
            "status=active\n"
            "goal_length=%lu\n"
            "plan_length=%lu\n"
            "created=%s\n"
            "file_count=%u\n"
            "\n[goal]\n%s\n"
            "\n[files]\n",
            (unsigned long)strlen(goal),
            (unsigned long)strlen(plan_text),
            timestamp,
            path_count,
            goal) < 0) {
        return 0;
    }

    for (index = 0U; index < path_count; ++index) {
        struct stat file_status;

        if (files[index].expect_missing) {
            if (fprintf(
                    file,
                    "file=%s|missing=1\n",
                    files[index].path) < 0) {
                return 0;
            }
        } else {
            if (stat(files[index].path, &file_status) != 0) {
                return 0;
            }

            if (fprintf(
                    file,
                    "file=%s|size=%lu|modified=%ld\n",
                    files[index].path,
                    (unsigned long)file_status.st_size,
                    (long)file_status.st_mtime) < 0) {
                return 0;
            }
        }
    }

    if (fprintf(file, "\n[plan]\n%s\n", plan_text) < 0) {
        return 0;
    }

    return 1;
}

#define openai_plan_save openai_plan_save_legacy
int openai_plan_save(const char *goal,
                     const char *plan_text)
{
    FILE *file;
    size_t total_size;
    plan_scope_file files[OPENAI_PLAN_MAX_FILES];
    unsigned int path_count;
    int success;

    if (goal == NULL ||
        *goal == '\0' ||
        plan_text == NULL ||
        *plan_text == '\0') {
        return 0;
    }

    total_size = strlen(goal) + strlen(plan_text);

    if (total_size > OPENAI_PLAN_MAX_BYTES) {
        (void)printf(
            "Implementation plan is too large (%lu bytes; limit %u).\n",
            (unsigned long)total_size,
            OPENAI_PLAN_MAX_BYTES
        );
        return 0;
    }

    if (plan_has_sensitive_text(goal) ||
        plan_has_sensitive_text(plan_text)) {
        (void)puts(
            "Implementation plan was not saved because it contains "
            "sensitive-content markers."
        );
        return 0;
    }

    if (!plan_collect_paths(plan_text, files, &path_count)) {
        (void)puts(
            "Implementation plan was not saved because "
            "its file scope is malformed, ambiguous, or stale."
        );
        return 0;
    }

    file = fopen(OPENAI_PLAN_FILE, "w");

    if (file == NULL) {
        (void)printf(
            "Unable to create %s: %s\n",
            OPENAI_PLAN_FILE,
            strerror(errno)
        );
        return 0;
    }

    success = plan_write_contents(
        file,
        goal,
        plan_text,
        files,
        path_count
    );

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

#define openai_plan_show openai_plan_show_legacy
void openai_plan_show(void)
{
    FILE *file;
    char line[1024];

    file = fopen(OPENAI_PLAN_FILE, "r");

    if (file == NULL) {
        (void)puts("No saved implementation plan is available.");
        return;
    }

    (void)puts("OVMS Agent saved implementation plan");
    (void)puts("------------------------------------");

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)fputs(line, stdout);
    }

    (void)fclose(file);
}


#define openai_plan_file_current openai_plan_file_current_legacy
int openai_plan_file_current(const char *plan_path, int verbose)
{
    FILE *file;
    char line[1024];
    int saw_format;
    int saw_status;
    int saw_file_count;
    unsigned int expected_files;
    unsigned int checked_files;
    int current;

    file = fopen(plan_path, "r");

    if (file == NULL) {
        if (verbose) {
            (void)puts("No saved implementation plan is available.");
        }

        return 0;
    }

    saw_format = 0;
    saw_status = 0;
    saw_file_count = 0;
    expected_files = 0U;
    checked_files = 0U;
    current = 1;

    while (fgets(line, sizeof(line), file) != NULL) {
        int format_value;
        unsigned int file_count;
        unsigned int missing_flag;
        char path[OPENAI_PLAN_PATH_SIZE];
        unsigned long saved_size;
        long saved_modified;

        if (sscanf(line, "format=%d", &format_value) == 1) {
            saw_format = format_value == 1;

            if (!saw_format) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Saved plan uses unsupported format %d.\n",
                        format_value
                    );
                }
            }

            continue;
        }

        if (strncmp(line, "status=", 7U) == 0) {
            char status[32];

            if (sscanf(line, "status=%31s", status) == 1 &&
                strcmp(status, "active") == 0) {
                saw_status = 1;
            } else {
                current = 0;

                if (verbose) {
                    (void)puts("Saved implementation plan is not active.");
                }
            }

            continue;
        }

        if (sscanf(line, "file_count=%u", &file_count) == 1) {
            expected_files = file_count;
            saw_file_count = 1;
            continue;
        }

        if (sscanf(
                line,
                "file=%255[^|]|missing=%u",
                path,
                &missing_flag) == 2) {
            struct stat file_status;
            int stat_status;

            ++checked_files;

            if (!plan_path_safe(path) || missing_flag != 1U) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan contains an invalid missing-file scope: %s\n",
                        path
                    );
                }

                continue;
            }

            errno = 0;
            stat_status = stat(path, &file_status);

            if (stat_status == 0) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan is stale: %s now exists.\n",
                        path
                    );
                }
            } else if (errno != ENOENT) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan is stale: %s cannot be checked safely.\n",
                        path
                    );
                }
            }

            continue;
        }

        if (sscanf(
                line,
                "file=%255[^|]|size=%lu|modified=%ld",
                path,
                &saved_size,
                &saved_modified) == 3) {
            struct stat file_status;

            ++checked_files;

            if (!plan_path_safe(path)) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan contains an unsafe path: %s\n",
                        path
                    );
                }

                continue;
            }

            if (stat(path, &file_status) != 0) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan is stale: %s is missing or inaccessible.\n",
                        path
                    );
                }

                continue;
            }

            if ((unsigned long)file_status.st_size != saved_size ||
                (long)file_status.st_mtime != saved_modified) {
                current = 0;

                if (verbose) {
                    (void)printf(
                        "Plan is stale: %s changed after planning.\n",
                        path
                    );
                }
            }

            continue;
        }

        if (strncmp(line, "file=", 5U) == 0) {
            current = 0;

            if (verbose) {
                (void)puts(
                    "Saved plan contains a malformed file-scope record."
                );
            }
        }
    }

    (void)fclose(file);

    if (!saw_format) {
        current = 0;

        if (verbose) {
            (void)puts("Saved plan has no valid format marker.");
        }
    }

    if (!saw_status) {
        current = 0;

        if (verbose) {
            (void)puts("Saved plan has no active status marker.");
        }
    }

    if (!saw_file_count) {
        current = 0;

        if (verbose) {
            (void)puts("Saved plan has no file-count marker.");
        }
    } else if (checked_files != expected_files) {
        current = 0;

        if (verbose) {
            (void)printf(
                "Saved plan fingerprint count mismatch: "
                "expected %u, found %u.\n",
                expected_files,
                checked_files
            );
        }
    }

    if (verbose && current) {
        (void)printf(
            "Saved implementation plan is current "
            "(%u file fingerprint%s verified).\n",
            checked_files,
            checked_files == 1U ? "" : "s"
        );
    }

    return current;
}

#undef openai_plan_save
#undef openai_plan_file_current
#include "openai_plan_m138_integrity.inc"
#include "openai_plan_m138_validate.inc"
#undef openai_plan_show
#undef openai_plan_clear
#include "openai_plan_m139_lifecycle.inc"
#include "openai_plan_m139_validate.inc"
#include "openai_plan_m140_approval.inc"
#include "openai_plan_m140_validate.inc"
#include "openai_plan_m141_validate.inc"
#include "openai_plan_m142_validate.inc"
#include "openai_plan_m143_validate.inc"
#include "openai_plan_m144_validate.inc"
#include "openai_plan_m145_validate.inc"
#include "openai_plan_m146_validate.inc"
#include "openai_m148_validate.inc"
#include "openai_m149_validate.inc"
#include "openai_m150_validate.inc"
#include "openai_plan_current_wrapper.inc"
void openai_plan_validate(void)
{
    (void)openai_plan_is_current(1);
    openai_plan_approval_report();
}

#define openai_plan_clear openai_plan_clear_legacy
void openai_plan_clear(void)
{
    char answer[32];

    (void)printf("Clear the saved implementation plan [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return;
    }

    if (answer[0] != 'y' && answer[0] != 'Y') {
        (void)puts("Saved implementation plan clear cancelled.");
        return;
    }

    if (remove(OPENAI_PLAN_FILE) != 0 && errno != ENOENT) {
        (void)printf(
            "Unable to remove %s: %s\n",
            OPENAI_PLAN_FILE,
            strerror(errno)
        );
        return;
    }

    (void)puts("Saved implementation plan cleared.");
}
