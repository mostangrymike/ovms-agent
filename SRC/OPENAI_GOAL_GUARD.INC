/*
 * Temporary M267 compatibility shim.
 * Production code must include LLM_GOAL_GUARD.INC directly.
 * Keep legacy text-only test control working until the regression consumer
 * is migrated, then delete this file.
 */
#ifdef OPENAI_GOAL_GUARD_TEXT_ONLY
#ifndef LLM_GOAL_GUARD_TEXT_ONLY
#define LLM_GOAL_GUARD_TEXT_ONLY 1
#define OPENAI_GOAL_GUARD_UNDEF_LLM 1
#endif
#endif

#include "LLM_GOAL_GUARD.INC"

#ifdef OPENAI_GOAL_GUARD_UNDEF_LLM
#undef LLM_GOAL_GUARD_TEXT_ONLY
#undef OPENAI_GOAL_GUARD_UNDEF_LLM
#endif
