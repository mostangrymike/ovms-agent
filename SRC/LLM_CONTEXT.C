#include "LLM_CONTEXT.H"

#define openai_context_append llm_context_append
#define openai_context_tail llm_context_tail
#define openai_context_evidence_text llm_context_evidence_text
#define openai_context_build llm_context_build
#define openai_context_current llm_context_current
#define openai_show_context_current llm_show_context_current

#include "LLM_CONTEXT_CORE.INC"

#undef openai_show_context_current
#undef openai_context_current
#undef openai_context_build
#undef openai_context_evidence_text
#undef openai_context_tail
#undef openai_context_append

/*
 * M267 transition wrappers.  Keep existing broad consumers linkable until
 * their large shared files can be migrated with a surgical edit path.
 */
int openai_context_evidence_text(const char *session,
                                 char *output,
                                 size_t output_size)
{
    return llm_context_evidence_text(session, output, output_size);
}

int openai_context_build(const char *goal,
                         char *output,
                         size_t output_size)
{
    return llm_context_build(goal, output, output_size);
}

int openai_context_current(char *output,
                           size_t output_size)
{
    return llm_context_current(output, output_size);
}

void openai_show_context_current(void)
{
    llm_show_context_current();
}
