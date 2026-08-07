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


static int openai_repair_line_candidate(const char *line)
{
    if (line == NULL) {
        return 0;
    }

    return
        strstr(
            line,
            "workflow=AGENT/REPAIR event=repair_attempt "
        ) != NULL;
}

static int openai_plan_token_valid(const char *line)
{
    const char *plan;
    unsigned int index;

    if (line == NULL) {
        return 0;
    }

    plan = strstr(line, " plan=");
    if (plan == NULL) {
        return 0;
    }

    plan += 6;

    for (index = 0U; index < 8U; ++index) {
        unsigned char ch;

        ch = (unsigned char)plan[index];

        if (!((ch >= (unsigned char)'0' &&
               ch <= (unsigned char)'9') ||
              (ch >= (unsigned char)'A' &&
               ch <= (unsigned char)'F') ||
              (ch >= (unsigned char)'a' &&
               ch <= (unsigned char)'f'))) {
            return 0;
        }
    }

    return plan[8] == ' ';
}

static int openai_repair_outcome_valid(const char *outcome)
{
    if (outcome == NULL) {
        return 0;
    }

    return
        strcmp(outcome, "committed") == 0 ||
        strcmp(outcome, "rolled_back") == 0 ||
        strcmp(outcome, "unsafe") == 0;
}

int openai_repair_check_text(char *output,
                             size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_attempt_record parsed;
    unsigned int record_count;
    unsigned int run_count;
    unsigned int malformed_count;
    unsigned int sequence_errors;
    unsigned int outcome_errors;
    unsigned int plan_errors;
    int active_run;
    int saw_attempt2;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    record_count = 0U;
    run_count = 0U;
    malformed_count = 0U;
    sequence_errors = 0U;
    outcome_errors = 0U;
    plan_errors = 0U;
    active_run = 0;
    saw_attempt2 = 0;

    file = fopen(openai_activity_path(), "r");

    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (!openai_repair_line_candidate(line)) {
                continue;
            }

            ++record_count;

            if (!openai_parse_repair_record(line, &parsed)) {
                ++malformed_count;
                continue;
            }

            if (!openai_plan_token_valid(line)) {
                ++plan_errors;
            }

            if (!openai_repair_outcome_valid(parsed.outcome)) {
                ++outcome_errors;
            }

            if (parsed.attempt == 1U) {
                ++run_count;
                active_run = 1;
                saw_attempt2 = 0;
            } else if (parsed.attempt == 2U) {
                if (!active_run || saw_attempt2) {
                    ++sequence_errors;
                } else {
                    saw_attempt2 = 1;
                }
            } else {
                ++sequence_errors;
            }
        }

        (void)fclose(file);
    }

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history check\n"
        "-------------------------------\n"
        "Records scanned:     %u\n"
        "Runs scanned:        %u\n"
        "Malformed records:   %u\n"
        "Sequence errors:     %u\n"
        "Outcome errors:      %u\n"
        "Plan hash errors:    %u\n"
        "Integrity:           %s\n",
        record_count,
        run_count,
        malformed_count,
        sequence_errors,
        outcome_errors,
        plan_errors,
        (malformed_count == 0U &&
         sequence_errors == 0U &&
         outcome_errors == 0U &&
         plan_errors == 0U) ? "PASS" : "FAIL"
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_repair_check(void)
{
    char report[2048];

    if (!openai_repair_check_text(
            report,
            sizeof(report))) {
        (void)puts(
            "Unable to check AGENT/REPAIR history."
        );
        return;
    }

    (void)fputs(report, stdout);
}


int openai_repair_info_text(char *output,
                            size_t output_size);

int openai_repair_diag_text(char *output,
                            size_t output_size)
{
    char information[2048];
    char check[2048];
    const char *info_body;
    const char *check_body;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (!openai_repair_info_text(
            information,
            sizeof(information)) ||
        !openai_repair_check_text(
            check,
            sizeof(check))) {
        return 0;
    }

    info_body = strchr(information, '\n');
    if (info_body == NULL) {
        return 0;
    }
    info_body = strchr(info_body + 1, '\n');
    if (info_body == NULL) {
        return 0;
    }
    ++info_body;

    check_body = strchr(check, '\n');
    if (check_body == NULL) {
        return 0;
    }
    check_body = strchr(check_body + 1, '\n');
    if (check_body == NULL) {
        return 0;
    }
    ++check_body;

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history diagnostics\n"
        "-------------------------------------\n"
        "%s\n"
        "%s",
        info_body,
        check_body
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_repair_diag(void)
{
    char report[4096];

    if (!openai_repair_diag_text(
            report,
            sizeof(report))) {
        (void)puts(
            "Unable to build AGENT/REPAIR diagnostics."
        );
        return;
    }

    (void)fputs(report, stdout);
}

int openai_repair_count_text(char *output,
                             size_t output_size)
{
    FILE *file;
    char line[1024];
    openai_repair_attempt_record parsed;
    unsigned int record_count;
    unsigned int run_count;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    record_count = 0U;
    run_count = 0U;

    file = fopen(openai_activity_path(), "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (!openai_parse_repair_record(line, &parsed)) {
                continue;
            }

            ++record_count;
            if (parsed.attempt == 1U) {
                ++run_count;
            }
        }

        (void)fclose(file);
    }

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history count\n"
        "-------------------------------\n"
        "Repair records: %u\n"
        "Repair runs:    %u\n",
        record_count,
        run_count
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_repair_count(void)
{
    char report[512];

    if (!openai_repair_count_text(
            report,
            sizeof(report))) {
        (void)puts(
            "Unable to count AGENT/REPAIR history."
        );
        return;
    }

    (void)fputs(report, stdout);
}

static int openai_edge_run_text(int newest,
                                char *output,
                                size_t output_size)
{
    FILE *file;
    char line[1024];
    char timestamp[32];
    char current_time[32];
    char selected_time[32];
    openai_repair_attempt_record parsed;
    openai_repair_attempt_record current[2];
    openai_repair_attempt_record selected[2];
    unsigned int current_count;
    unsigned int selected_count;
    int have_current;
    int have_selected;
    int written;
    size_t used;
    unsigned int index;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    current_time[0] = '\0';
    selected_time[0] = '\0';
    (void)memset(current, 0, sizeof(current));
    (void)memset(selected, 0, sizeof(selected));
    current_count = 0U;
    selected_count = 0U;
    have_current = 0;
    have_selected = 0;

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            /*
             * A new attempt 1 closes the previous run.
             * For newest, keep replacing the selected run.
             * For oldest, capture only the first completed run.
             */
            if (have_current) {
                if (newest || !have_selected) {
                    selected[0] = current[0];
                    selected[1] = current[1];
                    selected_count = current_count;
                    (void)strncpy(
                        selected_time,
                        current_time,
                        sizeof(selected_time) - 1U
                    );
                    selected_time[
                        sizeof(selected_time) - 1U
                    ] = '\0';
                    have_selected = 1;
                }
            }

            (void)memset(current, 0, sizeof(current));
            current[0] = parsed;
            current_count = 1U;
            have_current = 1;
            current_time[0] = '\0';

            if (sscanf(line, "%31s", timestamp) == 1) {
                (void)strncpy(
                    current_time,
                    timestamp,
                    sizeof(current_time) - 1U
                );
                current_time[
                    sizeof(current_time) - 1U
                ] = '\0';
            }

            continue;
        }

        if (parsed.attempt == 2U && have_current) {
            current[1] = parsed;
            current_count = 2U;
        }
    }

    (void)fclose(file);

    /*
     * Final run is closed by EOF.
     * Newest always selects it. Oldest selects it only when it was
     * the first and only run.
     */
    if (have_current &&
        (newest || !have_selected)) {
        selected[0] = current[0];
        selected[1] = current[1];
        selected_count = current_count;
        (void)strncpy(
            selected_time,
            current_time,
            sizeof(selected_time) - 1U
        );
        selected_time[
            sizeof(selected_time) - 1U
        ] = '\0';
        have_selected = 1;
    }

    if (!have_selected || selected_count == 0U) {
        return 0;
    }

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent %s repair run\n"
        "----------------------------\n"
        "Started: %s\n"
        "Attempts used: %u of 2\n",
        newest ? "latest" : "oldest",
        selected_time[0] != '\0' ?
            selected_time : "unknown",
        selected_count
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    for (index = 0U; index < selected_count; ++index) {
        const openai_repair_attempt_record *record;
        const char *build_name;
        const char *rollback_name;

        record = &selected[index];
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

    written = snprintf(
        output + used,
        output_size - used,
        "Final outcome: %s\n",
        selected[selected_count - 1U].outcome
    );

    return
        written >= 0 &&
        (size_t)written < output_size - used;
}

int openai_repair_latest_text(char *output,
                              size_t output_size)
{
    return openai_edge_run_text(
        1,
        output,
        output_size
    );
}

void openai_show_repair_latest(void)
{
    char report[2048];

    if (!openai_repair_latest_text(
            report,
            sizeof(report))) {
        (void)puts("No persisted AGENT/REPAIR history is available.");
        return;
    }

    (void)fputs(report, stdout);
}

int openai_repair_oldest_text(char *output,
                              size_t output_size)
{
    return openai_edge_run_text(
        0,
        output,
        output_size
    );
}

void openai_show_repair_oldest(void)
{
    char report[2048];

    if (!openai_repair_oldest_text(
            report,
            sizeof(report))) {
        (void)puts("No persisted AGENT/REPAIR history is available.");
        return;
    }

    (void)fputs(report, stdout);
}


int openai_repair_info_text(char *output,
                            size_t output_size)
{
    FILE *file;
    char line[1024];
    char timestamp[32];
    char oldest[32];
    char newest[32];
    openai_repair_attempt_record parsed;
    unsigned int record_count;
    unsigned int run_count;
    unsigned int history_limit;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    oldest[0] = '\0';
    newest[0] = '\0';
    record_count = 0U;
    run_count = 0U;
    history_limit = openai_repair_history_limit();

    file = fopen(openai_activity_path(), "r");

    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (!openai_parse_repair_record(line, &parsed)) {
                continue;
            }

            if (sscanf(line, "%31s", timestamp) != 1) {
                continue;
            }

            if (record_count == 0U) {
                (void)strncpy(
                    oldest,
                    timestamp,
                    sizeof(oldest) - 1U
                );
                oldest[sizeof(oldest) - 1U] = '\0';
            }

            (void)strncpy(
                newest,
                timestamp,
                sizeof(newest) - 1U
            );
            newest[sizeof(newest) - 1U] = '\0';

            ++record_count;

            if (parsed.attempt == 1U) {
                ++run_count;
            }
        }

        (void)fclose(file);
    }

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history information\n"
        "-------------------------------------\n"
        "Activity log:        %s\n"
        "Repair records:      %u\n"
        "Repair runs:         %u\n"
        "Oldest repair:       %s\n"
        "Newest repair:       %s\n"
        "History window:      %u\n"
        "History available:   %s\n",
        openai_activity_path(),
        record_count,
        run_count,
        record_count > 0U ? oldest : "none",
        record_count > 0U ? newest : "none",
        history_limit,
        record_count > 0U ? "yes" : "no"
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_repair_info(void)
{
    char information[2048];

    if (!openai_repair_info_text(
            information,
            sizeof(information))) {
        (void)puts(
            "Unable to inspect AGENT/REPAIR history."
        );
        return;
    }

    (void)fputs(information, stdout);
}


typedef struct openai_repair_run_record {
    openai_repair_attempt_record attempts[2];
    unsigned int count;
} openai_repair_run_record;

typedef struct openai_query_run {
    openai_repair_run_record run;
    char started[32];
} openai_query_run;

static const char *openai_skip_spaces(const char *text)
{
    if (text == NULL) {
        return "";
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    return text;
}

static int openai_arg_is_single(const char *text)
{
    const unsigned char *position;

    if (text == NULL || *text == '\0') {
        return 0;
    }

    position = (const unsigned char *)text;

    while (*position != (unsigned char)'\0') {
        if (*position == (unsigned char)' ' ||
            *position == (unsigned char)'\t' ||
            *position == (unsigned char)'\r' ||
            *position == (unsigned char)'\n') {
            return 0;
        }
        ++position;
    }

    return 1;
}

static unsigned int openai_collect_query_runs(
    openai_query_run *runs,
    unsigned int capacity)
{
    FILE *file;
    char line[1024];
    char timestamp[32];
    openai_repair_attempt_record parsed;
    unsigned int run_count;

    if (runs == NULL || capacity == 0U) {
        return 0U;
    }

    (void)memset(
        runs,
        0,
        sizeof(openai_query_run) * capacity
    );
    run_count = 0U;

    file = fopen(openai_activity_path(), "r");
    if (file == NULL) {
        return 0U;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        openai_query_run *entry;

        if (!openai_parse_repair_record(line, &parsed)) {
            continue;
        }

        if (parsed.attempt == 1U) {
            if (run_count < capacity) {
                ++run_count;
            } else {
                unsigned int index;

                for (index = 1U;
                     index < capacity;
                     ++index) {
                    runs[index - 1U] = runs[index];
                }
            }

            entry = &runs[run_count - 1U];
            (void)memset(entry, 0, sizeof(*entry));
            entry->run.attempts[0] = parsed;
            entry->run.count = 1U;

            if (sscanf(line, "%31s", timestamp) == 1) {
                (void)strncpy(
                    entry->started,
                    timestamp,
                    sizeof(entry->started) - 1U
                );
                entry->started[
                    sizeof(entry->started) - 1U
                ] = '\0';
            }

            continue;
        }

        if (parsed.attempt == 2U && run_count > 0U) {
            entry = &runs[run_count - 1U];
            entry->run.attempts[1] = parsed;
            entry->run.count = 2U;
        }
    }

    (void)fclose(file);
    return run_count;
}

static int openai_append_query_run(
    char *output,
    size_t output_size,
    size_t *used,
    unsigned int display_index,
    const openai_query_run *entry)
{
    unsigned int attempt_index;
    int written;

    if (output == NULL ||
        used == NULL ||
        entry == NULL ||
        entry->run.count == 0U) {
        return 0;
    }

    written = snprintf(
        output + *used,
        output_size - *used,
        "\nMatch %u\n"
        "  Started: %s\n",
        display_index,
        entry->started[0] != '\0' ?
            entry->started : "unknown"
    );

    if (written < 0 ||
        (size_t)written >= output_size - *used) {
        return 0;
    }
    *used += (size_t)written;

    for (attempt_index = 0U;
         attempt_index < entry->run.count;
         ++attempt_index) {
        const openai_repair_attempt_record *record;
        const char *build_name;
        const char *rollback_name;

        record = &entry->run.attempts[attempt_index];
        build_name =
            (record->build_status & 1) != 0 ?
            "success" : "failure";
        rollback_name =
            openai_rollback_name(record->rollback);

        written = snprintf(
            output + *used,
            output_size - *used,
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
            (size_t)written >= output_size - *used) {
            return 0;
        }
        *used += (size_t)written;
    }

    written = snprintf(
        output + *used,
        output_size - *used,
        "  Final outcome: %s\n",
        entry->run.attempts[
            entry->run.count - 1U
        ].outcome
    );

    if (written < 0 ||
        (size_t)written >= output_size - *used) {
        return 0;
    }

    *used += (size_t)written;
    return 1;
}

static int openai_hash_prefix_ok(const char *prefix)
{
    const unsigned char *position;
    unsigned int length;

    if (prefix == NULL || *prefix == '\0') {
        return 0;
    }

    length = 0U;
    position = (const unsigned char *)prefix;

    while (*position != (unsigned char)'\0') {
        if (!((*position >= (unsigned char)'0' &&
               *position <= (unsigned char)'9') ||
              (*position >= (unsigned char)'A' &&
               *position <= (unsigned char)'F') ||
              (*position >= (unsigned char)'a' &&
               *position <= (unsigned char)'f'))) {
            return 0;
        }

        ++length;
        if (length > 8U) {
            return 0;
        }
        ++position;
    }

    return length >= 1U;
}

static int openai_time_prefix_ok(const char *value)
{
    const unsigned char *position;
    unsigned int length;

    if (value == NULL || *value == '\0') {
        return 0;
    }

    length = 0U;
    position = (const unsigned char *)value;

    while (*position != (unsigned char)'\0') {
        unsigned char ch;

        ch = *position;

        if (!((ch >= (unsigned char)'0' &&
               ch <= (unsigned char)'9') ||
              ch == (unsigned char)'-' ||
              ch == (unsigned char)'T' ||
              ch == (unsigned char)':')) {
            return 0;
        }

        ++length;
        if (length > 19U) {
            return 0;
        }
        ++position;
    }

    return length >= 4U;
}

static int openai_hash_has_prefix(
    unsigned long hash,
    const char *prefix)
{
    char text[16];
    size_t length;

    if (prefix == NULL) {
        return 0;
    }

    (void)snprintf(
        text,
        sizeof(text),
        "%08lX",
        hash
    );

    length = strlen(prefix);

    return strncmp(text, prefix, length) == 0;
}

static int openai_hash_prefix_ci(
    unsigned long hash,
    const char *prefix)
{
    char upper_prefix[16];
    size_t index;
    size_t length;

    if (!openai_hash_prefix_ok(prefix)) {
        return 0;
    }

    length = strlen(prefix);

    for (index = 0U; index < length; ++index) {
        unsigned char ch;

        ch = (unsigned char)prefix[index];

        if (ch >= (unsigned char)'a' &&
            ch <= (unsigned char)'f') {
            ch = (unsigned char)(
                ch - (unsigned char)'a' +
                (unsigned char)'A'
            );
        }

        upper_prefix[index] = (char)ch;
    }
    upper_prefix[length] = '\0';

    return openai_hash_has_prefix(
        hash,
        upper_prefix
    );
}

static int openai_query_header(
    char *output,
    size_t output_size,
    const char *label,
    const char *value,
    unsigned int history_limit,
    size_t *used)
{
    int written;

    if (output == NULL ||
        label == NULL ||
        value == NULL ||
        used == NULL) {
        return 0;
    }

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history query\n"
        "-------------------------------\n"
        "Filter: %s\n"
        "Value: %s\n"
        "History window: %u\n",
        label,
        value,
        history_limit
    );

    if (written < 0 ||
        (size_t)written >= output_size) {
        return 0;
    }

    *used = (size_t)written;
    return 1;
}

int openai_query_outcome_text(
    const char *arguments,
    char *output,
    size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    const char *value;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;
    unsigned int match_count;
    size_t used;
    int written;

    value = openai_skip_spaces(arguments);

    if (!openai_arg_is_single(value) ||
        !(strcmp(value, "committed") == 0 ||
          strcmp(value, "rolled_back") == 0 ||
          strcmp(value, "unsafe") == 0)) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    if (!openai_query_header(
            output,
            output_size,
            "final outcome",
            value,
            history_limit,
            &used)) {
        return 0;
    }

    match_count = 0U;

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;

        stored_index = run_count - 1U - display_index;
        entry = &runs[stored_index];

        if (strcmp(
                entry->run.attempts[
                    entry->run.count - 1U
                ].outcome,
                value) != 0) {
            continue;
        }

        ++match_count;

        if (!openai_append_query_run(
                output,
                output_size,
                &used,
                match_count,
                entry)) {
            return 0;
        }
    }

    written = snprintf(
        output + used,
        output_size - used,
        "\nMatches: %u\n",
        match_count
    );

    return
        written >= 0 &&
        (size_t)written < output_size - used;
}

void openai_show_query_outcome(
    const char *arguments)
{
    char report[32768];

    if (!openai_query_outcome_text(
            arguments,
            report,
            sizeof(report))) {
        (void)puts(
            "Usage: AGENT/REPAIR/HISTORY/OUTCOME "
            "<committed|rolled_back|unsafe>"
        );
        return;
    }

    (void)fputs(report, stdout);
}

int openai_query_attempts_text(
    const char *arguments,
    char *output,
    size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    const char *value;
    unsigned int wanted;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;
    unsigned int match_count;
    size_t used;
    int written;

    value = openai_skip_spaces(arguments);

    if (!openai_arg_is_single(value) ||
        (strcmp(value, "1") != 0 &&
         strcmp(value, "2") != 0)) {
        return 0;
    }

    wanted = value[0] == '1' ? 1U : 2U;
    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    if (!openai_query_header(
            output,
            output_size,
            "attempt count",
            value,
            history_limit,
            &used)) {
        return 0;
    }

    match_count = 0U;

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;

        stored_index = run_count - 1U - display_index;
        entry = &runs[stored_index];

        if (entry->run.count != wanted) {
            continue;
        }

        ++match_count;

        if (!openai_append_query_run(
                output,
                output_size,
                &used,
                match_count,
                entry)) {
            return 0;
        }
    }

    written = snprintf(
        output + used,
        output_size - used,
        "\nMatches: %u\n",
        match_count
    );

    return
        written >= 0 &&
        (size_t)written < output_size - used;
}

void openai_show_query_attempts(
    const char *arguments)
{
    char report[32768];

    if (!openai_query_attempts_text(
            arguments,
            report,
            sizeof(report))) {
        (void)puts(
            "Usage: AGENT/REPAIR/HISTORY/ATTEMPTS <1|2>"
        );
        return;
    }

    (void)fputs(report, stdout);
}

int openai_query_plan_text(
    const char *arguments,
    char *output,
    size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    const char *value;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;
    unsigned int match_count;
    size_t used;
    int written;

    value = openai_skip_spaces(arguments);

    if (!openai_arg_is_single(value) ||
        !openai_hash_prefix_ok(value)) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    if (!openai_query_header(
            output,
            output_size,
            "plan hash prefix",
            value,
            history_limit,
            &used)) {
        return 0;
    }

    match_count = 0U;

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;
        unsigned int attempt_index;
        int matched;

        stored_index = run_count - 1U - display_index;
        entry = &runs[stored_index];
        matched = 0;

        for (attempt_index = 0U;
             attempt_index < entry->run.count;
             ++attempt_index) {
            if (openai_hash_prefix_ci(
                    entry->run.attempts[
                        attempt_index
                    ].plan_hash,
                    value)) {
                matched = 1;
                break;
            }
        }

        if (!matched) {
            continue;
        }

        ++match_count;

        if (!openai_append_query_run(
                output,
                output_size,
                &used,
                match_count,
                entry)) {
            return 0;
        }
    }

    written = snprintf(
        output + used,
        output_size - used,
        "\nMatches: %u\n",
        match_count
    );

    return
        written >= 0 &&
        (size_t)written < output_size - used;
}

void openai_show_query_plan(
    const char *arguments)
{
    char report[32768];

    if (!openai_query_plan_text(
            arguments,
            report,
            sizeof(report))) {
        (void)puts(
            "Usage: AGENT/REPAIR/HISTORY/PLAN <1-8 hex digits>"
        );
        return;
    }

    (void)fputs(report, stdout);
}

int openai_query_since_text(
    const char *arguments,
    char *output,
    size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    const char *value;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;
    unsigned int match_count;
    size_t used;
    int written;

    value = openai_skip_spaces(arguments);

    if (!openai_arg_is_single(value) ||
        !openai_time_prefix_ok(value)) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    if (!openai_query_header(
            output,
            output_size,
            "started since",
            value,
            history_limit,
            &used)) {
        return 0;
    }

    match_count = 0U;

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;

        stored_index = run_count - 1U - display_index;
        entry = &runs[stored_index];

        if (entry->started[0] == '\0' ||
            strcmp(entry->started, value) < 0) {
            continue;
        }

        ++match_count;

        if (!openai_append_query_run(
                output,
                output_size,
                &used,
                match_count,
                entry)) {
            return 0;
        }
    }

    written = snprintf(
        output + used,
        output_size - used,
        "\nMatches: %u\n",
        match_count
    );

    return
        written >= 0 &&
        (size_t)written < output_size - used;
}

void openai_show_query_since(
    const char *arguments)
{
    char report[32768];

    if (!openai_query_since_text(
            arguments,
            report,
            sizeof(report))) {
        (void)puts(
            "Usage: AGENT/REPAIR/HISTORY/SINCE "
            "<ISO timestamp prefix>"
        );
        return;
    }

    (void)fputs(report, stdout);
}


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


static int openai_is_repair_record_line(const char *line)
{
    if (line == NULL) {
        return 0;
    }

    return
        strstr(line, " workflow=AGENT/REPAIR ") != NULL &&
        strstr(line, " event=repair_attempt ") != NULL;
}

int openai_clear_repair_history(int approved)
{
    FILE *file;
    char *contents;
    long length;
    size_t count;
    char *position;
    char *line_start;
    unsigned int removed;

    if (!approved) {
        return 0;
    }

    file = fopen(openai_activity_path(), "rb");
    if (file == NULL) {
        return 1;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return 0;
    }

    length = ftell(file);
    if (length < 0L) {
        (void)fclose(file);
        return 0;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return 0;
    }

    contents = (char *)malloc((size_t)length + 1U);
    if (contents == NULL) {
        (void)fclose(file);
        return 0;
    }

    count = fread(contents, 1U, (size_t)length, file);
    if (ferror(file)) {
        free(contents);
        (void)fclose(file);
        return 0;
    }

    contents[count] = '\0';

    if (fclose(file) != 0) {
        free(contents);
        return 0;
    }

    file = fopen(openai_activity_path(), "w");
    if (file == NULL) {
        free(contents);
        return 0;
    }

    removed = 0U;
    line_start = contents;
    position = contents;

    while (*position != '\0') {
        if (*position == '\n') {
            char saved;

            saved = position[1];
            position[1] = '\0';

            if (openai_is_repair_record_line(line_start)) {
                ++removed;
            } else if (fputs(line_start, file) == EOF) {
                position[1] = saved;
                free(contents);
                (void)fclose(file);
                return 0;
            }

            position[1] = saved;
            line_start = position + 1;
        }

        ++position;
    }

    if (*line_start != '\0') {
        if (openai_is_repair_record_line(line_start)) {
            ++removed;
        } else if (fputs(line_start, file) == EOF) {
            free(contents);
            (void)fclose(file);
            return 0;
        }
    }

    free(contents);

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

void openai_clear_repair_cmd(void)
{
    char answer[64];
    size_t length;

    (void)puts(
        "This removes persisted AGENT/REPAIR attempt records "
        "from the active activity log."
    );
    (void)puts(
        "Other activity-log entries, source files, saved plans, "
        "and state are preserved."
    );
    (void)printf(
        "Type CLEAR REPAIR HISTORY to confirm: "
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return;
    }

    length = strlen(answer);
    while (length > 0U &&
           (answer[length - 1U] == '\n' ||
            answer[length - 1U] == '\r')) {
        answer[length - 1U] = '\0';
        --length;
    }

    if (strcmp(answer, "CLEAR REPAIR HISTORY") != 0) {
        (void)puts("Repair history clear cancelled.");
        return;
    }

    if (!openai_clear_repair_history(1)) {
        (void)printf(
            "Unable to clear repair history from %s: %s\n",
            openai_activity_path(),
            strerror(errno)
        );
        return;
    }

    (void)puts("Persisted repair history cleared.");
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

int openai_report_text(char *output,
                       size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    used = 0U;
    written = snprintf(
        output + used,
        output_size - used,
        "OVMS Agent repair history report\n"
        "--------------------------------\n"
        "History window: %u\n"
        "Runs available: %u\n",
        history_limit,
        run_count
    );

    if (written < 0 ||
        (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        unsigned int stored_index;

        stored_index =
            run_count - 1U - display_index;

        if (!openai_append_query_run(
                output,
                output_size,
                &used,
                display_index + 1U,
                &runs[stored_index])) {
            return 0;
        }
    }

    if (run_count == 0U) {
        written = snprintf(
            output + used,
            output_size - used,
            "\nNo persisted repair runs in the active window.\n"
        );

        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }
    }

    return 1;
}

void openai_show_report(void)
{
    char report[32768];

    if (!openai_report_text(
            report,
            sizeof(report))) {
        (void)puts(
            "Unable to build AGENT/REPAIR history report."
        );
        return;
    }

    (void)fputs(report, stdout);
}

int openai_summary_text(char *output,
                        size_t output_size)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int index;
    unsigned int committed;
    unsigned int rolled_back;
    unsigned int unsafe_count;
    unsigned int one_attempt;
    unsigned int two_attempt;
    unsigned int success_rate;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    committed = 0U;
    rolled_back = 0U;
    unsafe_count = 0U;
    one_attempt = 0U;
    two_attempt = 0U;

    for (index = 0U; index < run_count; ++index) {
        const openai_query_run *entry;
        const char *outcome;

        entry = &runs[index];

        if (entry->run.count == 1U) {
            ++one_attempt;
        } else if (entry->run.count == 2U) {
            ++two_attempt;
        }

        outcome = entry->run.attempts[
            entry->run.count - 1U
        ].outcome;

        if (strcmp(outcome, "committed") == 0) {
            ++committed;
        } else if (strcmp(
                       outcome,
                       "rolled_back") == 0) {
            ++rolled_back;
        } else if (strcmp(outcome, "unsafe") == 0) {
            ++unsafe_count;
        }
    }

    success_rate =
        run_count == 0U ?
        0U :
        (committed * 100U) / run_count;

    written = snprintf(
        output,
        output_size,
        "OVMS Agent repair history summary\n"
        "---------------------------------\n"
        "History window:   %u\n"
        "Runs analyzed:    %u\n"
        "Committed:        %u\n"
        "Rolled back:      %u\n"
        "Unsafe:           %u\n"
        "One attempt:      %u\n"
        "Two attempts:     %u\n"
        "Success rate:     %u%%\n",
        history_limit,
        run_count,
        committed,
        rolled_back,
        unsafe_count,
        one_attempt,
        two_attempt,
        success_rate
    );

    return
        written >= 0 &&
        (size_t)written < output_size;
}

void openai_show_summary(void)
{
    char summary[2048];

    if (!openai_summary_text(
            summary,
            sizeof(summary))) {
        (void)puts(
            "Unable to build AGENT/REPAIR history summary."
        );
        return;
    }

    (void)fputs(summary, stdout);
}

static int openai_export_probe(
    const char *path,
    int allow_overwrite)
{
    FILE *probe;

    if (!openai_repair_export_path_safe(path)) {
        return 0;
    }

    probe = fopen(path, "r");

    if (probe != NULL) {
        (void)fclose(probe);

        if (!allow_overwrite) {
            return 0;
        }
    }

    return 1;
}

int openai_csv_file(const char *path,
                    int allow_overwrite)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    FILE *file;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;

    if (!openai_export_probe(
            path,
            allow_overwrite)) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "run,started,attempt,plan,build_status,"
            "rollback,outcome,final_outcome\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;
        unsigned int attempt_index;
        const char *final_outcome;

        stored_index =
            run_count - 1U - display_index;
        entry = &runs[stored_index];
        final_outcome =
            entry->run.attempts[
                entry->run.count - 1U
            ].outcome;

        for (attempt_index = 0U;
             attempt_index < entry->run.count;
             ++attempt_index) {
            const openai_repair_attempt_record *record;

            record =
                &entry->run.attempts[attempt_index];

            if (fprintf(
                    file,
                    "%u,%s,%u,%08lX,%d,%d,%s,%s\n",
                    display_index + 1U,
                    entry->started[0] != '\0' ?
                        entry->started : "unknown",
                    record->attempt,
                    record->plan_hash,
                    record->build_status,
                    record->rollback,
                    record->outcome,
                    final_outcome) < 0) {
                (void)fclose(file);
                return 0;
            }
        }
    }

    return fclose(file) == 0;
}

int openai_kv_file(const char *path,
                   int allow_overwrite)
{
    openai_query_run runs[OPENAI_REPAIR_HISTORY_MAXIMUM];
    FILE *file;
    unsigned int history_limit;
    unsigned int run_count;
    unsigned int display_index;

    if (!openai_export_probe(
            path,
            allow_overwrite)) {
        return 0;
    }

    history_limit = openai_repair_history_limit();
    run_count = openai_collect_query_runs(
        runs,
        history_limit
    );

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fprintf(
            file,
            "history.window=%u\n"
            "history.runs=%u\n",
            history_limit,
            run_count) < 0) {
        (void)fclose(file);
        return 0;
    }

    for (display_index = 0U;
         display_index < run_count;
         ++display_index) {
        const openai_query_run *entry;
        unsigned int stored_index;
        unsigned int attempt_index;
        const char *final_outcome;

        stored_index =
            run_count - 1U - display_index;
        entry = &runs[stored_index];
        final_outcome =
            entry->run.attempts[
                entry->run.count - 1U
            ].outcome;

        if (fprintf(
                file,
                "run.%u.started=%s\n"
                "run.%u.attempts=%u\n"
                "run.%u.final_outcome=%s\n",
                display_index + 1U,
                entry->started[0] != '\0' ?
                    entry->started : "unknown",
                display_index + 1U,
                entry->run.count,
                display_index + 1U,
                final_outcome) < 0) {
            (void)fclose(file);
            return 0;
        }

        for (attempt_index = 0U;
             attempt_index < entry->run.count;
             ++attempt_index) {
            const openai_repair_attempt_record *record;

            record =
                &entry->run.attempts[attempt_index];

            if (fprintf(
                    file,
                    "run.%u.attempt.%u.plan=%08lX\n"
                    "run.%u.attempt.%u.build=%d\n"
                    "run.%u.attempt.%u.rollback=%d\n"
                    "run.%u.attempt.%u.outcome=%s\n",
                    display_index + 1U,
                    record->attempt,
                    record->plan_hash,
                    display_index + 1U,
                    record->attempt,
                    record->build_status,
                    display_index + 1U,
                    record->attempt,
                    record->rollback,
                    display_index + 1U,
                    record->attempt,
                    record->outcome) < 0) {
                (void)fclose(file);
                return 0;
            }
        }
    }

    return fclose(file) == 0;
}

static void openai_export_format(
    const char *arguments,
    const char *label,
    int csv_format)
{
    const char *path;
    FILE *probe;
    char answer[32];
    int exists;
    int success;

    path = openai_skip_spaces(arguments);

    if (!openai_repair_export_path_safe(path)) {
        (void)printf(
            "Usage: AGENT/REPAIR/HISTORY/%s <safe-filespec>\n",
            label
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
            "Replace existing %s export %s [y/N]? ",
            label,
            path
        );
        (void)fflush(stdout);

        if (fgets(answer, sizeof(answer), stdin) == NULL) {
            (void)putchar('\n');
            return;
        }

        if (answer[0] != 'y' &&
            answer[0] != 'Y') {
            (void)puts("Repair-history export cancelled.");
            return;
        }
    }

    success = csv_format ?
        openai_csv_file(path, exists ? 1 : 0) :
        openai_kv_file(path, exists ? 1 : 0);

    if (!success) {
        (void)printf(
            "Unable to export repair history to %s.\n",
            path
        );
        return;
    }

    (void)printf(
        "%s repair-history export written to %s.\n",
        label,
        path
    );
}

void openai_export_csv(const char *arguments)
{
    openai_export_format(
        arguments,
        "CSV",
        1
    );
}

void openai_export_kv(const char *arguments)
{
    openai_export_format(
        arguments,
        "KV",
        0
    );
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
