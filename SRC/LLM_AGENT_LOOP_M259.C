#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_config.h"
#include "llm_internal.h"
#include "LLM_AUTO.H"
#include "LLM_USAGE.H"
#include "ANSI_TERM.H"
#include "LLM_TOOL_REGISTRY.H"
#include "M289_NATIVE_BUILD.H"

static unsigned long m289_loop_build_status = 0UL;

static void m273_note_turn(unsigned int turn, unsigned int limit)
{
    char status[160];

    llm_auto_note_turn();
    if (llm_usage_status(status, sizeof(status), turn, limit)) {
        ansi_term_status(status);
    } else {
        ansi_term_status_turn(turn, limit);
    }
}

static int m273_provider_request(void)
{
    int result;

    result = perform_openai_request();
    ansi_term_status_clear();
    return result;
}

static char *m262_provider_env(const char *name)
{
    const char *value;

    if (name != NULL && strcmp(name, "OPENAI_API_KEY") == 0) {
        value = llm_api_key();
        return (char *)value;
    }

    if (name != NULL && strcmp(name, "OVMS_AGENT_MODEL") == 0) {
        value = llm_model();
        return (char *)value;
    }

    return getenv(name);
}

static int m289_loop_is_read(const llm_tool_descriptor *descriptor,
                             int allow_write)
{
    if (descriptor != NULL &&
        descriptor->kind == LLM_TOOL_BUILD_SOURCE) {
        return allow_write;
    }

    return llm_tool_is_read(descriptor);
}

static char *m289_loop_execute_read(
    agent_state *state,
    const llm_tool_descriptor *descriptor,
    const char *arguments,
    llm_file_cache_entry *cache)
{
    char *source;
    char *output;

    if (descriptor == NULL ||
        descriptor->kind != LLM_TOOL_BUILD_SOURCE) {
        return llm_tool_execute_read(descriptor, arguments, cache);
    }

    source = extract_string_argument(arguments, "source");
    m289_loop_build_status = 0UL;

    if (source == NULL || *source == '\0') {
        free(source);
        return make_tool_error(
            "build_source requires a project-relative source path",
            NULL
        );
    }

    output = m289_build_source(state, source, &m289_loop_build_status);

    (void)printf(
        "Tool executed: build_source %s [%s, status %%X%08lX]\n",
        source,
        (m289_loop_build_status & 1UL) != 0UL ? "success" : "failure",
        m289_loop_build_status
    );

    free(source);
    return output;
}

static char *m289_loop_result_make(
    const char *name,
    const char *status,
    const char *effect,
    int success,
    const char *arguments,
    const char *content)
{
    int native_ok;

    if (name != NULL && strcmp(name, "build_source") == 0) {
        native_ok = (m289_loop_build_status & 1UL) != 0UL;
        return llm_result_make(
            name,
            native_ok ? "success" : "failure",
            "execute",
            native_ok,
            arguments,
            content
        );
    }

    return llm_result_make(
        name, status, effect, success, arguments, content
    );
}

static void m289_loop_tx_result(const char *name,
                                const char *status,
                                const char *content)
{
    if (name != NULL && strcmp(name, "build_source") == 0) {
        llm_tx_model_result(
            name,
            (m289_loop_build_status & 1UL) != 0UL ?
                "success" : "failure",
            content
        );
        return;
    }

    llm_tx_model_result(name, status, content);
}

#define getenv m262_provider_env
#define llm_auto_note_turn() m273_note_turn(turn + 1U, turn_limit)
#define perform_openai_request() m273_provider_request()
#define llm_tool_is_read(descriptor) \
    m289_loop_is_read((descriptor), allow_write)
#define llm_tool_execute_read(descriptor, arguments, cache) \
    m289_loop_execute_read(state, (descriptor), (arguments), (cache))
#define llm_result_make(a,b,c,d,e,f) \
    m289_loop_result_make((a),(b),(c),(d),(e),(f))
#define llm_tx_model_result(a,b,c) \
    m289_loop_tx_result((a),(b),(c))
#include "LLM_AGENT_LOOP_M259_CORE.C"
#undef llm_tx_model_result
#undef llm_result_make
#undef llm_tool_execute_read
#undef llm_tool_is_read
#undef perform_openai_request
#undef llm_auto_note_turn
#undef getenv
