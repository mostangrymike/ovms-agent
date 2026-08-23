#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_SSE.H"
#include "LLM_TRANSPORT.H"
#include "llm_config.h"
#include "ANSI_TERM.H"

#define M262_RESPONSE_URL_MAX 640U
#define M273_STREAM_REQ "OVMS_AGENT_STREAM_REQ.TMP"

static llm_stream_cb m273_stream_output = NULL;
static int m273_last_streamed = 0;
static int m273_stream_last_nl = 1;

static int m262_response_url(const char *base,
                             char *output,
                             size_t output_size)
{
    static const char suffix[] = "/responses";
    size_t length;
    size_t suffix_length;

    if (base == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    length = strlen(base);
    while (length > 0U && base[length - 1U] == '/') {
        --length;
    }

    suffix_length = strlen(suffix);
    if (length >= suffix_length &&
        strncmp(base + length - suffix_length,
                suffix, suffix_length) == 0) {
        if (length + 1U > output_size) {
            return 0;
        }
        (void)memcpy(output, base, length);
        output[length] = '\0';
        return 1;
    }

    if (length + suffix_length + 1U > output_size) {
        return 0;
    }

    (void)memcpy(output, base, length);
    (void)memcpy(output + length, suffix, suffix_length + 1U);
    return 1;
}

static int m262_url_valid(const char *url)
{
    const unsigned char *cursor;

    if (url == NULL ||
        (strncmp(url, "https://", 8U) != 0 &&
         strncmp(url, "http://", 7U) != 0)) {
        return 0;
    }

    cursor = (const unsigned char *)url;
    while (*cursor != '\0') {
        if (!(isalnum(*cursor) ||
              *cursor == ':' || *cursor == '/' || *cursor == '.' ||
              *cursor == '_' || *cursor == '-' || *cursor == '~')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

void llm_transport_set_stream(llm_stream_cb callback)
{
    m273_stream_output = callback;
}

int llm_transport_streamed(void)
{
    return m273_last_streamed;
}

static void m273_remove_all(const char *path)
{
    if (path == NULL || *path == '\0') {
        return;
    }

    while (remove(path) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static void m273_emit_text(const char *text)
{
    size_t length;

    if (text == NULL || *text == '\0') {
        return;
    }

    /*
     * AGENT/PLAN saves and displays the completed plan as one object.
     * Keep that path non-incremental until plan persistence itself has
     * a streaming-safe contract; ordinary response modes may stream.
     */
    if (llm_last_workflow == LLM_WORKFLOW_PLAN) {
        return;
    }

    length = strlen(text);
    m273_last_streamed = 1;
    m273_stream_last_nl = text[length - 1U] == '\n';

    if (m273_stream_output != NULL) {
        m273_stream_output(text);
    }
}

static int m273_make_stream_req(void)
{
    FILE *input;
    FILE *output;
    char *data;
    long length;
    size_t actual;
    size_t end;
    int success;

    input = fopen(LLM_REQUEST_FILE, "r");
    if (input == NULL) {
        return 0;
    }

    if (fseek(input, 0L, SEEK_END) != 0) {
        (void)fclose(input);
        return 0;
    }

    length = ftell(input);
    if (length < 0L ||
        fseek(input, 0L, SEEK_SET) != 0) {
        (void)fclose(input);
        return 0;
    }

    data = (char *)malloc((size_t)length + 1U);
    if (data == NULL) {
        (void)fclose(input);
        return 0;
    }

    actual = fread(data, 1U, (size_t)length, input);
    success = !ferror(input) && fclose(input) == 0;
    if (!success) {
        free(data);
        return 0;
    }

    data[actual] = '\0';
    end = actual;

    while (end > 0U &&
           isspace((unsigned char)data[end - 1U])) {
        --end;
    }

    if (end == 0U || data[end - 1U] != '}') {
        free(data);
        return 0;
    }

    m273_remove_all(M273_STREAM_REQ);
    output = fopen(M273_STREAM_REQ, "w");
    if (output == NULL) {
        free(data);
        return 0;
    }

    success =
        fwrite(data, 1U, end - 1U, output) == end - 1U &&
        fputs(",\"stream\":true}\n", output) != EOF;

    if (fclose(output) != 0) {
        success = 0;
    }

    free(data);
    return success;
}

static int m273_stream_request(const char *url)
{
    static const char prefix[] =
        "curl --silent --show-error --no-buffer "
        "--header @" LLM_HEADERS_FILE " "
        "--data-binary @" M273_STREAM_REQ " ";
    FILE *pipe_stream;
    char command[1024];
    size_t needed;
    int parsed;
    int emitted;
    int close_status;

    if (!m273_make_stream_req()) {
        return -1;
    }

    needed = strlen(prefix) + strlen(url) + 1U;
    if (needed > sizeof(command)) {
        m273_remove_all(M273_STREAM_REQ);
        return -1;
    }

    (void)strcpy(command, prefix);
    (void)strcat(command, url);

    (void)fflush(NULL);
    pipe_stream = popen(command, "r");
    if (pipe_stream == NULL) {
        m273_remove_all(M273_STREAM_REQ);
        return -1;
    }

    (void)setvbuf(pipe_stream, NULL, _IONBF, 0);
    emitted = 0;
    parsed = llm_sse_parse(
        pipe_stream,
        m273_emit_text,
        LLM_RESPONSE_FILE,
        &emitted
    );

    close_status = pclose(pipe_stream);
    m273_remove_all(M273_STREAM_REQ);

    if (m273_last_streamed &&
        !m273_stream_last_nl &&
        m273_stream_output != NULL) {
        m273_stream_output("\n");
        m273_stream_last_nl = 1;
    }

    if (parsed && close_status != -1) {
        return 1;
    }

    /*
     * A stream attempt that produced no user-visible assistant text is
     * safe to retry through the pre-M273 blocking transport.  This is
     * the compatibility path for gateways whose SSE dialect differs
     * from the Responses event stream we understand.  Once any text
     * has been emitted, never retry the provider request: doing so could
     * duplicate both output and side effects/cost.
     */
    if (!m273_last_streamed) {
        if (getenv("OVMS_AGENT_STREAM_DEBUG") != NULL) {
            (void)puts(
                "M273 stream was not recognized; retrying blocking transport."
            );
        }
        return -1;
    }

    return 0;
}

int write_headers(const char *api_key)
{
    FILE *file;
    int success;

    file = fopen(LLM_HEADERS_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_HEADERS_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("Content-Type: application/json\n", file) == EOF ||
        fputs("Authorization: Bearer ", file) == EOF ||
        fputs(api_key, file) == EOF ||
        fputc('\n', file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write service headers.");
        return 0;
    }

    return 1;
}

void remove_temporary_files(void)
{
    (void)remove(LLM_REQUEST_FILE);
    (void)remove(LLM_HEADERS_FILE);
    m273_remove_all(M273_STREAM_REQ);
}

int perform_openai_request(void)
{
    static const char prefix[] =
        "curl --silent --show-error "
        "--output " LLM_RESPONSE_FILE " "
        "--header @" LLM_HEADERS_FILE " "
        "--data-binary @" LLM_REQUEST_FILE " ";
    const char *base;
    char url[M262_RESPONSE_URL_MAX];
    char command[1024];
    size_t needed;
    int status;

    m273_last_streamed = 0;
    m273_stream_last_nl = 1;

    /*
     * Streaming is an interactive capability, never the default wire
     * contract. ANSI_TERM remains the authority for terminal detection;
     * OVMS_AGENT_NOSTREAM independently disables SSE without disabling
     * color/status presentation.
     */
    if (m273_stream_output == ansi_term_stream &&
        (getenv("OVMS_AGENT_NOSTREAM") != NULL ||
         !ansi_term_enabled())) {
        m273_stream_output = NULL;
    } else if (m273_stream_output == NULL &&
               getenv("OVMS_AGENT_NOSTREAM") == NULL &&
               ansi_term_enabled()) {
        m273_stream_output = ansi_term_stream;
    }

    base = llm_api_url();
    if (base == NULL || *base == '\0') {
        (void)puts(
            "No AI provider service address is configured. "
            "Use PROVIDER ADD or PROVIDER USE."
        );
        return 0;
    }

    if (!m262_url_valid(base) ||
        !m262_response_url(base, url, sizeof(url))) {
        (void)puts("Configured AI provider service address is invalid.");
        return 0;
    }

    if (m273_stream_output != NULL) {
        status = m273_stream_request(url);
        if (status >= 0) {
            return status;
        }
    }

    needed = strlen(prefix) + strlen(url) + 1U;
    if (needed > sizeof(command)) {
        (void)puts("Configured AI provider service address is too long.");
        return 0;
    }

    (void)strcpy(command, prefix);
    (void)strcat(command, url);
    status = system(command);

    return (status & 1) != 0;
}
