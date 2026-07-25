#include <stdio.h>
#include <string.h>

#include "command_internal.h"
#include "symbol_index.h"

static const command_entry symbol_commands[] = {
    { "REINDEX", "Rebuild the persistent C symbol index", command_reindex },
    { "INDEX/STATUS", "Display symbol-index statistics", command_index_status },
    { "INDEX/CHECK", "Check whether the symbol index is current", command_index_check },
    { "WHERE", "Find a C symbol definition: WHERE symbol", command_where },
    { "CALLERS", "Find C call sites: CALLERS symbol", command_callers },
    { "SYMBOL", "Show symbol definitions and calls: SYMBOL symbol", command_symbol },
    { "SHOW/FUNCTION", "Display a complete C function body", command_show_function }
};

void command_register_symbol(void)
{
    (void)command_registry_add(
        symbol_commands,
        sizeof(symbol_commands) / sizeof(symbol_commands[0])
    );
}

static const char *command_symbol_argument(const char *arguments,
                                           const char *usage)
{
    const char *end;

    if (arguments == NULL || *arguments == '\0') {
        (void)puts(usage);
        return NULL;
    }

    end = arguments;

    while (*end != '\0' &&
           *end != ' ' &&
           *end != '\t') {
        ++end;
    }

    if (*end != '\0') {
        (void)puts(usage);
        return NULL;
    }

    return arguments;
}

void command_reindex(agent_state *state, const char *arguments)
{
    if (arguments != NULL && *arguments != '\0') {
        (void)puts("Usage: REINDEX");
        return;
    }

    symbol_reindex(state);
}

void command_index_status(agent_state *state,
                          const char *arguments)
{
    if (arguments != NULL && *arguments != '\0') {
        (void)puts("Usage: INDEX/STATUS");
        return;
    }

    symbol_index_status(state);
}

void command_index_check(agent_state *state,
                         const char *arguments)
{
    if (arguments != NULL && *arguments != '\0') {
        (void)puts("Usage: INDEX/CHECK");
        return;
    }

    symbol_index_check(state);
}

void command_where(agent_state *state, const char *arguments)
{
    const char *symbol;

    symbol = command_symbol_argument(
        arguments,
        "Usage: WHERE symbol"
    );

    if (symbol != NULL) {
        symbol_where(state, symbol);
    }
}

void command_callers(agent_state *state, const char *arguments)
{
    const char *symbol;

    symbol = command_symbol_argument(
        arguments,
        "Usage: CALLERS symbol"
    );

    if (symbol != NULL) {
        symbol_callers(state, symbol);
    }
}

void command_symbol(agent_state *state, const char *arguments)
{
    const char *symbol;

    symbol = command_symbol_argument(
        arguments,
        "Usage: SYMBOL symbol"
    );

    if (symbol != NULL) {
        symbol_show(state, symbol);
    }
}


void command_show_function(agent_state *state,
                           const char *arguments)
{
    const char *symbol;

    symbol = command_symbol_argument(
        arguments,
        "Usage: SHOW/FUNCTION symbol"
    );

    if (symbol != NULL) {
        symbol_show_function(state, symbol);
    }
}
