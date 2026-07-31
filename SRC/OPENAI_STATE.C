#include "openai_internal.h"
#include "openai_state_m109_path_helpers.inc"

static void openai_state_note_save_result(int save_succeeded);

void openai_save_state(void)
{
    int save_succeeded;
    int prior_valid;

    save_succeeded = openai_state_save_to(
        OPENAI_STATE_FILE,
        openai_last_workflow,
        openai_last_build_known,
        openai_last_build_status,
        openai_last_rollback);

    prior_valid = openai_state_valid;
    openai_state_note_save_result(save_succeeded);
    openai_state_save_known = 1;
    openai_state_save_succeeded = save_succeeded ? 1 : 0;
    if (save_succeeded) {
        openai_state_recovered = 0;
        openai_state_snapshot_current();
    } else {
        openai_state_valid = prior_valid;
    }
}

#include "openai_state_m107_m109_validate.inc"
#include "openai_state_m111_validate.inc"
#include "openai_state_m112_validate.inc"
#include "openai_state_m113_validate.inc"
#include "openai_state_m114_validate.inc"
#include "openai_state_m115_validate.inc"
#include "openai_state_m116_validate.inc"
#include "openai_state_m117_validate.inc"
#include "openai_state_m118_validate.inc"
#include "openai_state_m119_validate.inc"
#include "openai_state_m120_validate.inc"
#include "openai_state_m121_validate.inc"
#include "openai_state_m122_validate.inc"
#include "openai_state_m123_validate.inc"
#include "openai_state_m124_validate.inc"
#include "openai_state_m125_validate.inc"
#include "openai_state_m126_validate.inc"
#include "openai_state_m127_validate.inc"
#include "openai_state_m128_validate.inc"
#include "openai_state_m129_validate.inc"
#include "openai_state_m130_validate.inc"
#include "openai_state_m131_validate.inc"
#include "openai_state_m132_validate.inc"
#include "openai_state_m133_validate.inc"
#include "openai_state_m134_validate.inc"

void openai_state_save(void)
{
    openai_save_state();
}

void openai_load_state(void)
{
    if (openai_state_loaded) {
        return;
    }

    (void)openai_state_apply_load(OPENAI_STATE_FILE);
}

void openai_show_state(void)
{
    openai_load_state();

    (void)puts("OVMS Agent persisted state");
    (void)puts("--------------------------");
    (void)printf(
        "State file:               %s\n",
        OPENAI_STATE_FILE
    );

    if (!openai_state_valid) {
        (void)puts(
            "State status:             unavailable or invalid"
        );
        return;
    }

    (void)printf(
        "State source:             %s\n",
        openai_state_source_name(
            openai_state_valid,
            openai_state_recovered)
    );
    (void)puts("State format:             1");
    (void)printf(
        "Last workflow:            %s\n",
        openai_workflow_name(openai_saved_workflow)
    );

    if (openai_saved_build_known) {
        (void)printf(
            "Last build:               %s (status %d)\n",
            (openai_saved_build_status & 1) != 0 ?
                "success" : "failure",
            openai_saved_build_status
        );
    } else {
        (void)puts(
            "Last build:               not recorded"
        );
    }

    (void)printf(
        "Last rollback:            %s\n",
        openai_rollback_name(openai_saved_rollback)
    );

    (void)puts(
        "Chat state:               not persisted"
    );
    (void)puts(
        "Secrets/prompts/content:  not stored"
    );
}

void openai_clear_state(void)
{
    char answer[32];

    (void)printf(
        "Clear persisted OVMS Agent workflow state [y/N]? "
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return;
    }

    if (answer[0] != 'y' && answer[0] != 'Y') {
        (void)puts("Persistent state clear cancelled.");
        return;
    }

    if (!openai_state_purge_all(OPENAI_STATE_FILE)) {
        (void)printf(
            "Unable to remove %s: %s\n",
            OPENAI_STATE_FILE,
            strerror(errno)
        );
        return;
    }

    openai_state_reset_memory();
    openai_state_loaded = 1;

    (void)puts("Persistent workflow state cleared.");
}

void openai_show_memory(void)
{
    FILE *file;
    char line[4096];
    char *latest_build_success[5];
    char *latest_build_failure[5];
    char *latest_rollback_succeeded[5];
    char *latest_plan_consumed[5];
    char *latest_transaction_written[5];
    int cb_build_success = 0;
    int cb_build_failure = 0;
    int cb_rollback_succeeded = 0;
    int cb_plan_consumed = 0;
    int cb_transaction_written = 0;
    int i;

    for (i = 0; i < 5; ++i) {
        latest_build_success[i] = NULL;
        latest_build_failure[i] = NULL;
        latest_rollback_succeeded[i] = NULL;
        latest_plan_consumed[i] = NULL;
        latest_transaction_written[i] = NULL;
    }

    (void)puts("OVMS Agent activity memory");
    (void)puts("-------------------------");
    (void)printf("Activity file:            %s\n", "OVMS_AGENT_ACTIVITY.LOG");

    file = fopen("OVMS_AGENT_ACTIVITY.LOG", "r");

    if (file == NULL) {
        (void)printf("Activity log unavailable: %s\n", strerror(errno));
    } else {
        while (fgets(line, sizeof(line), file) != NULL) {
            /* maintain arrays with newest at index 0 */
            if (strstr(line, "build_success") != NULL) {
                /* shift right */
                for (i = 4; i > 0; --i) {
                    if (latest_build_success[i-1] != NULL) {
                        if (latest_build_success[i] != NULL) free(latest_build_success[i]);
                        latest_build_success[i] = strdup(latest_build_success[i-1]);
                    } else {
                        if (latest_build_success[i] != NULL) { free(latest_build_success[i]); latest_build_success[i] = NULL; }
                    }
                }
                if (latest_build_success[0] != NULL) free(latest_build_success[0]);
                latest_build_success[0] = malloc(strlen(line) + 1);
                if (latest_build_success[0] != NULL) strcpy(latest_build_success[0], line);
                if (cb_build_success < 5) cb_build_success++;
            }

            if (strstr(line, "build_failure") != NULL) {
                for (i = 4; i > 0; --i) {
                    if (latest_build_failure[i-1] != NULL) {
                        if (latest_build_failure[i] != NULL) free(latest_build_failure[i]);
                        latest_build_failure[i] = strdup(latest_build_failure[i-1]);
                    } else {
                        if (latest_build_failure[i] != NULL) { free(latest_build_failure[i]); latest_build_failure[i] = NULL; }
                    }
                }
                if (latest_build_failure[0] != NULL) free(latest_build_failure[0]);
                latest_build_failure[0] = malloc(strlen(line) + 1);
                if (latest_build_failure[0] != NULL) strcpy(latest_build_failure[0], line);
                if (cb_build_failure < 5) cb_build_failure++;
            }

            if (strstr(line, "rollback_succeeded") != NULL) {
                for (i = 4; i > 0; --i) {
                    if (latest_rollback_succeeded[i-1] != NULL) {
                        if (latest_rollback_succeeded[i] != NULL) free(latest_rollback_succeeded[i]);
                        latest_rollback_succeeded[i] = strdup(latest_rollback_succeeded[i-1]);
                    } else {
                        if (latest_rollback_succeeded[i] != NULL) { free(latest_rollback_succeeded[i]); latest_rollback_succeeded[i] = NULL; }
                    }
                }
                if (latest_rollback_succeeded[0] != NULL) free(latest_rollback_succeeded[0]);
                latest_rollback_succeeded[0] = malloc(strlen(line) + 1);
                if (latest_rollback_succeeded[0] != NULL) strcpy(latest_rollback_succeeded[0], line);
                if (cb_rollback_succeeded < 5) cb_rollback_succeeded++;
            }

            if (strstr(line, "plan_consumed") != NULL) {
                for (i = 4; i > 0; --i) {
                    if (latest_plan_consumed[i-1] != NULL) {
                        if (latest_plan_consumed[i] != NULL) free(latest_plan_consumed[i]);
                        latest_plan_consumed[i] = strdup(latest_plan_consumed[i-1]);
                    } else {
                        if (latest_plan_consumed[i] != NULL) { free(latest_plan_consumed[i]); latest_plan_consumed[i] = NULL; }
                    }
                }
                if (latest_plan_consumed[0] != NULL) free(latest_plan_consumed[0]);
                latest_plan_consumed[0] = malloc(strlen(line) + 1);
                if (latest_plan_consumed[0] != NULL) strcpy(latest_plan_consumed[0], line);
                if (cb_plan_consumed < 5) cb_plan_consumed++;
            }

            if (strstr(line, "transaction_written") != NULL) {
                for (i = 4; i > 0; --i) {
                    if (latest_transaction_written[i-1] != NULL) {
                        if (latest_transaction_written[i] != NULL) free(latest_transaction_written[i]);
                        latest_transaction_written[i] = strdup(latest_transaction_written[i-1]);
                    } else {
                        if (latest_transaction_written[i] != NULL) { free(latest_transaction_written[i]); latest_transaction_written[i] = NULL; }
                    }
                }
                if (latest_transaction_written[0] != NULL) free(latest_transaction_written[0]);
                latest_transaction_written[0] = malloc(strlen(line) + 1);
                if (latest_transaction_written[0] != NULL) strcpy(latest_transaction_written[0], line);
                if (cb_transaction_written < 5) cb_transaction_written++;
            }
        }

        (void)fclose(file);

        (void)puts("\nMost recent build_success events:");
        if (cb_build_success == 0) (void)puts("  (none)");
        for (i = 0; i < cb_build_success; ++i) {
            (void)printf("  %s", latest_build_success[i]);
        }

        (void)puts("\nMost recent build_failure events:");
        if (cb_build_failure == 0) (void)puts("  (none)");
        for (i = 0; i < cb_build_failure; ++i) {
            (void)printf("  %s", latest_build_failure[i]);
        }

        (void)puts("\nMost recent rollback_succeeded events:");
        if (cb_rollback_succeeded == 0) (void)puts("  (none)");
        for (i = 0; i < cb_rollback_succeeded; ++i) {
            (void)printf("  %s", latest_rollback_succeeded[i]);
        }

        (void)puts("\nMost recent plan_consumed events:");
        if (cb_plan_consumed == 0) (void)puts("  (none)");
        for (i = 0; i < cb_plan_consumed; ++i) {
            (void)printf("  %s", latest_plan_consumed[i]);
        }

        (void)puts("\nMost recent transaction_written events:");
        if (cb_transaction_written == 0) (void)puts("  (none)");
        for (i = 0; i < cb_transaction_written; ++i) {
            (void)printf("  %s", latest_transaction_written[i]);
        }

        /* free allocated memory */
        for (i = 0; i < 5; ++i) {
            if (latest_build_success[i] != NULL) free(latest_build_success[i]);
            if (latest_build_failure[i] != NULL) free(latest_build_failure[i]);
            if (latest_rollback_succeeded[i] != NULL) free(latest_rollback_succeeded[i]);
            if (latest_plan_consumed[i] != NULL) free(latest_plan_consumed[i]);
            if (latest_transaction_written[i] != NULL) free(latest_transaction_written[i]);
        }
    }

    /* Now show persisted state file if available */
    file = fopen(OPENAI_STATE_FILE, "r");
    if (file == NULL) {
        (void)printf("State file unavailable: %s\n", OPENAI_STATE_FILE);
        (void)puts("OVMS_AGENT.STATE not found or unreadable; state display skipped.");
    } else {
        (void)fclose(file);
        /* reuse existing display routine */
        openai_show_state();
    }
}

int openai_state_load_path(const char *path)
{
    return openai_state_apply_load(path);
}
