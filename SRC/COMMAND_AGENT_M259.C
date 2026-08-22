#include <string.h>

#include "github_m262.h"
#include "LLM_AUTO.H"
#include "LLM_CONTEXT.H"
#include "LLM_PATCH.H"
#include "LLM_PROJECT_MAP.H"

#define command_register_agent command_register_agent_base
#define openai_show_github m262_show_github
#define openai_selftest llm_selftest
#define openai_status llm_status
#define openai_verify llm_verify
#define openai_show_log llm_show_log
#define openai_show_old_log llm_show_old_log
#define openai_clear_log llm_clear_log
#define openai_show_metrics llm_show_metrics
#define openai_show_state llm_show_state
#define openai_show_memory llm_show_memory
#define openai_clear_state llm_clear_state
#define openai_show_auto_status llm_show_auto_status
#define openai_show_auto_limits llm_show_auto_limits
#define openai_auto_reset llm_auto_reset
#define openai_show_context_current llm_show_context_current
#define openai_patch_apply_cmd llm_patch_apply_cmd
#define openai_patch_dry_cmd llm_patch_dry_cmd
#define openai_patch_validate_cmd llm_patch_validate_cmd
#define openai_patch_last_cmd llm_patch_last_cmd
#define openai_show_project_map llm_show_project_map
#define openai_show_project_ctx llm_show_project_ctx
#define openai_project_refresh_cmd llm_project_refresh_cmd
#define openai_show_instr_status llm_show_instr_status
#define openai_show_instr llm_show_instr
#define openai_instr_reload_cmd llm_instr_reload_cmd
#define openai_show_git_status llm_show_git_status
#define openai_show_git_diff llm_show_git_diff
#define openai_show_git_changed llm_show_git_changed
#define openai_show_git_context llm_show_git_context
#define openai_git_refresh_cmd llm_git_refresh_cmd
#include "COMMAND_AGENT.C"
#undef openai_git_refresh_cmd
#undef openai_show_git_context
#undef openai_show_git_changed
#undef openai_show_git_diff
#undef openai_show_git_status
#undef openai_instr_reload_cmd
#undef openai_show_instr
#undef openai_show_instr_status
#undef openai_project_refresh_cmd
#undef openai_show_project_ctx
#undef openai_show_project_map
#undef openai_patch_last_cmd
#undef openai_patch_validate_cmd
#undef openai_patch_dry_cmd
#undef openai_patch_apply_cmd
#undef openai_show_context_current
#undef openai_auto_reset
#undef openai_show_auto_limits
#undef openai_show_auto_status
#undef openai_clear_state
#undef openai_show_memory
#undef openai_show_state
#undef openai_show_metrics
#undef openai_clear_log
#undef openai_show_old_log
#undef openai_show_log
#undef openai_verify
#undef openai_status
#undef openai_selftest
#undef openai_show_github
#undef command_register_agent

void llm_agent_image(agent_state *state,
                     const char *image_path,
                     const char *goal);

static void m259_command_image(agent_state *state,
                               const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *image_path;

    if (arguments == NULL || *arguments == '\0' ||
        strlen(arguments) >= sizeof(work)) {
        (void)puts("Usage: AGENT/IMAGE image-path goal");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    image_path = command_next_argument(&cursor);
    while (cursor != NULL && (*cursor == ' ' || *cursor == '\t')) ++cursor;

    if (image_path == NULL || cursor == NULL || *cursor == '\0') {
        (void)puts("Usage: AGENT/IMAGE image-path goal");
        return;
    }

    llm_agent_image(state, image_path, cursor);
}

static const command_entry m259_image_command[] = {
    { "AGENT/IMAGE",
      "Run read-only agent with local image context: AGENT/IMAGE image-path goal",
      m259_command_image }
};

void command_register_agent(void)
{
    command_register_agent_base();
    (void)command_registry_add(
        m259_image_command,
        sizeof(m259_image_command) / sizeof(m259_image_command[0])
    );
}
