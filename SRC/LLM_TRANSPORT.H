#ifndef LLM_TRANSPORT_H
#define LLM_TRANSPORT_H

int llm_write_headers(const char *api_key);
void llm_remove_temp_files(void);
int llm_perform_request(void);

#endif /* LLM_TRANSPORT_H */
