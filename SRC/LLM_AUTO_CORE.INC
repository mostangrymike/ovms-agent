#include "llm_internal.h"
#include "rms_write.h"
#include "SETTINGS.H"

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

static unsigned int llm_auto_last_turns = 0U;
static unsigned int llm_auto_last_tools = 0U;
static unsigned int llm_auto_last_writes = 0U;
static int llm_auto_last_workflow = LLM_WORKFLOW_NONE;
static char llm_auto_last_reason[32] = "none";

/*
 * M279's separate create-action ceiling remains a legacy logical-only knob.
 * M281 moves only turn/write limits into SETTINGS, so retain this parser for
 * OVMS_AGENT_AUTO_CREATES rather than changing M279 behavior implicitly.
 */
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

    return (unsigned int)settings_effective_long(
        "auto_turns",
        "OVMS_AGENT_AUTO_TURNS",
        (long)LLM_AUTO_TURNS_DEFAULT,
        1L,
        (long)LLM_AUTO_TURNS_MAX
    );
}

static unsigned int llm_auto_write_cfg(void)
{
    if (llm_auto_write_override != 0U) {
        return llm_auto_write_override;
    }

    return (unsigned int)settings_effective_long(
        "auto_writes",
        "OVMS_AGENT_AUTO_WRITES",
        (long)LLM_AUTO_WRITES_DEFAULT,
        1L,
        (long)LLM_AUTO_WRITES_MAX
    );
}

void llm_auto_begin(int workflow)
{
    llm_auto_cur_turns = 0U;
    llm_auto_cur_tools = 0U;
    llm_auto_cur_writes = 0U;
    llm_auto_cur_workflow = workflow;
    llm_auto_write_blocked = 0;

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

int llm_auto_final_has_evidence(int workflow)
{
    if (workflow == LLM_WORKFLOW_PLAN) {
        return 1;
    }

    return llm_auto_cur_tools != 0U;
}

int llm_auto_allow_write(void)
{
    if (llm_auto_cur_workflow == LLM_WORKFLOW_WRITE &&
        rms_run_has_writes()) {
        llm_auto_write_blocked = 1;
        return 0;
    }

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
        llm_auto_write_blocked &&
        strcmp(reason, "turn-limit") == 0;

    if (bounded_write) {
        reason = "bounded-write";
        (void)puts(
            "Guarded write reached the turn limit after an additional write "
            "was blocked; retaining the earlier approved file changes."
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
}

int llm_auto_bounded_completed(void)
{
    return strcmp(llm_auto_last_reason, "bounded-write") == 0;
}

int llm_auto_limits_text(char *output, size_t output_size)
{
    const char *turn_source;
    const char *write_source;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    turn_source = settings_value_source(
        "auto_turns", "OVMS_AGENT_AUTO_TURNS");
    write_source = settings_value_source(
        "auto_writes", "OVMS_AGENT_AUTO_WRITES");

    written = snprintf(
        output, output_size,
        "OVMS Agent autonomous limits\n"
        "----------------------------\n"
        "Model/tool turns: %u (max %u, %s)\n"
        "Write actions:    %u (max %u, %s)\n"
        "Configure with SETTINGS: auto_turns and auto_writes.\n",
        llm_auto_turn_cfg(),
        LLM_AUTO_TURNS_MAX,
        turn_source,
        llm_auto_write_cfg(),
        LLM_AUTO_WRITES_MAX,
        write_source
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
