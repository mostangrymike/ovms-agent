#include "llm_internal.h"

/* M257 build entry: wrapper on first include, preserved M232 base on recursion. */
#ifndef LLM_M257_INSTR_ENTRY
#define LLM_M257_INSTR_ENTRY
#include "LLM_INSTRUCTIONS_M257.C"
#else
#include "LLM_INSTRUCTIONS_BASE.C"
#endif
