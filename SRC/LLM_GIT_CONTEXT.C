#include "LLM_GIT_CONTEXT.H"
#include "LLM_GIT_CONTEXT_CORE.C"

int llm_git_refresh(const agent_state *state)
{
    return openai_git_refresh(state);
}

int llm_git_status_text(const agent_state *state,
                        char *output,
                        size_t output_size)
{
    return openai_git_status_text(state, output, output_size);
}

int llm_git_diff_text(const agent_state *state,
                      char *output,
                      size_t output_size)
{
    return openai_git_diff_text(state, output, output_size);
}

int llm_git_changed_text(const agent_state *state,
                         char *output,
                         size_t output_size)
{
    return openai_git_changed_text(state, output, output_size);
}

int llm_git_context(const agent_state *state,
                    char *output,
                    size_t output_size)
{
    return openai_git_context(state, output, output_size);
}

int llm_git_compose(const agent_state *state,
                    const char *goal,
                    char *output,
                    size_t output_size)
{
    return openai_git_compose(state, goal, output, output_size);
}

void llm_show_git_status(const agent_state *state)
{
    openai_show_git_status(state);
}

void llm_show_git_diff(const agent_state *state)
{
    openai_show_git_diff(state);
}

void llm_show_git_changed(const agent_state *state)
{
    openai_show_git_changed(state);
}

void llm_show_git_context(const agent_state *state)
{
    openai_show_git_context(state);
}

void llm_git_refresh_cmd(const agent_state *state)
{
    openai_git_refresh_cmd(state);
}

int llm_git_rms_copy(const char *path, const char *target)
{
    return openai_git_rms_copy(path, target);
}

void llm_test_git_data(const char *status_text,
                       const char *diff_text)
{
    openai_test_git_data(status_text, diff_text);
}
