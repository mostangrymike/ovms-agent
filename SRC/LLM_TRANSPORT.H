#ifndef LLM_TRANSPORT_H
#define LLM_TRANSPORT_H

int write_headers(const char *api_key);
void remove_temporary_files(void);
int perform_openai_request(void);

#endif /* LLM_TRANSPORT_H */
