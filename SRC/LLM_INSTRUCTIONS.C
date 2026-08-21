#include "LLM_INSTRUCTIONS.H"

/* M257 build entry: wrapper on first include, preserved M232 base on recursion. */
#ifndef LLM_M257_INSTR_ENTRY
#define LLM_M257_INSTR_ENTRY
#include "LLM_INSTRUCTIONS_M257.C"

int llm_instr_reload(const agent_state *state)
{
    return openai_instr_reload(state);
}

int llm_instr_compose(const agent_state *state,
                      const char *goal,
                      char *output,
                      size_t output_size)
{
    return openai_instr_compose(state, goal, output, output_size);
}

int llm_instr_status_text(const agent_state *state,
                          char *output,
                          size_t output_size)
{
    return openai_instr_status_text(state, output, output_size);
}

int llm_instr_show_text(const agent_state *state,
                        char *output,
                        size_t output_size)
{
    return openai_instr_show_text(state, output, output_size);
}

void llm_show_instr_status(const agent_state *state)
{
    openai_show_instr_status(state);
}

void llm_show_instr(const agent_state *state)
{
    openai_show_instr(state);
}

void llm_instr_reload_cmd(const agent_state *state)
{
    openai_instr_reload_cmd(state);
}

void llm_test_instr_path(const char *path)
{
    openai_test_instr_path(path);
}
#else
#include "LLM_INSTRUCTIONS_BASE.C"
#endif
