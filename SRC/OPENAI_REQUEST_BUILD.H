#ifndef OPENAI_REQUEST_BUILD_H
#define OPENAI_REQUEST_BUILD_H

int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt);

int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *context_items,
    const char *call_id,
    const char *tool_output);

#endif /* OPENAI_REQUEST_BUILD_H */
