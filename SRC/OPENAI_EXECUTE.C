#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_execute.h"
#include "openai_plan.h"
#include "project.h"

#define OPENAI_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define OPENAI_EXECUTE_LINE_SIZE 4096U
#define OPENAI_EXECUTE_PATH_SIZE 256U
#define OPENAI_EXECUTE_TEXT_SIZE 4096U

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

static int execute_parse_operation(openai_saved_operation *operation)
{
    FILE *file;
    char line[OPENAI_EXECUTE_LINE_SIZE];
    int in_operation;
    int saw_begin;
    int saw_end;
    int saw_type;
    int saw_path;
    int saw_old;
    int saw_new;
    int operation_count;

    if (operation == NULL) {
        return 0;
    }

    operation->path[0] = '\0';
    operation->old_text[0] = '\0';
    operation->new_text[0] = '\0';

    file = fopen(OPENAI_PLAN_FILE, "r");

    if (file == NULL) {
        (void)puts("No saved implementation plan is available.");
        return 0;
    }

    in_operation = 0;
    saw_begin = 0;
    saw_end = 0;
    saw_type = 0;
    saw_path = 0;
    saw_old = 0;
    saw_new = 0;
    operation_count = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strcmp(line, "BEGIN_OPERATION\n") == 0 ||
            strcmp(line, "BEGIN_OPERATION\r\n") == 0) {
            ++operation_count;

            if (operation_count > 1) {
                (void)puts(
                    "Saved plan contains more than one executable operation."
                );
                (void)fclose(file);
                return 0;
            }

            in_operation = 1;
            saw_begin = 1;
            continue;
        }

        if (!in_operation) {
            continue;
        }

        if (strcmp(line, "END_OPERATION\n") == 0 ||
            strcmp(line, "END_OPERATION\r\n") == 0) {
            saw_end = 1;
            in_operation = 0;
            continue;
        }

        if (strncmp(line, "type=", 5U) == 0) {
            char type[64];

            if (!execute_copy_value(type,
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
            if (!execute_copy_value(operation->path,
                                    sizeof(operation->path),
                                    line + 5)) {
                (void)puts("Saved operation path is invalid or too long.");
                (void)fclose(file);
                return 0;
            }

            saw_path = 1;
            continue;
        }

        if (strncmp(line, "old_text=", 9U) == 0) {
            if (!execute_copy_value(operation->old_text,
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
            if (!execute_copy_value(operation->new_text,
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

    if (!saw_begin ||
        !saw_end ||
        !saw_type ||
        !saw_path ||
        !saw_old ||
        !saw_new ||
        in_operation) {
        (void)puts(
            "Saved plan does not contain one complete executable operation."
        );
        return 0;
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
    openai_saved_operation operation;
    char *build_output;
    int build_status;
    int patched;

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
        openai_log_event("AGENT/EXECUTE", "plan_invalid", 0);
        return;
    }

    if (!execute_parse_operation(&operation)) {
        (void)puts(
            "Saved plan execution refused because no deterministic "
            "operation is available."
        );
        openai_log_event("AGENT/EXECUTE", "operation_invalid", 0);
        return;
    }

    if (!openai_path_is_safe(operation.path) ||
        openai_path_is_sensitive(operation.path)) {
        (void)printf(
            "Saved plan execution refused for path: %s\n",
            operation.path
        );
        openai_log_event("AGENT/EXECUTE", "path_refused", 0);
        return;
    }

    (void)puts("Executing one saved, validated operation.");
    patched = project_patch(
        state,
        operation.path,
        operation.old_text,
        operation.new_text
    );

    if (!patched) {
        (void)puts(
            "Saved operation was declined or could not be applied. "
            "Build not run."
        );
        openai_log_event("AGENT/EXECUTE", "patch_not_applied", 0);
        return;
    }

    openai_log_event("AGENT/EXECUTE", "patch_applied", 1);
    (void)puts("Saved operation applied. Running controlled build...");

    build_output = execute_run_build_tool(&build_status);

    (void)printf(
        "Tool executed: run_build [%s, status %d]\n",
        (build_status & 1) != 0 ? "success" : "failure",
        build_status
    );

    if ((build_status & 1) == 0) {
        openai_log_event("AGENT/EXECUTE", "build_failure", build_status);

        if (openai_confirm_restore(operation.path)) {
            if (openai_restore_previous_version(operation.path)) {
                (void)puts(
                    "Rollback complete. The prior contents are now "
                    "the latest file version."
                );
                openai_log_event(
                    "AGENT/EXECUTE",
                    "rollback_succeeded",
                    1
                );
            } else {
                (void)puts("Rollback was requested but failed.");
                openai_log_event(
                    "AGENT/EXECUTE",
                    "rollback_failed",
                    0
                );
            }
        } else {
            (void)puts(
                "Rollback declined. The patched version remains current."
            );
            openai_log_event(
                "AGENT/EXECUTE",
                "rollback_declined",
                0
            );
        }

        if (build_output != NULL) {
            (void)puts("");
            (void)puts(build_output);
        }

        free(build_output);
        return;
    }

    openai_log_event("AGENT/EXECUTE", "build_success", build_status);

    if (build_output != NULL) {
        (void)puts("");
        (void)puts(build_output);
    }

    free(build_output);

    if (execute_mark_consumed()) {
        (void)puts("Saved plan marked consumed.");
    } else {
        (void)puts(
            "Warning: build succeeded, but the saved plan could not "
            "be marked consumed."
        );
    }

    (void)puts("Saved plan execution complete.");
}
