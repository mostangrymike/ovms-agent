#ifndef LLM_TRANSPORT_H
#define LLM_TRANSPORT_H

#define LLM_TRANSPORT_CONNECT_TIMEOUT_SECONDS 10U
#define LLM_TRANSPORT_REQUEST_TIMEOUT_SECONDS 120U
#define LLM_TRANSPORT_CURL_TIMEOUT_ARGS \
    "--connect-timeout 10 --max-time 120 "

#define LLM_PROBE_REQUEST_FILE  "OVMS_AGENT_PROVIDER_REQ.TMP"
#define LLM_PROBE_HEADERS_FILE  "OVMS_AGENT_PROVIDER_HEAD.TMP"
#define LLM_PROBE_RESPONSE_FILE "OVMS_AGENT_PROVIDER_RESP.TMP"

typedef void (*llm_stream_cb)(const char *text);

int write_headers(const char *api_key);
void remove_temporary_files(void);
int perform_openai_request(void);
int llm_transport_probe(const char *base);
void llm_transport_set_stream(llm_stream_cb callback);
int llm_transport_streamed(void);

#endif /* LLM_TRANSPORT_H */
