#ifndef LLM_REQUEST_AGENT_H
#define LLM_REQUEST_AGENT_H

int llm_m279_tool_model_ok(const char *url,
                           const char *model);

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

int write_agent_final_request(
    const char *model,
    const char *previous_id,
    const char *call_id,
    const char *tool_output);

int write_agent_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output);

#endif /* LLM_REQUEST_AGENT_H */
