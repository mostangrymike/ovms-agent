#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openai_log.h"
#include "openai_state.h"

const char *openai_rollback_name(int rollback_state);

#define OPENAI_ACTIVITY_LOG_FILE "OVMS_AGENT_ACTIVITY.LOG"
#define OPENAI_ACTIVITY_LOG_OLD_FILE "OVMS_AGENT_ACTIVITY_OLD.LOG"
#define OPENAI_ACTIVITY_LOG_MAX_BYTES 262144L


static const char *openai_test_log_path = NULL;

void openai_test_set_log_path(const char *path)
{
    openai_test_log_path = path;
}

static const char *openai_activity_path(void)
{
    if (openai_test_log_path != NULL &&
        *openai_test_log_path != '\0') {
        return openai_test_log_path;
    }

    return OPENAI_ACTIVITY_LOG_FILE;
}

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

    if (openai_test_log_path != NULL) {
        return 1;
    }

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

    file = fopen(openai_activity_path(), "w");

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

    file = fopen(openai_activity_path(), "a");

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


void openai_log_repair_attempt(unsigned int attempt,
                               unsigned long plan_hash,
                               int build_status,
                               int rollback,
                               const char *outcome)
{
    FILE *file;
    time_t now;
    struct tm *local_time;
    char timestamp[32];

    if (outcome == NULL) {
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

    file = fopen(openai_activity_path(), "a");
    if (file == NULL) {
        return;
    }

    (void)fprintf(
        file,
        "%s workflow=AGENT/REPAIR event=repair_attempt "
        "attempt=%u plan=%08lX build=%d rollback=%d outcome=%s\n",
        timestamp,
        attempt,
        plan_hash,
        build_status,
        rollback,
        outcome
    );

    (void)fclose(file);
    openai_state_save();
}

int openai_last_repair_record(char *output,
                              size_t output_size)
{
    FILE *file;
    char line[1024];
    int found;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    file = fopen(openai_activity_path(), "r");

    if (file == NULL) {
        return 0;
    }

    found = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line,
                   "workflow=AGENT/REPAIR event=repair_attempt ") != NULL) {
            size_t length;

            length = strlen(line);
            if (length >= output_size) {
                length = output_size - 1U;
            }

            (void)memcpy(output, line, length);
            output[length] = '\0';
            found = 1;
        }
    }

    (void)fclose(file);
    return found;
}


typedef struct openai_repair_attempt_record {
    unsigned int attempt;
    unsigned long plan_hash;
    int build_status;
    int rollback;
    char outcome[32];
} openai_repair_attempt_record;

static int openai_parse_repair_record(
    const char *line,
    openai_repair_attempt_record *record)
{
    char timestamp[32];
    unsigned int attempt;
    unsigned long plan_hash;
    int build_status;
    int rollback;
    char outcome[32];
    int matched;

    if (line == NULL || record == NULL) {
        return 0;
    }

    matched = sscanf(
        line,
        "%31s workflow=AGENT/REPAIR event=repair_attempt "
        "attempt=%u plan=%lx build=%d rollback=%d outcome=%31s",
        timestamp,
        &attempt,
        &plan_hash,
        &build_status,
        &rollback,
        outcome
    );

    if (matched != 6) {
        return 0;
    }

    record->attempt = attempt;
    record->plan_hash = plan_hash;
    record->build_status = build_status;
    record->rollback = rollback;
    (void)strncpy(
        record->outcome,
        outcome,
        sizeof(record->outcome) - 1U
    );
    record->outcome[sizeof(record->outcome) - 1U] = '\0';
    return 1;
}

int openai_repair_status_text(char *output,
                              size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_attempt_record records[2];
    openai_repair_attempt_record parsed;
    unsigned int count;
    int found;
    int written;
    size_t used;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    file = fopen(openai_activity_path(), "r");

    if (file == NULL) {
        return 0;
    }

    count = 0U;
    found = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            count = 0U;
        }

        if (parsed.attempt >= 1U &&
            parsed.attempt <= 2U) {
            records[parsed.attempt - 1U] = parsed;
            if (parsed.attempt > count) {
                count = parsed.attempt;
            }
            found = 1;
        }
    }

    (void)fclose(file);

    if (!found || count == 0U) {
        return 0;
    }

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent repair status\n"
        "------------------------\n"
        "Attempts used: %u of 2\n",
        count
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    {
        unsigned int index;

        for (index = 0U; index < count; ++index) {
            const openai_repair_attempt_record *record;
            const char *build_name;
            const char *rollback_name;

            record = &records[index];
            build_name =
                (record->build_status & 1) != 0 ?
                "success" : "failure";
            rollback_name =
                openai_rollback_name(record->rollback);

            written = snprintf(
                output + used,
                output_size - used,
                "Attempt %u: plan %08lX, build %s "
                "(status %d), rollback %s, outcome %s\n",
                record->attempt,
                record->plan_hash,
                build_name,
                record->build_status,
                rollback_name,
                record->outcome
            );

            if (written < 0 ||
                (size_t)written >= output_size - used) {
                return 0;
            }
            used += (size_t)written;
        }
    }

    written = snprintf(
        output + used,
        output_size - used,
        "Final outcome: %s\n",
        records[count - 1U].outcome
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }

    return 1;
}

#define OPENAI_REPAIR_HISTORY_DEFAULT 5U
#define OPENAI_REPAIR_HISTORY_MAXIMUM 20U

static unsigned int openai_repair_history_override = 0U;

static int openai_history_limit_valid(const char *value)
{
    unsigned int result;
    const unsigned char *position;

    if (value == NULL || *value == '\0') {
        return 0;
    }

    result = 0U;
    position = (const unsigned char *)value;

    while (*position != (unsigned char)'\0') {
        unsigned int digit;

        if (*position < (unsigned char)'0' ||
            *position > (unsigned char)'9') {
            return 0;
        }

        digit = (unsigned int)(*position - (unsigned char)'0');

        if (result >
            (OPENAI_REPAIR_HISTORY_MAXIMUM - digit) / 10U) {
            return 0;
        }

        result = result * 10U + digit;
        ++position;
    }

    return
        result >= 1U &&
        result <= OPENAI_REPAIR_HISTORY_MAXIMUM;
}

static unsigned int openai_parse_history_limit(const char *value)
{
    unsigned int result;
    const unsigned char *position;

    if (!openai_history_limit_valid(value)) {
        return OPENAI_REPAIR_HISTORY_DEFAULT;
    }

    result = 0U;
    position = (const unsigned char *)value;

    while (*position != (unsigned char)'\0') {
        result =
            result * 10U +
            (unsigned int)(*position - (unsigned char)'0');
        ++position;
    }

    return result;
}

static unsigned int openai_repair_history_limit(void)
{
    if (openai_repair_history_override != 0U) {
        return openai_repair_history_override;
    }

    return openai_parse_history_limit(
        getenv("OVMS_AGENT_REPAIR_HISTORY_RUNS")
    );
}

unsigned int openai_test_history_limit(const char *value)
{
    return openai_parse_history_limit(value);
}

void openai_test_set_history_limit(unsigned int limit)
{
    if (limit >= 1U &&
        limit <= OPENAI_REPAIR_HISTORY_MAXIMUM) {
        openai_repair_history_override = limit;
    } else {
        openai_repair_history_override = 0U;
    }
}


typedef struct openai_repair_run_record {
    openai_repair_attempt_record attempts[2];
    unsigned int count;
} openai_repair_run_record;

int openai_repair_history_text(char *output,
                               size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_run_record runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    openai_repair_attempt_record parsed;
    unsigned int run_count;
    unsigned int history_limit;
    int written;
    size_t used;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    (void)memset(runs, 0, sizeof(runs));
    run_count = 0U;
    history_limit = openai_repair_history_limit();

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        openai_repair_run_record *run;

        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            if (run_count < history_limit) {
                ++run_count;
            } else {
                unsigned int index;

                for (index = 1U;
                     index < history_limit;
                     ++index) {
                    runs[index - 1U] = runs[index];
                }
            }

            run = &runs[run_count - 1U];
            (void)memset(run, 0, sizeof(*run));
            run->attempts[0] = parsed;
            run->count = 1U;
            continue;
        }

        if (parsed.attempt == 2U && run_count > 0U) {
            run = &runs[run_count - 1U];
            run->attempts[1] = parsed;
            if (run->count < 2U) {
                run->count = 2U;
            }
        }
    }

    (void)fclose(file);

    if (run_count == 0U) {
        return 0;
    }

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent repair history\n"
        "-------------------------\n"
        "Recent runs: %u (maximum %u)\n",
        run_count,
        history_limit
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    {
        unsigned int display_run;

        for (display_run = 0U;
             display_run < run_count;
             ++display_run) {
            const openai_repair_run_record *run;
            unsigned int stored_index;
            unsigned int attempt_index;

            stored_index = run_count - 1U - display_run;
            run = &runs[stored_index];

            written = snprintf(
                output + used,
                output_size - used,
                "\nRun %u%s\n",
                display_run + 1U,
                display_run == 0U ? " (newest)" : ""
            );

            if (written < 0 ||
                (size_t)written >= output_size - used) {
                return 0;
            }
            used += (size_t)written;

            for (attempt_index = 0U;
                 attempt_index < run->count;
                 ++attempt_index) {
                const openai_repair_attempt_record *record;
                const char *build_name;
                const char *rollback_name;

                record = &run->attempts[attempt_index];
                build_name =
                    (record->build_status & 1) != 0 ?
                    "success" : "failure";
                rollback_name =
                    openai_rollback_name(record->rollback);

                written = snprintf(
                    output + used,
                    output_size - used,
                    "  Attempt %u: plan %08lX, build %s "
                    "(status %d), rollback %s, outcome %s\n",
                    record->attempt,
                    record->plan_hash,
                    build_name,
                    record->build_status,
                    rollback_name,
                    record->outcome
                );

                if (written < 0 ||
                    (size_t)written >= output_size - used) {
                    return 0;
                }
                used += (size_t)written;
            }

            written = snprintf(
                output + used,
                output_size - used,
                "  Final outcome: %s\n",
                run->attempts[run->count - 1U].outcome
            );

            if (written < 0 ||
                (size_t)written >= output_size - used) {
                return 0;
            }
            used += (size_t)written;
        }
    }

    return 1;
}


int openai_repair_failures_text(char *output,
                                size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_run_record runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    openai_repair_attempt_record parsed;
    unsigned int run_count;
    unsigned int history_limit;
    unsigned int failure_count;
    int written;
    size_t used;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    (void)memset(runs, 0, sizeof(runs));
    run_count = 0U;
    history_limit = openai_repair_history_limit();

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        openai_repair_run_record *run;

        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            if (run_count < history_limit) {
                ++run_count;
            } else {
                unsigned int index;

                for (index = 1U;
                     index < history_limit;
                     ++index) {
                    runs[index - 1U] = runs[index];
                }
            }

            run = &runs[run_count - 1U];
            (void)memset(run, 0, sizeof(*run));
            run->attempts[0] = parsed;
            run->count = 1U;
            continue;
        }

        if (parsed.attempt == 2U && run_count > 0U) {
            run = &runs[run_count - 1U];
            run->attempts[1] = parsed;
            if (run->count < 2U) {
                run->count = 2U;
            }
        }
    }

    (void)fclose(file);

    if (run_count == 0U) {
        return 0;
    }

    failure_count = 0U;

    {
        unsigned int index;

        for (index = 0U; index < run_count; ++index) {
            const openai_repair_run_record *run;
            const char *outcome;

            run = &runs[index];
            if (run->count == 0U) {
                continue;
            }

            outcome =
                run->attempts[run->count - 1U].outcome;

            if (strcmp(outcome, "committed") != 0) {
                ++failure_count;
            }
        }
    }

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent failed repair history\n"
        "--------------------------------\n"
        "History window: %u recent runs (maximum %u)\n"
        "Failed runs in window: %u\n",
        run_count,
        history_limit,
        failure_count
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    if (failure_count == 0U) {
        written = snprintf(
            output + used,
            output_size - used,
            "No failed repair runs are present in the recent window.\n"
        );

        return
            written >= 0 &&
            (size_t)written < output_size - used;
    }

    {
        unsigned int display_run;
        unsigned int failure_number;

        failure_number = 0U;

        for (display_run = 0U;
             display_run < run_count;
             ++display_run) {
            const openai_repair_run_record *run;
            unsigned int stored_index;
            unsigned int attempt_index;
            const char *final_outcome;

            stored_index = run_count - 1U - display_run;
            run = &runs[stored_index];

            if (run->count == 0U) {
                continue;
            }

            final_outcome =
                run->attempts[run->count - 1U].outcome;

            if (strcmp(final_outcome, "committed") == 0) {
                continue;
            }

            ++failure_number;

            written = snprintf(
                output + used,
                output_size - used,
                "\nFailure %u%s (history run %u)\n",
                failure_number,
                failure_number == 1U ? " (newest)" : "",
                display_run + 1U
            );

            if (written < 0 ||
                (size_t)written >= output_size - used) {
                return 0;
            }
            used += (size_t)written;

            for (attempt_index = 0U;
                 attempt_index < run->count;
                 ++attempt_index) {
                const openai_repair_attempt_record *record;
                const char *build_name;
                const char *rollback_name;

                record = &run->attempts[attempt_index];
                build_name =
                    (record->build_status & 1) != 0 ?
                    "success" : "failure";
                rollback_name =
                    openai_rollback_name(record->rollback);

                written = snprintf(
                    output + used,
                    output_size - used,
                    "  Attempt %u: plan %08lX, build %s "
                    "(status %d), rollback %s, outcome %s\n",
                    record->attempt,
                    record->plan_hash,
                    build_name,
                    record->build_status,
                    rollback_name,
                    record->outcome
                );

                if (written < 0 ||
                    (size_t)written >= output_size - used) {
                    return 0;
                }
                used += (size_t)written;
            }

            written = snprintf(
                output + used,
                output_size - used,
                "  Final outcome: %s\n",
                final_outcome
            );

            if (written < 0 ||
                (size_t)written >= output_size - used) {
                return 0;
            }
            used += (size_t)written;
        }
    }

    return 1;
}



int openai_repair_show_text(unsigned long plan_hash,
                            char *output,
                            size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_attempt_record parsed;
    openai_repair_run_record current;
    openai_repair_run_record selected;
    unsigned int run_number;
    unsigned int selected_run;
    unsigned int total_runs;
    int current_matches;
    int selected_found;
    int written;
    size_t used;
    unsigned int index;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    (void)memset(&current, 0, sizeof(current));
    (void)memset(&selected, 0, sizeof(selected));
    run_number = 0U;
    selected_run = 0U;
    total_runs = 0U;
    current_matches = 0;
    selected_found = 0;

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            if (run_number > 0U && current_matches) {
                selected = current;
                selected_run = run_number;
                selected_found = 1;
            }

            ++run_number;
            total_runs = run_number;
            (void)memset(&current, 0, sizeof(current));
            current.attempts[0] = parsed;
            current.count = 1U;
            current_matches =
                parsed.plan_hash == plan_hash;
            continue;
        }

        if (parsed.attempt == 2U && run_number > 0U) {
            current.attempts[1] = parsed;
            current.count = 2U;

            if (parsed.plan_hash == plan_hash) {
                current_matches = 1;
            }
        }
    }

    if (run_number > 0U && current_matches) {
        selected = current;
        selected_run = run_number;
        selected_found = 1;
    }

    (void)fclose(file);

    if (!selected_found || selected.count == 0U) {
        return 0;
    }

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent repair detail\n"
        "------------------------\n"
        "Requested plan: %08lX\n"
        "Persisted run: %u of %u (chronological)\n"
        "Attempts used: %u of 2\n",
        plan_hash,
        selected_run,
        total_runs,
        selected.count
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    for (index = 0U; index < selected.count; ++index) {
        const openai_repair_attempt_record *record;
        const char *build_name;
        const char *rollback_name;
        const char *requested;

        record = &selected.attempts[index];
        build_name =
            (record->build_status & 1) != 0 ?
            "success" : "failure";
        rollback_name =
            openai_rollback_name(record->rollback);
        requested =
            record->plan_hash == plan_hash ?
            " [requested]" : "";

        written = snprintf(
            output + used,
            output_size - used,
            "Attempt %u: plan %08lX%s, build %s "
            "(status %d), rollback %s, outcome %s\n",
            record->attempt,
            record->plan_hash,
            requested,
            build_name,
            record->build_status,
            rollback_name,
            record->outcome
        );

        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }
        used += (size_t)written;
    }

    written = snprintf(
        output + used,
        output_size - used,
        "Final outcome: %s\n",
        selected.attempts[selected.count - 1U].outcome
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }

    return 1;
}

static int openai_parse_plan_hash(const char *text,
                                  unsigned long *hash_out)
{
    const char *position;
    unsigned long value;
    unsigned int count;

    if (text == NULL || hash_out == NULL) {
        return 0;
    }

    position = text;
    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    value = 0UL;
    count = 0U;

    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t') {
        unsigned int digit;

        if (*position >= '0' && *position <= '9') {
            digit = (unsigned int)(*position - '0');
        } else if (*position >= 'A' && *position <= 'F') {
            digit = (unsigned int)(*position - 'A') + 10U;
        } else if (*position >= 'a' && *position <= 'f') {
            digit = (unsigned int)(*position - 'a') + 10U;
        } else {
            return 0;
        }

        if (count >= 8U) {
            return 0;
        }

        value = (value << 4) | (unsigned long)digit;
        ++count;
        ++position;
    }

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    if (count != 8U || *position != '\0') {
        return 0;
    }

    *hash_out = value;
    return 1;
}

void openai_show_repair_plan(const char *arguments)
{
    unsigned long plan_hash;
    char detail[4096];

    if (!openai_parse_plan_hash(arguments, &plan_hash)) {
        (void)puts(
            "Usage: AGENT/REPAIR/SHOW <8-digit-plan-hash>"
        );
        return;
    }

    if (!openai_repair_show_text(
            plan_hash,
            detail,
            sizeof(detail))) {
        (void)printf(
            "No persisted AGENT/REPAIR record was found "
            "for plan %08lX.\n",
            plan_hash
        );
        return;
    }

    (void)fputs(detail, stdout);
}

void openai_show_repair_failures(void)
{
    char history[32768];

    if (!openai_repair_failures_text(
            history,
            sizeof(history))) {
        (void)puts(
            "No persisted AGENT/REPAIR history is available."
        );
        return;
    }

    (void)fputs(history, stdout);
}


static int openai_format_repair_config(const char *value,
                                       char *output,
                                       size_t output_size)
{
    unsigned int resolved;
    const char *source;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (openai_history_limit_valid(value)) {
        resolved = openai_parse_history_limit(value);
        source = "environment";
    } else {
        resolved = OPENAI_REPAIR_HISTORY_DEFAULT;
        source = "default";
    }

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair configuration\n"
        "-------------------------------\n"
        "History window:       %u\n"
        "Default window:       %u\n"
        "Maximum window:       %u\n"
        "Source:               %s\n"
        "Environment variable: OVMS_AGENT_REPAIR_HISTORY_RUNS\n",
        resolved,
        OPENAI_REPAIR_HISTORY_DEFAULT,
        OPENAI_REPAIR_HISTORY_MAXIMUM,
        source
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

int openai_repair_config_text(char *output,
                              size_t output_size)
{
    if (openai_repair_history_override != 0U) {
        int written;

        written = snprintf(
            output,
            output_size,
            "OVMS Agent repair configuration\n"
            "-------------------------------\n"
            "History window:       %u\n"
            "Default window:       %u\n"
            "Maximum window:       %u\n"
            "Source:               test override\n"
            "Environment variable: OVMS_AGENT_REPAIR_HISTORY_RUNS\n",
            openai_repair_history_override,
            OPENAI_REPAIR_HISTORY_DEFAULT,
            OPENAI_REPAIR_HISTORY_MAXIMUM
        );

        return
            written >= 0 &&
            (size_t)written < output_size;
    }

    return openai_format_repair_config(
        getenv("OVMS_AGENT_REPAIR_HISTORY_RUNS"),
        output,
        output_size
    );
}

int openai_test_repair_config_text(const char *value,
                                   char *output,
                                   size_t output_size)
{
    return openai_format_repair_config(
        value,
        output,
        output_size
    );
}

void openai_show_repair_config(void)
{
    char configuration[1024];

    if (!openai_repair_config_text(
            configuration,
            sizeof(configuration))) {
        (void)puts(
            "Unable to resolve AGENT/REPAIR configuration."
        );
        return;
    }

    (void)fputs(configuration, stdout);
}


int openai_repair_stats_text(char *output,
                             size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_run_record runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    openai_repair_attempt_record parsed;
    unsigned int run_count;
    unsigned int history_limit;
    unsigned int committed_runs;
    unsigned int failed_runs;
    unsigned int first_successes;
    unsigned int second_recoveries;
    unsigned int two_attempt_failures;
    unsigned int rollback_operations;
    unsigned int success_rate;
    int written;
    unsigned int run_index;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    (void)memset(runs, 0, sizeof(runs));
    run_count = 0U;
    history_limit = openai_repair_history_limit();

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        openai_repair_run_record *run;

        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            if (run_count < history_limit) {
                ++run_count;
            } else {
                unsigned int index;

                for (index = 1U;
                     index < history_limit;
                     ++index) {
                    runs[index - 1U] = runs[index];
                }
            }

            run = &runs[run_count - 1U];
            (void)memset(run, 0, sizeof(*run));
            run->attempts[0] = parsed;
            run->count = 1U;
            continue;
        }

        if (parsed.attempt == 2U && run_count > 0U) {
            run = &runs[run_count - 1U];
            run->attempts[1] = parsed;
            run->count = 2U;
        }
    }

    (void)fclose(file);

    if (run_count == 0U) {
        return 0;
    }

    committed_runs = 0U;
    failed_runs = 0U;
    first_successes = 0U;
    second_recoveries = 0U;
    two_attempt_failures = 0U;
    rollback_operations = 0U;

    for (run_index = 0U;
         run_index < run_count;
         ++run_index) {
        const openai_repair_run_record *run;
        const char *final_outcome;
        unsigned int attempt_index;
        int committed;

        run = &runs[run_index];
        if (run->count == 0U) {
            continue;
        }

        final_outcome =
            run->attempts[run->count - 1U].outcome;
        committed =
            strcmp(final_outcome, "committed") == 0;

        if (committed) {
            ++committed_runs;

            if (run->count == 1U) {
                ++first_successes;
            } else if (run->count == 2U) {
                ++second_recoveries;
            }
        } else {
            ++failed_runs;

            if (run->count == 2U) {
                ++two_attempt_failures;
            }
        }

        for (attempt_index = 0U;
             attempt_index < run->count;
             ++attempt_index) {
            int rollback;

            rollback = run->attempts[attempt_index].rollback;

            /*
             * Persisted rollback states are stable integers:
             *   2 = succeeded
             *   3 = failed
             * Count both as rollback operations without depending on
             * rollback macros that are private to another module.
             */
            if (rollback == 2 || rollback == 3) {
                ++rollback_operations;
            }
        }
    }

    success_rate =
        (committed_runs * 100U) / run_count;

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair statistics\n"
        "----------------------------\n"
        "Runs analyzed:              %u\n"
        "Committed runs:             %u\n"
        "Failed/rolled-back runs:    %u\n"
        "First-attempt successes:    %u\n"
        "Second-attempt recoveries:  %u\n"
        "Two-attempt failures:       %u\n"
        "Rollback operations:        %u\n"
        "Success rate:               %u%%\n",
        run_count,
        committed_runs,
        failed_runs,
        first_successes,
        second_recoveries,
        two_attempt_failures,
        rollback_operations,
        success_rate
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_repair_stats(void)
{
    char statistics[2048];

    if (!openai_repair_stats_text(
            statistics,
            sizeof(statistics))) {
        (void)puts(
            "No persisted AGENT/REPAIR history is available."
        );
        return;
    }

    (void)fputs(statistics, stdout);
}

static int openai_repair_export_path_safe(const char *path)
{
    const unsigned char *position;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (strstr(path, "..") != NULL) {
        return 0;
    }

    for (position = (const unsigned char *)path;
         *position != (unsigned char)'\0';
         ++position) {
        if (*position < (unsigned char)' ' ||
            *position == (unsigned char)127 ||
            *position == (unsigned char)'*' ||
            *position == (unsigned char)'%' ||
            *position == (unsigned char)';' ||
            *position == (unsigned char)'|' ||
            *position == (unsigned char)'&' ||
            *position == (unsigned char)'<' ||
            *position == (unsigned char)'>' ||
            *position == (unsigned char)'"' ||
            *position == (unsigned char)'\'' ||
            *position == (unsigned char)'`' ||
            *position == (unsigned char)'/' ||
            *position == (unsigned char)'\\') {
            return 0;
        }
    }

    return 1;
}

int openai_repair_export_file(const char *path,
                              int allow_overwrite)
{
    FILE *probe;
    FILE *file;
    char history[32768];

    if (!openai_repair_export_path_safe(path)) {
        return 0;
    }

    if (!openai_repair_history_text(
            history,
            sizeof(history))) {
        return 0;
    }

    probe = fopen(path, "r");
    if (probe != NULL) {
        (void)fclose(probe);

        if (!allow_overwrite) {
            return 0;
        }
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(history, file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return 1;
}

void openai_export_repair_history(const char *arguments)
{
    const char *path;
    FILE *probe;
    char answer[32];
    int exists;

    path = arguments;

    while (path != NULL &&
           (*path == ' ' || *path == '\t')) {
        ++path;
    }

    if (!openai_repair_export_path_safe(path)) {
        (void)puts(
            "Usage: AGENT/REPAIR/EXPORT <safe-filespec>"
        );
        return;
    }

    probe = fopen(path, "r");
    exists = probe != NULL;

    if (probe != NULL) {
        (void)fclose(probe);
    }

    if (exists) {
        (void)printf(
            "Replace existing repair-history export %s [y/N]? ",
            path
        );
        (void)fflush(stdout);

        if (fgets(answer, sizeof(answer), stdin) == NULL) {
            (void)putchar('\n');
            return;
        }

        if (answer[0] != 'y' && answer[0] != 'Y') {
            (void)puts("Repair-history export cancelled.");
            return;
        }
    }

    if (!openai_repair_export_file(path, exists ? 1 : 0)) {
        (void)printf(
            "Unable to export repair history to %s.\n",
            path
        );
        return;
    }

    (void)printf(
        "Repair history exported to %s.\n",
        path
    );
}

void openai_show_repair_history(void)
{
    char history[32768];

    if (!openai_repair_history_text(
            history,
            sizeof(history))) {
        (void)puts("No persisted AGENT/REPAIR history is available.");
        return;
    }

    (void)fputs(history, stdout);
}

void openai_show_repair_status(void)
{
    char summary[2048];

    if (!openai_repair_status_text(
            summary,
            sizeof(summary))) {
        (void)puts("No persisted AGENT/REPAIR history is available.");
        return;
    }

    (void)fputs(summary, stdout);
}

void openai_show_log(void)
{
    FILE *file;
    char line[1024];

    file = fopen(openai_activity_path(), "r");

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

    file = fopen(openai_activity_path(), "w");

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

    file = fopen(openai_activity_path(), "r");

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
