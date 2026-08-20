#include "llm_internal.h"
#include "rms_write.h"

#define OPENAI_AUTO_TURNS_DEFAULT 12U
#define OPENAI_AUTO_TURNS_MAX 32U
#define OPENAI_AUTO_WRITES_DEFAULT 3U
#define OPENAI_AUTO_WRITES_MAX 8U

static unsigned int openai_auto_turn_override = 0U;
static unsigned int openai_auto_write_override = 0U;

static unsigned int openai_auto_cur_turns = 0U;
static unsigned int openai_auto_cur_tools = 0U;
static unsigned int openai_auto_cur_writes = 0U;
static int openai_auto_cur_workflow = OPENAI_WORKFLOW_NONE;
static int openai_auto_write_blocked = 0;

static unsigned int openai_auto_last_turns = 0U;
static unsigned int openai_auto_last_tools = 0U;
static unsigned int openai_auto_last_writes = 0U;
static int openai_auto_last_workflow = OPENAI_WORKFLOW_NONE;
static char openai_auto_last_reason[32] = "none";

static unsigned int openai_auto_env_limit(const char *name,
                                           unsigned int fallback,
                                           unsigned int maximum)
{
    const char *value;
    char *end;
    unsigned long parsed;

    value = getenv(name);

    if (value == NULL || *value == '\0') {
        return fallback;
    }

    end = NULL;
    parsed = strtoul(value, &end, 10);

    if (end == value || *end != '\0' ||
        parsed == 0UL || parsed > (unsigned long)maximum) {
        return fallback;
    }

    return (unsigned int)parsed;
}

static unsigned int openai_auto_turn_cfg(void)
{
    if (openai_auto_turn_override != 0U) {
        return openai_auto_turn_override;
    }

    return openai_auto_env_limit(
        "OVMS_AGENT_AUTO_TURNS",
        OPENAI_AUTO_TURNS_DEFAULT,
        OPENAI_AUTO_TURNS_MAX
    );
}

static unsigned int openai_auto_write_cfg(void)
{
    if (openai_auto_write_override != 0U) {
        return openai_auto_write_override;
    }

    return openai_auto_env_limit(
        "OVMS_AGENT_AUTO_WRITES",
        OPENAI_AUTO_WRITES_DEFAULT,
        OPENAI_AUTO_WRITES_MAX
    );
}

void openai_auto_begin(int workflow)
{
    openai_auto_cur_turns = 0U;
    openai_auto_cur_tools = 0U;
    openai_auto_cur_writes = 0U;
    openai_auto_cur_workflow = workflow;
    openai_auto_write_blocked = 0;

    if (workflow == OPENAI_WORKFLOW_WRITE) {
        rms_run_begin();
    } else {
        rms_run_commit();
    }
}

unsigned int openai_auto_turn_limit(int workflow)
{
    if (workflow == OPENAI_WORKFLOW_PLAN) {
        return OPENAI_PLAN_MAX_TURNS;
    }

    return openai_auto_turn_cfg();
}

void openai_auto_note_turn(void)
{
    ++openai_auto_cur_turns;
}

void openai_auto_note_tool(void)
{
    ++openai_auto_cur_tools;
}

int openai_auto_allow_write(void)
{
    if (openai_auto_cur_writes >= openai_auto_write_cfg()) {
        openai_auto_write_blocked = 1;
        return 0;
    }

    ++openai_auto_cur_writes;
    return 1;
}

int openai_auto_partial_limit(void)
{
    if (openai_auto_cur_workflow != OPENAI_WORKFLOW_WRITE ||
        !rms_run_has_writes()) {
        return 0;
    }

    return openai_auto_write_blocked ||
           openai_auto_cur_turns >= openai_auto_turn_cfg();
}

void openai_auto_finish(const char *reason)
{
    int rollback_needed;

    openai_auto_last_turns = openai_auto_cur_turns;
    openai_auto_last_tools = openai_auto_cur_tools;
    openai_auto_last_writes = openai_auto_cur_writes;
    openai_auto_last_workflow = openai_auto_cur_workflow;

    if (reason == NULL || *reason == '\0') {
        reason = "unknown";
    }

    rollback_needed =
        openai_auto_cur_workflow == OPENAI_WORKFLOW_WRITE &&
        rms_run_has_writes() &&
        (strcmp(reason, "final") != 0 || openai_auto_write_blocked);

    if (rollback_needed) {
        (void)puts(
            "Incomplete guarded write detected; restoring pre-run "
            "OpenVMS file versions."
        );

        if (rms_run_rollback()) {
            (void)puts(
                "Guarded write rollback complete. No partial file "
                "changes remain."
            );
        } else {
            (void)puts(
                "Guarded write rollback failed. Inspect the affected "
                "OpenVMS file versions before continuing."
            );
        }
    } else {
        rms_run_commit();
    }

    (void)strncpy(
        openai_auto_last_reason,
        reason,
        sizeof(openai_auto_last_reason) - 1U
    );
    openai_auto_last_reason[
        sizeof(openai_auto_last_reason) - 1U
    ] = '\0';
}

int openai_auto_limits_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent autonomous limits\n"
        "----------------------------\n"
        "Model/tool turns: %u (max %u)\n"
        "Write actions:    %u (max %u)\n"
        "Turn variable:    OVMS_AGENT_AUTO_TURNS\n"
        "Write variable:   OVMS_AGENT_AUTO_WRITES\n",
        openai_auto_turn_cfg(),
        OPENAI_AUTO_TURNS_MAX,
        openai_auto_write_cfg(),
        OPENAI_AUTO_WRITES_MAX
    );

    return written >= 0 && (size_t)written < output_size;
}

int openai_auto_status_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent autonomous loop status\n"
        "---------------------------------\n"
        "Last workflow: %s\n"
        "Turns:         %u\n"
        "Tool calls:    %u\n"
        "Write actions: %u\n"
        "Stop reason:   %s\n",
        openai_workflow_name(openai_auto_last_workflow),
        openai_auto_last_turns,
        openai_auto_last_tools,
        openai_auto_last_writes,
        openai_auto_last_reason
    );

    return written >= 0 && (size_t)written < output_size;
}

void openai_show_auto_limits(void)
{
    char output[1024];

    if (!openai_auto_limits_text(output, sizeof(output))) {
        (void)puts("Unable to show autonomous limits.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_auto_status(void)
{
    char output[1024];

    if (!openai_auto_status_text(output, sizeof(output))) {
        (void)puts("Unable to show autonomous loop status.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_auto_reset(void)
{
    openai_auto_last_turns = 0U;
    openai_auto_last_tools = 0U;
    openai_auto_last_writes = 0U;
    openai_auto_last_workflow = OPENAI_WORKFLOW_NONE;
    (void)strcpy(openai_auto_last_reason, "none");
}

void openai_auto_test_limits(unsigned int turns,
                             unsigned int writes)
{
    if (turns > OPENAI_AUTO_TURNS_MAX) {
        turns = OPENAI_AUTO_TURNS_MAX;
    }

    if (writes > OPENAI_AUTO_WRITES_MAX) {
        writes = OPENAI_AUTO_WRITES_MAX;
    }

    openai_auto_turn_override = turns;
    openai_auto_write_override = writes;
}
