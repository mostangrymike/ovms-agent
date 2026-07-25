#ifndef OVMS_AGENT_COMMAND_INTERNAL_H
#define OVMS_AGENT_COMMAND_INTERNAL_H

#include <stddef.h>

#include "agent.h"

typedef void (*command_handler)(agent_state *state,
                                const char *arguments);

typedef struct command_entry {
    const char *name;
    const char *description;
    command_handler handler;
} command_entry;

int command_registry_add(const command_entry *entries,
                         size_t entry_count);
void command_registry_initialize(void);
const command_entry *command_find(const char *name);
const command_entry *command_registry_get(size_t index);
size_t command_registry_count(void);

void command_register_core(void);
void command_register_project(void);
void command_register_agent(void);
void command_register_symbol(void);

char *command_next_argument(char **cursor);
int command_decode_escapes(char *text);

void command_help(agent_state *state, const char *arguments);
void command_version(agent_state *state, const char *arguments);
void command_quit(agent_state *state, const char *arguments);

void command_root(agent_state *state, const char *arguments);
void command_status(agent_state *state, const char *arguments);
void command_list(agent_state *state, const char *arguments);
void command_tree(agent_state *state, const char *arguments);
void command_read(agent_state *state, const char *arguments);
void command_search(agent_state *state, const char *arguments);
void command_edit(agent_state *state, const char *arguments);
void command_patch(agent_state *state, const char *arguments);
void command_build(agent_state *state, const char *arguments);
void command_gitstatus(agent_state *state, const char *arguments);
void command_gitdiff(agent_state *state, const char *arguments);

void command_reindex(agent_state *state, const char *arguments);
void command_index_status(agent_state *state,
                          const char *arguments);
void command_index_check(agent_state *state,
                         const char *arguments);
void command_where(agent_state *state, const char *arguments);
void command_callers(agent_state *state, const char *arguments);
void command_symbol(agent_state *state, const char *arguments);
void command_show_function(agent_state *state,
                           const char *arguments);

void command_ask(agent_state *state, const char *arguments);
void command_chat(agent_state *state, const char *arguments);
void command_chat_reset(agent_state *state, const char *arguments);
void command_review(agent_state *state, const char *arguments);
void command_agent(agent_state *state, const char *arguments);
void command_agent_write(agent_state *state, const char *arguments);
void command_agent_build(agent_state *state, const char *arguments);
void command_agent_fix(agent_state *state, const char *arguments);
void command_agent_retry(agent_state *state, const char *arguments);
void command_agent_selftest(agent_state *state, const char *arguments);
void command_agent_status(agent_state *state, const char *arguments);
void command_agent_verify(agent_state *state, const char *arguments);
void command_agent_log(agent_state *state, const char *arguments);
void command_agent_log_old(agent_state *state, const char *arguments);
void command_agent_log_clear(agent_state *state, const char *arguments);
void command_agent_metrics(agent_state *state, const char *arguments);
void command_agent_state(agent_state *state, const char *arguments);
void command_agent_state_clear(agent_state *state, const char *arguments);
void command_agent_create(agent_state *state, const char *arguments);
void command_agent_plan(agent_state *state, const char *arguments);
void command_agent_plan_show(agent_state *state, const char *arguments);
void command_agent_plan_clear(agent_state *state, const char *arguments);
void command_agent_plan_validate(agent_state *state,
                                 const char *arguments);
void command_agent_execute(agent_state *state, const char *arguments);

#endif /* OVMS_AGENT_COMMAND_INTERNAL_H */
