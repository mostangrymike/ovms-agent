#include <stdio.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_image.h"

int openai_agent_image_set(const char *path);
void openai_agent_image_clear(void);

#include "OPENAI_AGENT.C"

int openai_image_log(const char *image_path,
                     const char *outcome,
                     int status)
{
    openai_image_meta meta;
    char event[768];

    if (image_path == NULL || outcome == NULL) return 0;

    if (openai_image_info(image_path, &meta)) {
        (void)snprintf(event, sizeof(event),
                       "%s path=%s type=%s size=%lu",
                       outcome, meta.path, meta.media_type, meta.size);
    } else {
        (void)snprintf(event, sizeof(event),
                       "%s path=%s", outcome, image_path);
    }

    openai_log_event("AGENT/IMAGE", event, status);
    return 1;
}

void openai_agent_image(agent_state *state,
                        const char *image_path,
                        const char *goal)
{
    openai_image_meta meta;

    if (state == NULL || image_path == NULL || goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/IMAGE image-path goal");
        return;
    }

    if (!openai_image_info(image_path, &meta)) {
        (void)openai_image_log(image_path, "image_rejected", 2);
        (void)puts("Image rejected: unsafe path, unsupported type, invalid signature, or size limit exceeded.");
        return;
    }

    if (!openai_agent_image_set(meta.path)) {
        openai_log_event("AGENT/IMAGE", "image_context_failed", 2);
        (void)puts("Unable to prepare image context.");
        return;
    }

    (void)openai_image_log(meta.path, "image_accepted", 1);
    openai_agent(state, goal);
    openai_agent_image_clear();
}
