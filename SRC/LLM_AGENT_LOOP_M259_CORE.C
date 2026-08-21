#include <stdio.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_IMAGE.H"
#include "openai_request_agent.h"

int write_agent_image_req(const char *model,
                          const char *instructions,
                          const char *user_prompt,
                          const char *image_path,
                          int allow_write);

#define M259_AGENT_IMAGE_PATH 512U
static char m259_agent_image[M259_AGENT_IMAGE_PATH];

int openai_agent_image_set(const char *path)
{
    llm_image_meta meta;

    m259_agent_image[0] = '\0';
    if (!llm_image_info(path, &meta)) return 0;
    if (strlen(meta.path) >= sizeof(m259_agent_image)) return 0;
    (void)strcpy(m259_agent_image, meta.path);
    return 1;
}

void openai_agent_image_clear(void)
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
        openai_agent_image_clear();
        return result;
    }

    return write_agent_request_mode(model, instructions, user_prompt,
                                    previous_id, call_id, tool_output,
                                    allow_write);
}

int openai_test_agent_image_req(const char *model,
                                const char *instructions,
                                const char *goal,
                                const char *image_path)
{
    int result;

    if (!openai_agent_image_set(image_path)) return 0;
    result = m259_agent_request(model, instructions, goal,
                                NULL, NULL, NULL, 0);
    openai_agent_image_clear();
    return result;
}

#define write_agent_request_mode m259_agent_request
#include "LLM_AGENT_LOOP.C"
#undef write_agent_request_mode