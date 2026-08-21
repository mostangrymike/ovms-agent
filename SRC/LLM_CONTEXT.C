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

/* Temporary M267 compatibility bridge for the remaining parity-core call. */
int openai_context_build(const char *goal,
                         char *output,
                         size_t output_size)
{
    return llm_context_build(goal, output, output_size);
}
