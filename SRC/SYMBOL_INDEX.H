#ifndef OVMS_AGENT_SYMBOL_INDEX_H
#define OVMS_AGENT_SYMBOL_INDEX_H

#include "agent.h"

#define SYMBOL_INDEX_FILE "OVMS_AGENT_SYMBOLS.DAT"

void symbol_reindex(agent_state *state);
void symbol_index_status(agent_state *state);
void symbol_index_check(agent_state *state);
void symbol_where(agent_state *state, const char *symbol);
void symbol_callers(agent_state *state, const char *symbol);
void symbol_show(agent_state *state, const char *symbol);
void symbol_show_function(agent_state *state,
                          const char *symbol);
void symbol_show_struct(agent_state *state,
                        const char *symbol);
void symbol_show_module(agent_state *state,
                        const char *module);
void symbol_show_dependencies(agent_state *state,
                              const char *module);
void symbol_who_uses(agent_state *state,
                     const char *symbol);
void symbol_module_graph(agent_state *state,
                         const char *module);
void symbol_module_graph_all(agent_state *state);
void symbol_impact(agent_state *state,
                   const char *symbol);
void symbol_module_reverse(agent_state *state,
                           const char *module);

#endif /* OVMS_AGENT_SYMBOL_INDEX_H */
