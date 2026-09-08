#if defined(LLM_REQUEST_AGENT_H)
#include "LLM_M312_FINAL.INC"
#elif defined(LLM_TOOL_REGISTRY_H)
#include "LLM_M312_SEARCH.INC"
#else
#include "LLM_M312_FINAL.INC"
#include "LLM_M312_SEARCH.INC"
#endif
