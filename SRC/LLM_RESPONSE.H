#ifndef LLM_RESPONSE_H
#define LLM_RESPONSE_H

#include <stddef.h>

int save_response_id(const char *json);
int llm_response_empty_completed(const char *json,
                                 long *output_tokens);
int llm_text_tool_like(const char *text,
                       char *tool_name,
                       size_t tool_name_size);
int display_clean_response(void);
void display_raw_response(void);
int display_api_error_from_json(const char *json);
int display_output_text_from_json(const char *json);

#endif /* LLM_RESPONSE_H */
