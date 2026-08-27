#include "llm_internal.h"
#include "rms_write.h"

#define LLM_AUTO_TURNS_DEFAULT 12U
#define LLM_AUTO_TURNS_MAX 32U
#define LLM_AUTO_WRITES_DEFAULT 3U
#define LLM_AUTO_WRITES_MAX 8U

static unsigned int llm_auto_turn_override = 0U;
static unsigned int llm_auto_write_override = 0U;

static unsigned int llm_auto_cur_turns = 0U;
static unsigned int llm_auto_cur_tools = 0U;
static unsigned int llm_auto_cur_writes = 0U;
static int llm_auto_cur_workflow = LLM_WORKFLOW_NONE;
static int llm_auto_write_blocked = 0;
static int llm_auto_declined_after_write = 0;

static unsigned int llm_auto_last_turns = 0U;
static unsigned int llm_auto_last_tools = 0U;
static unsigned int llm_auto_last_writes = 0U;
static int llm_auto_last_workflow = LLM_WORKFLOW_NONE;
static char llm_auto_last_reason[32] = "none";

static unsigned int llm_auto_env_limit(const char *name,
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

static unsigned int llm_auto_turn_cfg(void)
{
    if (llm_auto_turn_override != 0U) {
        return llm_auto_turn_override;
    }

    return llm_auto_env_limit(
        "OVMS_AGENT_AUTO_TURNS",
        LLM_AUTO_TURNS_DEFAULT,
        LLM_AUTO_TURNS_MAX
    );
}

static unsigned int llm_auto_write_cfg(void)
{
    if (llm_auto_write_override != 0U) {
        return llm_auto_write_override;
    }

    return llm_auto_env_limit(
        "OVMS_AGENT_AUTO_WRITES",
        LLM_AUTO_WRITES_DEFAULT,
        LLM_AUTO_WRITES_MAX
    );
}

void llm_auto_begin(int workflow)
{
    llm_auto_cur_turns = 0U;
    llm_auto_cur_tools = 0U;
    llm_auto_cur_writes = 0U;
    llm_auto_cur_workflow = workflow;
    llm_auto_write_blocked = 0;
    llm_auto_declined_after_write = 0;

    if (workflow == LLM_WORKFLOW_WRITE) {
        rms_run_begin();
    } else {
        rms_run_commit();
    }
}

unsigned int llm_auto_turn_limit(int workflow)
{
    if (workflow == LLM_WORKFLOW_PLAN) {
        return LLM_PLAN_MAX_TURNS;
    }

    return llm_auto_turn_cfg();
}

void llm_auto_note_turn(void)
{
    ++llm_auto_cur_turns;
}

void llm_auto_note_tool(void)
{
    ++llm_auto_cur_tools;
}

void llm_auto_note_applied(void)
{
    if (llm_auto_cur_workflow == LLM_WORKFLOW_WRITE) {
        llm_auto_declined_after_write = 0;
    }
}

void llm_auto_note_decline(void)
{
    if (llm_auto_cur_workflow == LLM_WORKFLOW_WRITE &&
        rms_run_has_writes()) {
        llm_auto_declined_after_write = 1;
    }
}

int llm_auto_final_has_evidence(int workflow)
{
    if (workflow == LLM_WORKFLOW_PLAN) {
        return 1;
    }

    return llm_auto_cur_tools != 0U;
}

int llm_auto_allow_write(void)
{
    if (llm_auto_cur_writes >= llm_auto_write_cfg()) {
        llm_auto_write_blocked = 1;
        return 0;
    }

    ++llm_auto_cur_writes;
    return 1;
}

int llm_auto_partial_limit(void)
{
    if (llm_auto_cur_workflow != LLM_WORKFLOW_WRITE ||
        !rms_run_has_writes()) {
        return 0;
    }

    return llm_auto_write_blocked ||
           llm_auto_cur_turns >= llm_auto_turn_cfg();
}

void llm_auto_finish(const char *reason)
{
    int rollback_needed;
    int bounded_write;

    llm_auto_last_turns = llm_auto_cur_turns;
    llm_auto_last_tools = llm_auto_cur_tools;
    llm_auto_last_writes = llm_auto_cur_writes;
    llm_auto_last_workflow = llm_auto_cur_workflow;

    if (reason == NULL || *reason == '\0') {
        reason = "unknown";
    }

    bounded_write =
        llm_auto_cur_workflow == LLM_WORKFLOW_WRITE &&
        rms_run_has_writes() &&
        llm_auto_declined_after_write &&
        strcmp(reason, "turn-limit") == 0;

    if (bounded_write) {
        reason = "bounded-write";
        (void)puts(
            "Guarded write reached the turn limit after a later patch was "
            "declined; retaining earlier approved file changes."
        );
    }

    rollback_needed =
        llm_auto_cur_workflow == LLM_WORKFLOW_WRITE &&
        rms_run_has_writes() &&
        strcmp(reason, "final") != 0 &&
        strcmp(reason, "bounded-write") != 0;

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
        llm_auto_last_reason,
        reason,
        sizeof(llm_auto_last_reason) - 1U
    );
    llm_auto_last_reason[
        sizeof(llm_auto_last_reason) - 1U
    ] = '\0';
    llm_auto_declined_after_write = 0;
}

int llm_auto_limits_text(char *output, size_t output_size)
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
        llm_auto_turn_cfg(),
        LLM_AUTO_TURNS_MAX,
        llm_auto_write_cfg(),
        LLM_AUTO_WRITES_MAX
    );

    return written >= 0 && (size_t)written < output_size;
}

int llm_auto_status_text(char *output, size_t output_size)
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
        llm_workflow_name(llm_auto_last_workflow),
        llm_auto_last_turns,
        llm_auto_last_tools,
        llm_auto_last_writes,
        llm_auto_last_reason
    );

    return written >= 0 && (size_t)written < output_size;
}

void llm_show_auto_limits(void)
{
    char output[1024];

    if (!llm_auto_limits_text(output, sizeof(output))) {
        (void)puts("Unable to show autonomous limits.");
        return;
    }

    (void)fputs(output, stdout);
}

void llm_show_auto_status(void)
{
    char output[1024];

    if (!llm_auto_status_text(output, sizeof(output))) {
        (void)puts("Unable to show autonomous loop status.");
        return;
    }

    (void)fputs(output, stdout);
}

void llm_auto_reset(void)
{
    llm_auto_last_turns = 0U;
    llm_auto_last_tools = 0U;
    llm_auto_last_writes = 0U;
    llm_auto_last_workflow = LLM_WORKFLOW_NONE;
    llm_auto_declined_after_write = 0;
    (void)strcpy(llm_auto_last_reason, "none");
}

void llm_auto_test_limits(unsigned int turns,
                             unsigned int writes)
{
    if (turns > LLM_AUTO_TURNS_MAX) {
        turns = LLM_AUTO_TURNS_MAX;
    }

    if (writes > LLM_AUTO_WRITES_MAX) {
        writes = LLM_AUTO_WRITES_MAX;
    }

    llm_auto_turn_override = turns;
    llm_auto_write_override = writes;
}
