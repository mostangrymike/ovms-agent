#ifndef LLM_REQUEST_BUILD_H
#define LLM_REQUEST_BUILD_H

int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt);

int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *context_items,
    const char *call_id,
    const char *tool_output);

#endif /* LLM_REQUEST_BUILD_H */
