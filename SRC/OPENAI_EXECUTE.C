#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "edit_txn.h"
#include "openai_execute.h"
#include "openai_plan.h"
#include "project.h"

#define OPENAI_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define OPENAI_EXECUTE_LINE_SIZE 4096U
#define OPENAI_EXECUTE_PATH_SIZE 256U
#define OPENAI_EXECUTE_TEXT_SIZE 4096U
#define OPENAI_EXECUTE_MAX_OPERATIONS 32U

typedef struct openai_saved_operation {
    char path[OPENAI_EXECUTE_PATH_SIZE];
    char old_text[OPENAI_EXECUTE_TEXT_SIZE];
    char new_text[OPENAI_EXECUTE_TEXT_SIZE];
} openai_saved_operation;

static int execute_copy_value(char *destination,
                              size_t destination_size,
                              const char *source)
{
    size_t length;

    if (destination == NULL ||
        destination_size == 0U ||
        source == NULL) {
        return 0;
    }

    length = strlen(source);

    while (length > 0U &&
           (source[length - 1U] == '\n' ||
            source[length - 1U] == '\r')) {
        --length;
    }

    if (length == 0U || length >= destination_size) {
        return 0;
    }

    (void)memcpy(destination, source, length);
    destination[length] = '\0';
    return 1;
}

static int execute_parse_operations(
    openai_saved_operation *operations,
    unsigned int operation_capacity,
    unsigned int *operation_count_out)
{
    FILE *file;
    char line[OPENAI_EXECUTE_LINE_SIZE];
    openai_saved_operation *operation;
    unsigned int operation_count;
    unsigned int declared_operation_count;
    int saw_declared_count;
    int in_operation;
    int saw_type;
    int saw_path;
    int saw_old;
    int saw_new;

    if (operations == NULL ||
        operation_capacity == 0U ||
        operation_count_out == NULL) {
        return 0;
    }

    file = fopen(OPENAI_PLAN_FILE, "r");

    if (file == NULL) {
        (void)puts("No saved implementation plan is available.");
        return 0;
    }

    operation = NULL;
    operation_count = 0U;
    declared_operation_count = 0U;
    saw_declared_count = 0;
    in_operation = 0;
    saw_type = 0;
    saw_path = 0;
    saw_old = 0;
    saw_new = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "operation_count=", 16U) == 0) {
            char *end_pointer;
            unsigned long value;

            if (saw_declared_count || in_operation) {
                (void)puts(
                    "Saved plan contains a misplaced or duplicate operation_count."
                );
                (void)fclose(file);
                return 0;
            }

            end_pointer = NULL;
            value = strtoul(line + 16, &end_pointer, 10);

            if (end_pointer == line + 16 ||
                value == 0UL ||
                value > operation_capacity ||
                (*end_pointer != '\n' &&
                 *end_pointer != '\r' &&
                 *end_pointer != '\0')) {
                (void)puts(
                    "Saved plan operation_count is invalid."
                );
                (void)fclose(file);
                return 0;
            }

            declared_operation_count = (unsigned int)value;
            saw_declared_count = 1;
            continue;
        }

        if (strcmp(line, "BEGIN_OPERATION\n") == 0 ||
            strcmp(line, "BEGIN_OPERATION\r\n") == 0) {
            if (in_operation ||
                operation_count >= operation_capacity) {
                (void)puts(
                    "Saved plan contains too many or nested operations."
                );
                (void)fclose(file);
                return 0;
            }

            operation = &operations[operation_count];
            (void)memset(operation, 0, sizeof(*operation));
            in_operation = 1;
            saw_type = 0;
            saw_path = 0;
            saw_old = 0;
            saw_new = 0;
            continue;
        }

        if (!in_operation) {
            continue;
        }

        if (strcmp(line, "END_OPERATION\n") == 0 ||
            strcmp(line, "END_OPERATION\r\n") == 0) {
            if (!saw_type ||
                !saw_path ||
                !saw_old ||
                !saw_new) {
                (void)puts(
                    "Saved plan contains an incomplete operation."
                );
                (void)fclose(file);
                return 0;
            }

            ++operation_count;
            operation = NULL;
            in_operation = 0;
            continue;
        }

        if (strncmp(line, "type=", 5U) == 0) {
            char type[64];

            if (!execute_copy_value(
                    type,
                    sizeof(type),
                    line + 5) ||
                strcmp(type, "replace_text") != 0) {
                (void)puts(
                    "Saved operation type is unsupported; "
                    "only replace_text is allowed."
                );
                (void)fclose(file);
                return 0;
            }

            saw_type = 1;
            continue;
        }

        if (strncmp(line, "path=", 5U) == 0) {
            if (!execute_copy_value(
                    operation->path,
                    sizeof(operation->path),
                    line + 5)) {
                (void)puts(
                    "Saved operation path is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_path = 1;
            continue;
        }

        if (strncmp(line, "old_text=", 9U) == 0) {
            if (!execute_copy_value(
                    operation->old_text,
                    sizeof(operation->old_text),
                    line + 9)) {
                (void)puts(
                    "Saved operation old_text is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_old = 1;
            continue;
        }

        if (strncmp(line, "new_text=", 9U) == 0) {
            if (!execute_copy_value(
                    operation->new_text,
                    sizeof(operation->new_text),
                    line + 9)) {
                (void)puts(
                    "Saved operation new_text is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_new = 1;
            continue;
        }

        (void)puts(
            "Saved operation contains an unsupported or malformed line."
        );
        (void)fclose(file);
        return 0;
    }

    (void)fclose(file);

    if (in_operation ||
        operation_count == 0U) {
        (void)puts(
            "Saved plan does not contain complete executable operations."
        );
        return 0;
    }

    if (!saw_declared_count) {
        (void)puts(
            "Saved plan is missing operation_count; regenerate it with AGENT/PLAN."
        );
        return 0;
    }

    if (declared_operation_count != operation_count) {
        (void)printf(
            "Saved plan operation mismatch: declared %u, parsed %u.\n",
            declared_operation_count,
            operation_count
        );
        (void)puts(
            "Execution refused because one or more planned edits lack operation blocks."
        );
        return 0;
    }

    *operation_count_out = operation_count;
    return 1;
}


static char *execute_build_replacement(
    const openai_saved_operation *operation)
{
    char *content;
    char *match;
    char *second_match;
    char *replacement;
    size_t prefix_length;
    size_t old_length;
    size_t new_length;
    size_t content_length;
    size_t result_length;

    content = openai_read_text_file(operation->path);

    if (content == NULL ||
        operation->old_text[0] == '\0') {
        free(content);
        return NULL;
    }

    match = strstr(content, operation->old_text);

    if (match == NULL) {
        (void)printf(
            "Saved operation old_text was not found in %s.\n",
            operation->path
        );
        free(content);
        return NULL;
    }

    second_match = strstr(
        match + strlen(operation->old_text),
        operation->old_text
    );

    if (second_match != NULL) {
        (void)printf(
            "Saved operation old_text is not unique in %s.\n",
            operation->path
        );
        free(content);
        return NULL;
    }

    prefix_length = (size_t)(match - content);
    old_length = strlen(operation->old_text);
    new_length = strlen(operation->new_text);
    content_length = strlen(content);
    result_length =
        prefix_length +
        new_length +
        content_length -
        prefix_length -
        old_length;

    replacement = malloc(result_length + 1U);

    if (replacement == NULL) {
        free(content);
        return NULL;
    }

    if (prefix_length > 0U) {
        (void)memcpy(
            replacement,
            content,
            prefix_length
        );
    }

    if (new_length > 0U) {
        (void)memcpy(
            replacement + prefix_length,
            operation->new_text,
            new_length
        );
    }

    (void)memcpy(
        replacement + prefix_length + new_length,
        match + old_length,
        content_length -
            prefix_length -
            old_length
    );
    replacement[result_length] = '\0';

    free(content);
    return replacement;
}

static int execute_stage_operations(
    edit_txn *transaction,
    const openai_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;

    for (index = 0U;
         index < operation_count;
         ++index) {
        char *replacement;

        if (!openai_path_is_safe(
                operations[index].path) ||
            openai_path_is_sensitive(
                operations[index].path)) {
            (void)printf(
                "Saved plan execution refused for path: %s\n",
                operations[index].path
            );
            return 0;
        }

        replacement =
            execute_build_replacement(
                &operations[index]
            );

        if (replacement == NULL) {
            return 0;
        }

        if (!edit_txn_add(
                transaction,
                operations[index].path,
                replacement)) {
            (void)printf(
                "Unable to stage saved operation for %s. "
                "The path may be duplicated, unreadable, unsafe, or impossible to snapshot.\n",
                operations[index].path
            );
            free(replacement);
            return 0;
        }

        free(replacement);
    }

    return 1;
}

static int execute_mark_consumed(void)
{
    FILE *input;
    FILE *output;
    char line[OPENAI_EXECUTE_LINE_SIZE];
    int replaced;
    int success;

    input = fopen(OPENAI_PLAN_FILE, "r");

    if (input == NULL) {
        return 0;
    }

    output = fopen("OVMS_AGENT_PLAN_NEW.TXT", "w");

    if (output == NULL) {
        (void)fclose(input);
        return 0;
    }

    replaced = 0;
    success = 1;

    while (fgets(line, sizeof(line), input) != NULL) {
        if (!replaced && strncmp(line, "status=", 7U) == 0) {
            if (fputs("status=consumed\n", output) == EOF) {
                success = 0;
                break;
            }

            replaced = 1;
        } else if (fputs(line, output) == EOF) {
            success = 0;
            break;
        }
    }

    if (fclose(input) != 0) {
        success = 0;
    }

    if (fclose(output) != 0) {
        success = 0;
    }

    if (!success || !replaced) {
        (void)remove("OVMS_AGENT_PLAN_NEW.TXT");
        return 0;
    }

    if (remove(OPENAI_PLAN_FILE) != 0) {
        (void)remove("OVMS_AGENT_PLAN_NEW.TXT");
        return 0;
    }

    if (rename("OVMS_AGENT_PLAN_NEW.TXT",
               OPENAI_PLAN_FILE) != 0) {
        return 0;
    }

    return 1;
}

void openai_plan_execute(agent_state *state)
{
    openai_saved_operation *operations;
    edit_txn *transaction;
    char *build_output;
    unsigned int operation_count;
    unsigned int index;
    int build_status;
    int write_ok;
    int rollback_ok;

    openai_log_event("AGENT/EXECUTE", "start", 0);

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!openai_plan_is_current(1)) {
        (void)puts(
            "Saved plan execution refused. Run AGENT/PLAN again."
        );
        openai_log_event(
            "AGENT/EXECUTE",
            "plan_invalid",
            0
        );
        return;
    }

    operations = (openai_saved_operation *)calloc(
        OPENAI_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations)
    );

    transaction = (edit_txn *)malloc(
        sizeof(*transaction)
    );

    if (operations == NULL ||
        transaction == NULL) {
        (void)puts(
            "Insufficient memory for plan-wide transaction."
        );
        free(operations);
        free(transaction);
        openai_log_event(
            "AGENT/EXECUTE",
            "allocation_failed",
            0
        );
        return;
    }

    operation_count = 0U;

    if (!execute_parse_operations(
            operations,
            OPENAI_EXECUTE_MAX_OPERATIONS,
            &operation_count)) {
        (void)puts(
            "Saved plan execution refused because no deterministic "
            "operations are available."
        );
        free(operations);
        free(transaction);
        openai_log_event(
            "AGENT/EXECUTE",
            "operation_invalid",
            0
        );
        return;
    }

    (void)puts("Parsed saved operations:");

    for (index = 0U;
         index < operation_count;
         ++index) {
        (void)printf(
            "  %u: %s\n",
            index + 1U,
            operations[index].path
        );
    }

    edit_txn_init(transaction);

    if (!execute_stage_operations(
            transaction,
            operations,
            operation_count)) {
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event(
            "AGENT/EXECUTE",
            "stage_failed",
            0
        );
        return;
    }

    (void)printf(
        "Executing %u saved operations as one transaction.\n",
        operation_count
    );

    write_ok = edit_txn_write(transaction);

    if (!write_ok) {
        (void)puts(
            "Plan-wide write failed. Transaction rollback was attempted."
        );
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event(
            "AGENT/EXECUTE",
            "write_failed",
            0
        );
        return;
    }

    openai_log_event(
        "AGENT/EXECUTE",
        "transaction_written",
        (int)operation_count
    );

    (void)puts(
        "All saved operations written. Running controlled build..."
    );

    build_output =
        execute_run_build_tool(&build_status);

    (void)printf(
        "Tool executed: run_build [%s, status %d]\n",
        (build_status & 1) != 0 ?
            "success" : "failure",
        build_status
    );

    if ((build_status & 1) == 0) {
        openai_log_event(
            "AGENT/EXECUTE",
            "build_failure",
            build_status
        );

        rollback_ok =
            edit_txn_rollback(transaction);

        (void)printf(
            "Plan-wide rollback: %s\n",
            rollback_ok ? "PASS" : "FAIL"
        );

        openai_log_event(
            "AGENT/EXECUTE",
            rollback_ok ?
                "rollback_succeeded" :
                "rollback_failed",
            rollback_ok
        );

        if (build_output != NULL) {
            (void)puts("");
            (void)puts(build_output);
        }

        free(build_output);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        return;
    }

    if (!edit_txn_commit(transaction)) {
        (void)puts(
            "Build passed, but transaction commit failed."
        );
        (void)edit_txn_rollback(transaction);
        free(build_output);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event(
            "AGENT/EXECUTE",
            "commit_failed",
            0
        );
        return;
    }

    openai_log_event(
        "AGENT/EXECUTE",
        "build_success",
        build_status
    );

    if (build_output != NULL) {
        (void)puts("");
        (void)puts(build_output);
    }

    free(build_output);
    edit_txn_dispose(transaction);
    free(transaction);
    free(operations);

    if (execute_mark_consumed()) {
        (void)puts("Saved plan marked consumed.");
        openai_log_event(
            "AGENT/EXECUTE",
            "plan_consumed",
            1
        );
    } else {
        (void)puts(
            "Warning: changes committed, but plan could not be marked consumed."
        );
        openai_log_event(
            "AGENT/EXECUTE",
            "consume_failed",
            0
        );
    }

    (void)puts(
        "Plan-wide transaction completed successfully."
    );
}
