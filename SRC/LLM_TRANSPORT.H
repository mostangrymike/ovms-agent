#ifndef LLM_TRANSPORT_H
#define LLM_TRANSPORT_H

typedef void (*llm_stream_cb)(const char *text);

int write_headers(const char *api_key);
void remove_temporary_files(void);
int perform_openai_request(void);
void llm_transport_set_stream(llm_stream_cb callback);
int llm_transport_streamed(void);

#endif /* LLM_TRANSPORT_H */
