#include <stdio.h>
#include <string.h>

#include "command_internal.h"
#include "llm_config.h"
#include "secret_input.h"

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
            "Usage: PROVIDER [LIST|SHOW [name]|USE name|ADD name|DELETE name]"
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
        "Usage: PROVIDER [LIST|SHOW [name]|USE name|ADD name|DELETE name]"
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
    { "PROVIDER", "Manage persistent AI provider profiles", command_provider }
};

void command_register_provider(void)
{
    (void)command_registry_add(
        provider_commands,
        sizeof(provider_commands) / sizeof(provider_commands[0])
    );
}
