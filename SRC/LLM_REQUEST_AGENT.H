#ifndef LLM_REQUEST_AGENT_H
#define LLM_REQUEST_AGENT_H

int write_create_agent_request(
    const char *model,
    const char *prompt,
    int include_write_tool);

int write_agent_request_mode(
    const char *model,
    const char *prompt,
    const char *previous_id,
    int include_write_tool);

int write_agent_final_request(
    const char *model,
    const char *previous_id,
    const char *tool_call_id,
    const char *tool_output);

int write_agent_request(
    const char *model,
    const char *prompt,
    const char *previous_id);

#endif /* LLM_REQUEST_AGENT_H */
