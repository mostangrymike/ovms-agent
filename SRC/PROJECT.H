#ifndef OVMS_AGENT_PROJECT_H
#define OVMS_AGENT_PROJECT_H

#include "agent.h"

void project_show_root(const agent_state *state);
void project_show_status(const agent_state *state);
void project_list(const agent_state *state, int show_all);
void project_read(const agent_state *state, const char *path);
void project_git_status(const agent_state *state);
void project_git_diff(const agent_state *state);
void project_build(const agent_state *state);
void project_search(const agent_state *state,
                    const char *path,
                    const char *pattern);
#endif
int project_patch(const agent_state *state,
                  const char *path,
                  const char *old_text,
                  const char *new_text);
