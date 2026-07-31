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
    int is_block;
} openai_saved_operation;

#include "openai_execute_read_text_block.inc"
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
    const char *path,
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

    file = fopen(path, "r");

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
        #include "openai_execute_count_handler.inc"

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

        #include "openai_execute_toplevel_guard.inc"

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

        #include "openai_execute_after_new_guard.inc"
        #include "openai_execute_type_handler.inc"
        #include "openai_execute_path_precheck.inc"

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

        #include "openai_execute_block_misuse_guard.inc"
        #include "openai_execute_block_handlers.inc"
        #include "openai_execute_legacy_misuse_guard.inc"
        #include "openai_execute_legacy_prechecks.inc"

        if (!operation->is_block &&
            strncmp(line, "old_text=", 9U) == 0) {
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

        if (!operation->is_block &&
            strncmp(line, "new_text=", 9U) == 0) {
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

        #include "openai_execute_fallback_m100.inc"
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

#include "openai_execute_validate.inc"

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

#include "openai_execute_chained_stage.inc"
#include "openai_execute_stage_validate.inc"
#include "openai_execute_dry_validate.inc"
#include "openai_execute_stage_expect_failure.inc"
#include "openai_execute_rollback_validate.inc"
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
        (void)printf("Warning: could not rename OVMS_AGENT_PLAN_NEW.TXT to %s: %s\n", OPENAI_PLAN_FILE, strerror(errno));
        return 0;
    }
    return 1;
}

#include "openai_execute_write_text_block.inc"
#include "openai_execute_m108_writers.inc"
static int execute_save_ops_to(
    const char *path,
    const openai_saved_operation *operations,
    unsigned int operation_count)
{
    FILE *f;
    unsigned int i;
    int success;

    if (path == NULL ||
        *path == '\0' ||
        operations == NULL) {
        return 0;
    }

    f = fopen(path, "w");

    if (f == NULL) {
        return 0;
    }

    success = 1;

    if (fprintf(f, "operation_count=%u\n",
                operation_count) < 0) {
        success = 0;
    }

    for (i = 0U; i < operation_count && success; ++i) {
        if (fprintf(f, "BEGIN_OPERATION\n") < 0) {
            success = 0;
            break;
        }

        if (fprintf(f, "type=replace_block\n") < 0) {
            success = 0;
            break;
        }

        if (fprintf(f, "path=%s\n",
                    operations[i].path) < 0) {
            success = 0;
            break;
        }

        if (!execute_write_text_block(
                f,
                "BEGIN_OLD_TEXT",
                "END_OLD_TEXT",
                operations[i].old_text)) {
            success = 0;
            break;
        }

        if (!execute_write_text_block(
                f,
                "BEGIN_NEW_TEXT",
                "END_NEW_TEXT",
                operations[i].new_text)) {
            success = 0;
            break;
        }

        if (fprintf(f, "END_OPERATION\n") < 0) {
            success = 0;
            break;
        }
    }

    if (fclose(f) != 0) {
        success = 0;
    }

    return success;
}

#include "openai_execute_m107_validate.inc"
#include "openai_exec_m135_validate.inc"
#include "openai_execute_dry_run.inc"
#include "openai_exec_m136_consume.inc"
#include "openai_exec_m136_validate.inc"
#include "openai_exec_m137_recovery.inc"
#include "openai_exec_m137_validate.inc"
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

    if (!execute_plan_recover()) {
        openai_plan_recovery_sync(OPENAI_PLAN_FILE);
        (void)puts("Unable to recover plan-consumption artifacts.");
        openai_log_event(
            "AGENT/EXECUTE",
            "plan_recovery_failed",
            0
        );
        return;
    }

    openai_plan_recovery_sync(OPENAI_PLAN_FILE);

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
            OPENAI_PLAN_FILE,
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

    if (!execute_stage_operations_chained(
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

    if (!openai_plan_approval_valid(OPENAI_PLAN_FILE)) {
        if (openai_approval_invalidated) {
            (void)puts(
                "Saved plan approval was invalidated; run AGENT/APPROVE again.");
        } else {
            (void)puts(
                "Saved plan execution requires AGENT/APPROVE.");
        }
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event(
            "AGENT/EXECUTE",
            "approval_required",
            0
        );
        return;
    }

    /* Approval consumed before first project write. */
    openai_plan_approval_consume();

    (void)printf(
        "Executing %u saved operations as one transaction.\n",
        operation_count
    );

    write_ok = edit_txn_write(transaction);

    if (!write_ok) {
        rollback_ok = edit_txn_rollback(transaction);
        (void)printf(
            "Plan-wide write failed. Transaction rollback: %s\n",
            rollback_ok ? "PASS" : "FAIL"
        );
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event(
            "AGENT/EXECUTE",
            "write_failed",
            rollback_ok
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

        if (!execute_save_build_to(
                "OVMS_AGENT_FAILED_BUILD.TXT",
                build_output)) {
            (void)puts("Warning: unable to save failed build output.");
        }

        if (!execute_save_ops_to(
                "OVMS_AGENT_FAILED_OPERATIONS.TXT",
                operations,
                operation_count)) {
            (void)puts("Warning: unable to save failed operation evidence.");
        }

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

        openai_last_build_known = 1;
        openai_last_build_status = build_status;
        openai_last_rollback = rollback_ok ? OPENAI_ROLLBACK_SUCCEEDED : OPENAI_ROLLBACK_FAILED;
        openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
        openai_state_save();

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

    openai_last_build_known = 1;
    openai_last_build_status = build_status;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;
    openai_state_save();

    if (build_output != NULL) {
        (void)puts("");
        (void)puts(build_output);
    }

    free(build_output);
    edit_txn_dispose(transaction);
    free(transaction);
    free(operations);

    openai_plan_approval_clear();

    if (execute_mark_consumed_safe()) {
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
