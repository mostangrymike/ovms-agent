#include <stdio.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_IMAGE.H"
#include "LLM_AUTO.H"
#include "LLM_PATCH.H"
#include "openai_request_agent.h"

int write_agent_image_req(const char *model,
                          const char *instructions,
                          const char *user_prompt,
                          const char *image_path,
                          int allow_write);

#define M259_AGENT_IMAGE_PATH 512U
static char m259_agent_image[M259_AGENT_IMAGE_PATH];

int llm_agent_image_set(const char *path)
{
    llm_image_meta meta;

    m259_agent_image[0] = '\0';
    if (!llm_image_info(path, &meta)) return 0;
    if (strlen(meta.path) >= sizeof(m259_agent_image)) return 0;
    (void)strcpy(m259_agent_image, meta.path);
    return 1;
}

void llm_agent_image_clear(void)
{
    m259_agent_image[0] = '\0';
}

static int m259_agent_request(const char *model,
                              const char *instructions,
                              const char *user_prompt,
                              const char *previous_id,
                              const char *call_id,
                              const char *tool_output,
                              int allow_write)
{
    int result;

    if (m259_agent_image[0] != '\0' &&
        previous_id == NULL && call_id == NULL && tool_output == NULL) {
        result = write_agent_image_req(model, instructions, user_prompt,
                                       m259_agent_image, allow_write);
        llm_agent_image_clear();
        return result;
    }

    return write_agent_request_mode(model, instructions, user_prompt,
                                    previous_id, call_id, tool_output,
                                    allow_write);
}

int llm_test_agent_image_req(const char *model,
                             const char *instructions,
                             const char *goal,
                             const char *image_path)
{
    int result;

    if (!llm_agent_image_set(image_path)) return 0;
    result = m259_agent_request(model, instructions, goal,
                                NULL, NULL, NULL, 0);
    llm_agent_image_clear();
    return result;
}

#define openai_auto_begin llm_auto_begin
#define openai_auto_turn_limit llm_auto_turn_limit
#define openai_auto_note_turn llm_auto_note_turn
#define openai_auto_note_tool llm_auto_note_tool
#define openai_auto_allow_write llm_auto_allow_write
#define openai_auto_partial_limit llm_auto_partial_limit
#define openai_auto_finish llm_auto_finish
#define openai_auto_limits_text llm_auto_limits_text
#define openai_auto_status_text llm_auto_status_text
#define openai_show_auto_limits llm_show_auto_limits
#define openai_show_auto_status llm_show_auto_status
#define openai_auto_reset llm_auto_reset
#define openai_auto_test_limits llm_auto_test_limits
#define openai_patch_apply_json llm_patch_apply_json
#define openai_tool_descriptor llm_tool_descriptor
#define openai_tool_find llm_tool_find
#define openai_tool_is_read llm_tool_is_read
#define openai_tool_is_replace llm_tool_is_replace
#define openai_tool_execute_read llm_tool_execute_read
#define OPENAI_TOOL_REPLACE_LINES LLM_TOOL_REPLACE_LINES
#define write_agent_request_mode m259_agent_request
#include "LLM_AGENT_LOOP.C"
#undef write_agent_request_mode
#undef OPENAI_TOOL_REPLACE_LINES
#undef openai_tool_execute_read
#undef openai_tool_is_replace
#undef openai_tool_is_read
#undef openai_tool_find
#undef openai_tool_descriptor
#undef openai_patch_apply_json
#undef openai_auto_test_limits
#undef openai_auto_reset
#undef openai_show_auto_status
#undef openai_show_auto_limits
#undef openai_auto_status_text
#undef openai_auto_limits_text
#undef openai_auto_finish
#undef openai_auto_partial_limit
#undef openai_auto_allow_write
#undef openai_auto_note_tool
#undef openai_auto_note_turn
#undef openai_auto_turn_limit
#undef openai_auto_begin
