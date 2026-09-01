#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_USAGE.H"

/* M276 bounded provider-rate-limit parsing is part of the transport object. */
#include "LLM_RATE_LIMIT.C"

int llm_m294_stream_ok(const char *url,
                       const char *model,
                       const char *request)
{
    static const char groq_host[] = "api.groq.com";
    static const char gpt_oss[] = "openai/gpt-oss-";
    static const char qwen_36_27b[] = "qwen/qwen3.6-27b";

    if (request == NULL || strstr(request, "\"tools\":") == NULL ||
        url == NULL || model == NULL) {
        return 1;
    }

    if (strstr(url, groq_host) == NULL) {
        return 1;
    }

    if (strncmp(model, gpt_oss, sizeof(gpt_oss) - 1U) == 0) {
        return 0;
    }

    return strcmp(model, qwen_36_27b) != 0;
}

static char *m294_transport_env(const char *name)
{
    char *value;
    char *request;
    int stream_ok;

    if (name == NULL) {
        return NULL;
    }

    if (strcmp(name, "OVMS_AGENT_NOSTREAM") != 0) {
        return getenv(name);
    }

    value = getenv(name);
    if (value != NULL) {
        return value;
    }

    request = read_entire_file(LLM_REQUEST_FILE, NULL);
    stream_ok = llm_m294_stream_ok(
        llm_api_url(), llm_model(), request);
    free(request);

    return stream_ok ? NULL : "1";
}

#define getenv m294_transport_env
#define perform_openai_request m273_raw_request
#include "LLM_TRANSPORT.C"
#undef perform_openai_request
#undef getenv

int perform_openai_request(void)
{
    int result;

    result = m273_raw_request();
    if (result) {
        (void)llm_usage_record_file(LLM_RESPONSE_FILE);
    }

    return result;
}

int llm_transport_probe(const char *base)
{
    static const char prefix[] =
        "curl --silent --show-error "
        LLM_TRANSPORT_CURL_TIMEOUT_ARGS
        "--output " LLM_PROBE_RESPONSE_FILE " "
        "--header @" LLM_PROBE_HEADERS_FILE " "
        "--data-binary @" LLM_PROBE_REQUEST_FILE " ";
    char url[M262_RESPONSE_URL_MAX];
    char command[1024];
    char *response;
    size_t needed;
    unsigned int retries;
    unsigned int wait_seconds;
    int status;

    if (base == NULL || *base == '\0' ||
        !m262_url_valid(base) ||
        !m262_response_url(base, url, sizeof(url))) {
        return 0;
    }

    needed = strlen(prefix) + strlen(url) + 1U;
    if (needed > sizeof(command)) {
        return 0;
    }

    (void)strcpy(command, prefix);
    (void)strcat(command, url);
    retries = 0U;

    for (;;) {
        m273_remove_all(LLM_PROBE_RESPONSE_FILE);
        status = system(command);
        if (!ovms_status_success(
                (unsigned long)(unsigned int)status)) {
            return 0;
        }

        response = read_entire_file(LLM_PROBE_RESPONSE_FILE, NULL);
        if (response == NULL) {
            return 0;
        }

        wait_seconds = 0U;
        if (!llm_rate_limit_delay(response, &wait_seconds)) {
            free(response);
            return 1;
        }

        free(response);

        if (retries >= M276_RATE_RETRIES) {
            return 1;
        }

        ++retries;
        (void)sleep(wait_seconds);
    }
}
