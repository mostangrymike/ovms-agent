#include <string.h>

#include "github_m262.h"

#define command_register_agent command_register_agent_base
#define openai_show_github m262_show_github
#include "COMMAND_AGENT.C"
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
