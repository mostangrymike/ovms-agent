/* M267 neutral ABI entry for the mature retry implementation. */
#define openai_agent_retry llm_agent_retry
#define openai_agent_mode llm_agent_mode
#include "LLM_RETRY_CORE.C"
#undef openai_agent_mode
#undef openai_agent_retry
