#include <stdlib.h>
#include <string.h>

#include "llm_config.h"
#include "llm_internal.h"
#include "LLM_AUTO.H"
#include "LLM_USAGE.H"
#include "ANSI_TERM.H"

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

#define getenv m262_provider_env
#define llm_auto_note_turn() m273_note_turn(turn + 1U, turn_limit)
#define perform_openai_request() m273_provider_request()
#include "LLM_AGENT_LOOP_M259_CORE.C"
#undef perform_openai_request
#undef llm_auto_note_turn
#undef getenv
