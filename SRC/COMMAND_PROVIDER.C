#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ANSI_TERM.H"
#include "command_internal.h"
#include "llm_config.h"
#include "LLM_JSON_PARSE.H"
#include "LLM_JSON_WRITE.H"
#include "LLM_RESPONSE.H"
#include "LLM_TRANSPORT.H"
#include "secret_input.h"

#define PROV_PROBE_PROMPT "Reply with exactly: OK"
#define PROV_PROBE_TOKENS 64

static int prov_read_value(const char *prompt,
                           char *buffer,
                           size_t buffer_size)
{
    size_t length;

    if (prompt == NULL || buffer == NULL || buffer_size < 2U) {
        return 0;
    }

    (void)fputs(prompt, stdout);
    (void)fflush(stdout);

    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    while (length > 0U &&
           (buffer[length - 1U] == '\n' ||
            buffer[length - 1U] == '\r')) {
        buffer[--length] = '\0';
    }

    return buffer[0] != '\0';
}

static const llm_provider *prov_find(const char *name)
{
    unsigned int count;
    unsigned int index;

    if (name == NULL || *name == '\0') {
        return llm_prov_active();
    }

    count = llm_prov_count();
    for (index = 0U; index < count; ++index) {
        const llm_provider *provider;

        provider = llm_prov_get(index);
        if (provider != NULL &&
            strcmp(provider->name, name) == 0) {
            return provider;
        }
    }

    return NULL;
}

static void prov_list(void)
{
    const llm_provider *active;
    unsigned int count;
    unsigned int index;

    active = llm_prov_active();
    count = llm_prov_count();

    if (count == 0U) {
        (void)puts("No AI provider profiles are configured.");
        return;
    }

    (void)puts("AI provider profiles:");
    for (index = 0U; index < count; ++index) {
        const llm_provider *provider;

        provider = llm_prov_get(index);
        if (provider == NULL) {
            continue;
        }

        (void)printf(
            "  %c %-16s model=%s key=%s\n",
            provider == active ? '*' : ' ',
            provider->name,
            provider->model[0] != '\0' ?
                provider->model : "<not set>",
            provider->api_key[0] != '\0' ?
                "configured" : "not configured"
        );
    }
}

static void prov_show(const char *name)
{
    const llm_provider *provider;

    provider = prov_find(name);
    if (provider == NULL) {
        (void)puts("AI provider profile not found.");
        return;
    }

    (void)printf("Provider:           %s\n", provider->name);
    (void)printf("Service address:    %s\n", provider->url);
    (void)printf("Model:              %s\n", provider->model);
    (void)printf("Service access key: %s\n",
                 provider->api_key[0] != '\0' ?
                     "******** (configured)" :
                     "<not configured>");
}

static void prov_remove_all(const char *path)
{
    if (path == NULL || *path == '\0') {
        return;
    }

    while (remove(path) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static void prov_probe_cleanup(void)
{
    prov_remove_all(LLM_PROBE_REQUEST_FILE);
    prov_remove_all(LLM_PROBE_HEADERS_FILE);
    prov_remove_all(LLM_PROBE_RESPONSE_FILE);
}

static int prov_probe_request(const llm_provider *provider)
{
    FILE *file;
    int success;

    if (provider == NULL || provider->model[0] == '\0') {
        return 0;
    }

    file = fopen(LLM_PROBE_REQUEST_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    success = 1;
    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, provider->model) ||
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, PROV_PROBE_PROMPT) ||
        fprintf(file,
                "\",\"max_output_tokens\":%d,"
                "\"parallel_tool_calls\":false}\n",
                PROV_PROBE_TOKENS) < 0) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int prov_probe_headers(const llm_provider *provider)
{
    FILE *file;
    int success;

    if (provider == NULL || provider->api_key[0] == '\0') {
        return 0;
    }

    file = fopen(LLM_PROBE_HEADERS_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    success = 1;
    if (fputs("Content-Type: application/json\n", file) == EOF ||
        fputs("Authorization: Bearer ", file) == EOF ||
        fputs(provider->api_key, file) == EOF ||
        fputc('\n', file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int prov_probe_has_text(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    while (*text == ' ' || *text == '\t' ||
           *text == '\r' || *text == '\n') {
        ++text;
    }

    return *text != '\0';
}

static const char *prov_probe_classify_json(const char *json,
                                             int transported,
                                             int *responsive)
{
    char *text;
    const char *message;

    if (responsive != NULL) {
        *responsive = 0;
    }

    if (!transported || json == NULL) {
        return "TRANSPORT";
    }

    text = extract_output_text_from_json(json);
    if (text != NULL) {
        if (prov_probe_has_text(text)) {
            free(text);
            if (responsive != NULL) {
                *responsive = 1;
            }
            return "OK";
        }
        free(text);
    }

    message = find_string_value(json, "message");
    if (message != NULL) {
        return "API ERROR";
    }

    if (llm_response_empty_completed(json, NULL)) {
        return "EMPTY";
    }

    return "INVALID";
}

const char *command_prov_test_json(const char *json,
                                   int transported,
                                   int *responsive)
{
    return prov_probe_classify_json(json, transported, responsive);
}

static const char *prov_probe_classify(int transported,
                                       int *responsive)
{
    char *json;
    const char *status;

    if (responsive != NULL) {
        *responsive = 0;
    }

    if (!transported) {
        return "TRANSPORT";
    }

    json = read_entire_file(LLM_PROBE_RESPONSE_FILE, NULL);
    if (json == NULL) {
        return "TRANSPORT";
    }

    status = prov_probe_classify_json(json, 1, responsive);
    free(json);
    return status;
}

static int prov_test_one(const llm_provider *provider)
{
    char row[512];
    const char *status;
    time_t started;
    time_t finished;
    long elapsed;
    int responsive;
    int transported;

    if (provider == NULL) {
        return 0;
    }

    prov_probe_cleanup();
    responsive = 0;
    transported = 0;
    started = time(NULL);

    if (provider->url[0] == '\0' ||
        provider->model[0] == '\0' ||
        provider->api_key[0] == '\0') {
        status = "CONFIG";
    } else if (!prov_probe_request(provider) ||
               !prov_probe_headers(provider)) {
        status = "LOCAL ERROR";
    } else {
        transported = llm_transport_probe(provider->url);
        status = prov_probe_classify(transported, &responsive);
    }

    finished = time(NULL);
    elapsed = finished >= started ?
        (long)(finished - started) : 0L;

    (void)sprintf(row,
                  "  %s  model=%s  %s  %lds\n",
                  provider->name,
                  provider->model[0] != '\0' ?
                      provider->model : "<not set>",
                  status,
                  elapsed);
    ansi_term_diff(responsive ? ANSI_DIFF_ADD : ANSI_DIFF_DELETE, row);
    prov_probe_cleanup();
    return responsive;
}

static void prov_test(const char *name)
{
    const llm_provider *provider;
    unsigned int count;
    unsigned int index;
    unsigned int tested;
    unsigned int responsive;

    tested = 0U;
    responsive = 0U;

    (void)puts("LLM provider health test");
    (void)puts("------------------------");

    if (name != NULL && *name != '\0') {
        provider = prov_find(name);
        if (provider == NULL) {
            (void)puts("AI provider profile not found.");
            return;
        }
        tested = 1U;
        responsive = prov_test_one(provider) ? 1U : 0U;
    } else {
        count = llm_prov_count();
        if (count == 0U) {
            (void)puts("No AI provider profiles are configured.");
            return;
        }

        for (index = 0U; index < count; ++index) {
            provider = llm_prov_get(index);
            if (provider == NULL) {
                continue;
            }
            ++tested;
            if (prov_test_one(provider)) {
                ++responsive;
            }
        }
    }

    (void)printf(
        "Provider health summary: tested=%u responsive=%u failed=%u\n",
        tested,
        responsive,
        tested - responsive
    );
}

static void prov_add(const char *name)
{
    char url[LLM_PROV_URL_MAX];
    char model[128];
    char key[LLM_PROV_KEY_MAX];

    if (name == NULL || *name == '\0') {
        (void)puts("Usage: PROVIDER ADD name");
        return;
    }

    if (!prov_read_value("Service address: ", url, sizeof(url)) ||
        !prov_read_value("Model: ", model, sizeof(model)) ||
        !secret_read("Service access key: ", key, sizeof(key))) {
        (void)memset(key, 0, sizeof(key));
        (void)puts("Provider configuration cancelled.");
        return;
    }

    if (!llm_prov_add(name, url, model, key)) {
        (void)memset(key, 0, sizeof(key));
        (void)puts("Unable to save AI provider profile.");
        return;
    }

    (void)memset(key, 0, sizeof(key));
    (void)printf("AI provider profile saved: %s\n", name);
}

static void command_provider(agent_state *state,
                             const char *arguments)
{
    char work[256];
    char *cursor;
    char *verb;
    char *name;
    char *extra;

    (void)state;

    if (arguments == NULL || *arguments == '\0') {
        prov_list();
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("Provider command is too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    verb = command_next_argument(&cursor);
    name = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (verb == NULL || extra != NULL) {
        (void)puts(
            "Usage: PROVIDER [LIST|SHOW [name]|TEST [name]|USE name|ADD name|DELETE name]"
        );
        return;
    }

    if (strcmp(verb, "LIST") == 0) {
        if (name != NULL) {
            (void)puts("Usage: PROVIDER LIST");
            return;
        }
        prov_list();
        return;
    }

    if (strcmp(verb, "SHOW") == 0) {
        prov_show(name);
        return;
    }

    if (strcmp(verb, "TEST") == 0) {
        prov_test(name);
        return;
    }

    if (strcmp(verb, "USE") == 0) {
        if (name == NULL || !llm_prov_use(name)) {
            (void)puts("Unable to select AI provider profile.");
            return;
        }
        (void)printf("AI provider selected: %s\n", name);
        return;
    }

    if (strcmp(verb, "ADD") == 0) {
        prov_add(name);
        return;
    }

    if (strcmp(verb, "DELETE") == 0) {
        if (name == NULL || !llm_prov_delete(name)) {
            (void)puts("Unable to delete AI provider profile.");
            return;
        }
        (void)printf("AI provider profile deleted: %s\n", name);
        return;
    }

    (void)puts(
        "Usage: PROVIDER [LIST|SHOW [name]|TEST [name]|USE name|ADD name|DELETE name]"
    );
}

static void command_model(agent_state *state,
                          const char *arguments)
{
    char work[256];
    char *cursor;
    char *model;
    char *extra;
    const char *current;

    (void)state;

    if (arguments == NULL || *arguments == '\0') {
        current = llm_model();
        (void)printf("Current model: %s\n",
                     current != NULL && *current != '\0' ?
                         current : "<not configured>");
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("Model name is too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    model = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (model == NULL || *model == '\0' || extra != NULL) {
        (void)puts("Usage: MODEL [model-name]");
        return;
    }

    if (!llm_set_model(model)) {
        (void)puts("Unable to set model.");
        return;
    }

    (void)printf("Model set to: %s\n", llm_model());
}

static const command_entry provider_commands[] = {
    { "MODEL", "Show or set the active AI provider model", command_model },
    { "PROVIDER", "Manage and test persistent AI provider profiles", command_provider }
};

void command_register_provider(void)
{
    (void)command_registry_add(
        provider_commands,
        sizeof(provider_commands) / sizeof(provider_commands[0])
    );
}
