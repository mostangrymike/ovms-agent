#include "openai_internal.h"

void openai_save_state(void)
{
    FILE *file;

    file = fopen(OPENAI_STATE_FILE, "w");

    if (file == NULL) {
        return;
    }

    (void)fprintf(
        file,
        "format=1\n"
        "last_workflow=%d\n"
        "last_build_known=%d\n"
        "last_build_status=%d\n"
        "last_rollback=%d\n",
        openai_last_workflow,
        openai_last_build_known,
        openai_last_build_status,
        openai_last_rollback
    );

    if (fclose(file) == 0) {
        openai_state_loaded = 1;
        openai_state_valid = 1;
    }
}

void openai_state_save(void)
{
    openai_save_state();
}

void openai_load_state(void)
{
    FILE *file;
    char line[256];
    int format;
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int saw_format;
    int saw_workflow;
    int saw_build_known;
    int saw_build_status;
    int saw_rollback;

    if (openai_state_loaded) {
        return;
    }

    openai_state_loaded = 1;
    openai_state_valid = 0;

    format = 0;
    workflow = OPENAI_WORKFLOW_NONE;
    build_known = 0;
    build_status = 0;
    rollback = OPENAI_ROLLBACK_NONE;

    saw_format = 0;
    saw_workflow = 0;
    saw_build_known = 0;
    saw_build_status = 0;
    saw_rollback = 0;

    file = fopen(OPENAI_STATE_FILE, "r");

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        int value;

        if (sscanf(line, "format=%d", &value) == 1) {
            format = value;
            saw_format = 1;
        } else if (sscanf(
                       line,
                       "last_workflow=%d",
                       &value) == 1) {
            workflow = value;
            saw_workflow = 1;
        } else if (sscanf(
                       line,
                       "last_build_known=%d",
                       &value) == 1) {
            build_known = value;
            saw_build_known = 1;
        } else if (sscanf(
                       line,
                       "last_build_status=%d",
                       &value) == 1) {
            build_status = value;
            saw_build_status = 1;
        } else if (sscanf(
                       line,
                       "last_rollback=%d",
                       &value) == 1) {
            rollback = value;
            saw_rollback = 1;
        }
    }

    (void)fclose(file);

    if (!saw_format ||
        !saw_workflow ||
        !saw_build_known ||
        !saw_build_status ||
        !saw_rollback ||
        format != 1 ||
        workflow < OPENAI_WORKFLOW_NONE ||
        workflow > OPENAI_WORKFLOW_PLAN ||
        (build_known != 0 && build_known != 1) ||
        rollback < OPENAI_ROLLBACK_NONE ||
        rollback > OPENAI_ROLLBACK_DECLINED) {
        (void)puts(
            "Warning: OVMS_AGENT.STATE is invalid or unsupported; "
            "persisted workflow state was ignored."
        );
        return;
    }

    openai_last_workflow = workflow;
    openai_last_build_known = build_known;
    openai_last_build_status = build_status;
    openai_last_rollback = rollback;
    openai_state_valid = 1;
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

    (void)puts("State format:             1");
    (void)printf(
        "Last workflow:            %s\n",
        openai_workflow_name(openai_last_workflow)
    );

    if (openai_last_build_known) {
        (void)printf(
            "Last build:               %s (status %d)\n",
            (openai_last_build_status & 1) != 0 ?
                "success" : "failure",
            openai_last_build_status
        );
    } else {
        (void)puts(
            "Last build:               not recorded"
        );
    }

    (void)printf(
        "Last rollback:            %s\n",
        openai_rollback_name(openai_last_rollback)
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

    if (remove(OPENAI_STATE_FILE) != 0 && errno != ENOENT) {
        (void)printf(
            "Unable to remove %s: %s\n",
            OPENAI_STATE_FILE,
            strerror(errno)
        );
        return;
    }

    openai_last_workflow = OPENAI_WORKFLOW_NONE;
    openai_last_build_known = 0;
    openai_last_build_status = 0;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;
    openai_state_loaded = 1;
    openai_state_valid = 0;

    (void)puts("Persistent workflow state cleared.");
}

