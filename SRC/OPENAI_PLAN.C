#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "openai_plan.h"

#define OPENAI_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define OPENAI_PLAN_MAX_BYTES 65536U
#define OPENAI_PLAN_MAX_FILES 32U
#define OPENAI_PLAN_PATH_SIZE 256U

static int plan_has_sensitive_text(const char *text)
{
    static const char *blocked[] = {
        "OPENAI_API_KEY",
        "AUTHORIZATION: BEARER",
        "OVMS_AGENT_HEADERS",
        ".PEM",
        ".KEY",
        NULL
    };
    const char **pattern;
    const unsigned char *position;

    if (text == NULL) {
        return 1;
    }

    for (pattern = blocked; *pattern != NULL; ++pattern) {
        size_t pattern_length;

        pattern_length = strlen(*pattern);

        for (position = (const unsigned char *)text;
             *position != (unsigned char)'\0';
             ++position) {
            size_t index;

            for (index = 0U;
                 index < pattern_length &&
                 position[index] != (unsigned char)'\0' &&
                 toupper((int)position[index]) ==
                     (unsigned char)(*pattern)[index];
                 ++index) {
                /* compare case-insensitively */
            }

            if (index == pattern_length) {
                return 1;
            }
        }
    }

    return 0;
}

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

static int plan_path_exists(
    char paths[][OPENAI_PLAN_PATH_SIZE],
    unsigned int count,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(paths[index], path) == 0) {
            return 1;
        }
    }

    return 0;
}

static unsigned int plan_collect_paths(
    const char *plan_text,
    char paths[][OPENAI_PLAN_PATH_SIZE])
{
    const char *position;
    const char *section_end;
    unsigned int count;

    position = strstr(plan_text, "Files to modify");

    if (position != NULL) {
        section_end = strstr(position, "Files to create");
    } else {
        position = plan_text;
        section_end = NULL;
    }

    count = 0U;

    while (*position != '\0' &&
           (section_end == NULL || position < section_end) &&
           count < OPENAI_PLAN_MAX_FILES) {
        const char *start;
        size_t length;
        char candidate[OPENAI_PLAN_PATH_SIZE];
        struct stat file_status;

        if (!(isalpha((unsigned char)*position) ||
              *position == '.' ||
              *position == '_')) {
            ++position;
            continue;
        }

        start = position;

        while (*position != '\0' &&
               plan_path_char((unsigned char)*position)) {
            ++position;
        }

        length = (size_t)(position - start);

        while (length > 0U &&
               (start[length - 1U] == '.' ||
                start[length - 1U] == ',' ||
                start[length - 1U] == ';')) {
            --length;
        }

        if (length == 0U || length >= sizeof(candidate)) {
            continue;
        }

        (void)memcpy(candidate, start, length);
        candidate[length] = '\0';

        if (strchr(candidate, '/') == NULL &&
            strcmp(candidate, "BUILD.COM") != 0 &&
            strcmp(candidate, "OPENAI_MODULES.OPT") != 0 &&
            strcmp(candidate, "BUILD_OPENAI_MODULES.COM") != 0) {
            continue;
        }

        if (!plan_path_safe(candidate) ||
            plan_path_exists(paths, count, candidate)) {
            continue;
        }

        if (stat(candidate, &file_status) != 0) {
            continue;
        }

        (void)strcpy(paths[count], candidate);
        ++count;
    }

    return count;
}

static int plan_write_contents(
    FILE *file,
    const char *goal,
    const char *plan_text,
    char paths[][OPENAI_PLAN_PATH_SIZE],
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

        if (stat(paths[index], &file_status) == 0) {
            if (fprintf(
                    file,
                    "file=%s|size=%lu|modified=%ld\n",
                    paths[index],
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

int openai_plan_save(const char *goal,
                     const char *plan_text)
{
    FILE *file;
    size_t total_size;
    char paths[OPENAI_PLAN_MAX_FILES][OPENAI_PLAN_PATH_SIZE];
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

    path_count = plan_collect_paths(plan_text, paths);

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
        paths,
        path_count
    );

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

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


int openai_plan_is_current(int verbose)
{
    FILE *file;
    char line[1024];
    int saw_format;
    int saw_status;
    int saw_file_count;
    unsigned int expected_files;
    unsigned int checked_files;
    int current;

    file = fopen(OPENAI_PLAN_FILE, "r");

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

void openai_plan_validate(void)
{
    (void)openai_plan_is_current(1);
}

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
