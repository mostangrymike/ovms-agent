#include <string.h>

#include "github_m262.h"
#include "LLM_AUTO.H"
#include "LLM_CONTEXT.H"
#include "LLM_PATCH.H"
#include "LLM_PROJECT_MAP.H"
#include "LLM_USAGE.H"

#define command_register_agent command_register_agent_base
#define llm_show_github m262_show_github
#define llm_show_metrics m273_show_metrics
#include "COMMAND_AGENT.C"
#undef llm_show_metrics
#undef llm_show_github
#undef command_register_agent

void llm_agent_image(agent_state *state,
                     const char *image_path,
                     const char *goal);
void llm_agent_autopilot(agent_state *state,
                         const char *goal);
void llm_exec_create_context(agent_state *state,
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

static void m268_command_autopilot(agent_state *state,
                                   const char *arguments)
{
    if (arguments == NULL || *arguments == '\0') {
        (void)puts("Usage: AGENT/AUTOPILOT goal");
        return;
    }

    llm_agent_autopilot(state, arguments);
}

static void m277_command_exec_create(agent_state *state,
                                     const char *arguments)
{
    if (arguments == NULL || *arguments == '\0') {
        (void)puts("Usage: AGENT/EXEC/RESUME/CREATE goal");
        return;
    }

    llm_exec_create_context(state, arguments);
}

static const command_entry m259_extra_commands[] = {
    { "AGENT/IMAGE",
      "Run read-only agent with local image context: AGENT/IMAGE image-path goal",
      m259_command_image },
    { "AGENT/AUTOPILOT",
      "Run bounded local build/repair/rebuild loop: AGENT/AUTOPILOT goal",
      m268_command_autopilot },
    { "AGENT/EXEC/RESUME/CREATE",
      "Resume persistent context and create one guarded file: AGENT/EXEC/RESUME/CREATE goal",
      m277_command_exec_create }
};

void command_register_agent(void)
{
    command_register_agent_base();
    (void)command_registry_add(
        m259_extra_commands,
        sizeof(m259_extra_commands) / sizeof(m259_extra_commands[0])
    );
}
