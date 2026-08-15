#include <stdio.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_image.h"

int openai_agent_image_set(const char *path);
void openai_agent_image_clear(void);

#include "OPENAI_AGENT.C"

void openai_agent_image(agent_state *state,
                        const char *image_path,
                        const char *goal)
{
    openai_image_meta meta;
    char event[768];

    if (state == NULL || image_path == NULL || goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/IMAGE image-path goal");
        return;
    }

    if (!openai_image_info(image_path, &meta)) {
        (void)snprintf(event, sizeof(event),
                       "image_rejected path=%s", image_path);
        openai_log_event("AGENT/IMAGE", event, 2);
        (void)puts("Image rejected: unsafe path, unsupported type, invalid signature, or size limit exceeded.");
        return;
    }

    if (!openai_agent_image_set(meta.path)) {
        openai_log_event("AGENT/IMAGE", "image_context_failed", 2);
        (void)puts("Unable to prepare image context.");
        return;
    }

    (void)snprintf(event, sizeof(event),
                   "image_accepted path=%s type=%s size=%lu",
                   meta.path, meta.media_type, meta.size);
    openai_log_event("AGENT/IMAGE", event, 1);
    openai_agent(state, goal);
    openai_agent_image_clear();
}
