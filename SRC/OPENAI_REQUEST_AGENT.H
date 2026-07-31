#ifndef OPENAI_REQUEST_AGENT_H
#define OPENAI_REQUEST_AGENT_H

int write_create_agent_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output);

int write_agent_request_mode(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output,
    int allow_write);

int write_agent_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output);

#endif /* OPENAI_REQUEST_AGENT_H */
