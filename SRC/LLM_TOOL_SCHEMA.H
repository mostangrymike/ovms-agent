#ifndef LLM_TOOL_SCHEMA_H
#define LLM_TOOL_SCHEMA_H

#include <stdio.h>

int write_agent_tools(FILE *file);
int write_agent_tools_atomic(FILE *file);
int write_agent_tools_with_replace(FILE *file);
int write_agent_tools_with_create(FILE *file);
int write_build_agent_tools(FILE *file);

#endif /* LLM_TOOL_SCHEMA_H */
