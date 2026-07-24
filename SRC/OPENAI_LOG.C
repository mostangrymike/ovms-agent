#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "openai_log.h"
#include "openai_state.h"

#define OPENAI_ACTIVITY_LOG_FILE "OVMS_AGENT_ACTIVITY.LOG"
#define OPENAI_ACTIVITY_LOG_OLD_FILE "OVMS_AGENT_ACTIVITY_OLD.LOG"
#define OPENAI_ACTIVITY_LOG_MAX_BYTES 262144L

static int openai_copy_file(const char *source_path,
                            const char *destination_path)
{
    unsigned char buffer[8192];
    FILE *source;
    FILE *destination;
    size_t count;
    int success;

    source = fopen(source_path, "rb");

    if (source == NULL) {
        return 0;
    }

    destination = fopen(destination_path, "wb");

    if (destination == NULL) {
        (void)fclose(source);
        return 0;
    }

    success = 1;

    while ((count = fread(
                buffer,
                1U,
                sizeof(buffer),
                source)) > 0U) {
        if (fwrite(buffer, 1U, count, destination) != count) {
            success = 0;
            break;
        }
    }

    if (ferror(source)) {
        success = 0;
    }

    if (fclose(source) != 0) {
        success = 0;
    }

    if (fclose(destination) != 0) {
        success = 0;
    }

    return success;
}

static int openai_rotate_log_if_needed(void)
{
    FILE *file;
    long length;

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "rb");

    if (file == NULL) {
        return 1;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return 0;
    }

    length = ftell(file);
    (void)fclose(file);

    if (length < 0L) {
        return 0;
    }

    if (length < OPENAI_ACTIVITY_LOG_MAX_BYTES) {
        return 1;
    }

    (void)remove(OPENAI_ACTIVITY_LOG_OLD_FILE);

    if (!openai_copy_file(
            OPENAI_ACTIVITY_LOG_FILE,
            OPENAI_ACTIVITY_LOG_OLD_FILE)) {
        return 0;
    }

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "w");

    if (file == NULL) {
        return 0;
    }

    (void)fprintf(
        file,
        "Log rotated after reaching %ld bytes. "
        "Previous snapshot: %s\n",
        length,
        OPENAI_ACTIVITY_LOG_OLD_FILE
    );

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

void openai_log_event(const char *workflow,
                             const char *event,
                             int status)
{
    FILE *file;
    time_t now;
    struct tm *local_time;
    char timestamp[32];

    if (workflow == NULL || event == NULL) {
        return;
    }

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

    if (!openai_rotate_log_if_needed()) {
        return;
    }

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "a");

    if (file == NULL) {
        return;
    }

    (void)fprintf(
        file,
        "%s workflow=%s event=%s status=%d\n",
        timestamp,
        workflow,
        event,
        status
    );

    (void)fclose(file);
    openai_state_save();
}

void openai_show_log(void)
{
    FILE *file;
    char line[1024];

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "r");

    if (file == NULL) {
        (void)puts("No activity log is available in this process directory.");
        return;
    }

    (void)puts("OVMS Agent activity log");
    (void)puts("-----------------------");

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)fputs(line, stdout);
    }

    (void)fclose(file);
}

void openai_show_old_log(void)
{
    FILE *file;
    char line[1024];

    file = fopen(OPENAI_ACTIVITY_LOG_OLD_FILE, "r");

    if (file == NULL) {
        (void)puts("No rotated activity log is available.");
        return;
    }

    (void)puts("OVMS Agent rotated activity log");
    (void)puts("-------------------------------");

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)fputs(line, stdout);
    }

    (void)fclose(file);
}

void openai_clear_log(void)
{
    char answer[32];
    FILE *file;

    (void)printf(
        "Clear the active OVMS Agent activity log [y/N]? "
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return;
    }

    if (answer[0] != 'y' && answer[0] != 'Y') {
        (void)puts("Activity log clear cancelled.");
        return;
    }

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "w");

    if (file == NULL) {
        (void)printf(
            "Unable to clear %s: %s\n",
            OPENAI_ACTIVITY_LOG_FILE,
            strerror(errno)
        );
        return;
    }

    (void)fclose(file);
    (void)puts("Active activity log cleared.");
}

static unsigned long openai_percentage(unsigned long part,
                                       unsigned long total)
{
    if (total == 0UL) {
        return 0UL;
    }

    return (part * 100UL + total / 2UL) / total;
}

void openai_show_metrics(void)
{
    FILE *file;
    char line[1024];
    unsigned long total_events;
    unsigned long workflow_starts;
    unsigned long builds_success;
    unsigned long builds_failure;
    unsigned long patches_applied;
    unsigned long patches_declined;
    unsigned long patches_failed;
    unsigned long rollbacks_succeeded;
    unsigned long rollbacks_failed;
    unsigned long rollbacks_declined;
    unsigned long selftests_passed;
    unsigned long selftests_failed;
    unsigned long verifies_passed;
    unsigned long verifies_failed;

    total_events = 0UL;
    workflow_starts = 0UL;
    builds_success = 0UL;
    builds_failure = 0UL;
    patches_applied = 0UL;
    patches_declined = 0UL;
    patches_failed = 0UL;
    rollbacks_succeeded = 0UL;
    rollbacks_failed = 0UL;
    rollbacks_declined = 0UL;
    selftests_passed = 0UL;
    selftests_failed = 0UL;
    verifies_passed = 0UL;
    verifies_failed = 0UL;

    file = fopen(OPENAI_ACTIVITY_LOG_FILE, "r");

    if (file == NULL) {
        (void)puts(
            "No activity log is available for metrics."
        );
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        ++total_events;

        if (strstr(line, " event=start ") != NULL) {
            ++workflow_starts;
        } else if (strstr(line, " event=build_success ") != NULL) {
            ++builds_success;
        } else if (strstr(line, " event=build_failure ") != NULL) {
            ++builds_failure;
        } else if (strstr(line, " event=patch_applied ") != NULL) {
            ++patches_applied;
        } else if (strstr(line, " event=patch_declined ") != NULL) {
            ++patches_declined;
        } else if (strstr(line, " event=patch_failed ") != NULL) {
            ++patches_failed;
        } else if (strstr(line, " event=rollback_succeeded ") != NULL) {
            ++rollbacks_succeeded;
        } else if (strstr(line, " event=rollback_failed ") != NULL) {
            ++rollbacks_failed;
        } else if (strstr(line, " event=rollback_declined ") != NULL) {
            ++rollbacks_declined;
        } else if (strstr(line, " event=selftest_pass ") != NULL) {
            ++selftests_passed;
        } else if (strstr(line, " event=selftest_fail ") != NULL) {
            ++selftests_failed;
        } else if (strstr(line, " event=verify_pass ") != NULL) {
            ++verifies_passed;
        } else if (strstr(line, " event=verify_fail ") != NULL) {
            ++verifies_failed;
        }
    }

    (void)fclose(file);

    (void)puts("OVMS Agent activity metrics");
    (void)puts("---------------------------");
    (void)printf("Total logged events:          %lu\n", total_events);
    (void)printf("Workflow starts:              %lu\n", workflow_starts);
    (void)puts("");

    (void)puts("Builds");
    (void)printf("  Success:                    %lu\n", builds_success);
    (void)printf("  Failure:                    %lu\n", builds_failure);
    (void)printf(
        "  Success rate:               %lu%%\n",
        openai_percentage(
            builds_success,
            builds_success + builds_failure
        )
    );
    (void)puts("");

    (void)puts("Patches");
    (void)printf("  Applied:                    %lu\n", patches_applied);
    (void)printf("  Declined:                   %lu\n", patches_declined);
    (void)printf("  Failed:                     %lu\n", patches_failed);
    (void)puts("");

    (void)puts("Rollbacks");
    (void)printf("  Succeeded:                  %lu\n", rollbacks_succeeded);
    (void)printf("  Failed:                     %lu\n", rollbacks_failed);
    (void)printf("  Declined:                   %lu\n", rollbacks_declined);
    (void)puts("");

    (void)puts("Reliability checks");
    (void)printf("  Self-tests passed:          %lu\n", selftests_passed);
    (void)printf("  Self-tests failed:          %lu\n", selftests_failed);
    (void)printf("  Verifications passed:       %lu\n", verifies_passed);
    (void)printf("  Verifications failed:       %lu\n", verifies_failed);
}
