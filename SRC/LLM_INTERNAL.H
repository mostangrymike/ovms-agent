/*
 * M267 provider-neutral internal declaration boundary.
 *
 * The mature internal header body is retained byte-for-byte in
 * LLM_INTERNAL_CORE.H.  Ordinary translation units see neutral LLM names
 * for the already-migrated INSTRUCTIONS and GIT_CONTEXT APIs.  Defining
 * wrappers that intentionally pre-map legacy spellings to *_base symbols
 * keep their mappings because each alias below is conditional.
 */
#ifndef LLM_INTERNAL_WRAPPER_H
#define LLM_INTERNAL_WRAPPER_H

#ifndef llm_git_refresh
#define LLM_INT_U_GIT_REFRESH 1
#endif
#ifndef llm_git_status_text
#define LLM_INT_U_GIT_STATUS 1
#endif
#ifndef llm_git_diff_text
#define LLM_INT_U_GIT_DIFF 1
#endif
#ifndef llm_git_changed_text
#define LLM_INT_U_GIT_CHANGED 1
#endif
#ifndef llm_git_context
#define LLM_INT_U_GIT_CONTEXT 1
#endif
#ifndef llm_git_compose
#define LLM_INT_U_GIT_COMPOSE 1
#endif
#ifndef llm_show_git_status
#define LLM_INT_U_SHOW_GIT_STATUS 1
#endif
#ifndef llm_show_git_diff
#define LLM_INT_U_SHOW_GIT_DIFF 1
#endif
#ifndef llm_show_git_changed
#define LLM_INT_U_SHOW_GIT_CHANGED 1
#endif
#ifndef llm_show_git_context
#define LLM_INT_U_SHOW_GIT_CONTEXT 1
#endif
#ifndef llm_git_refresh_cmd
#define LLM_INT_U_GIT_REFRESH_CMD 1
#endif
#ifndef llm_test_git_data
#define LLM_INT_U_TEST_GIT_DATA 1
#endif

#include "LLM_INTERNAL_CORE.H"

int llm_git_rms_copy(const char *path, const char *target);

#ifdef LLM_INT_U_TEST_GIT_DATA
#undef LLM_INT_U_TEST_GIT_DATA
#endif
#ifdef LLM_INT_U_GIT_REFRESH_CMD
#undef LLM_INT_U_GIT_REFRESH_CMD
#endif
#ifdef LLM_INT_U_SHOW_GIT_CONTEXT
#undef LLM_INT_U_SHOW_GIT_CONTEXT
#endif
#ifdef LLM_INT_U_SHOW_GIT_CHANGED
#undef LLM_INT_U_SHOW_GIT_CHANGED
#endif
#ifdef LLM_INT_U_SHOW_GIT_DIFF
#undef LLM_INT_U_SHOW_GIT_DIFF
#endif
#ifdef LLM_INT_U_SHOW_GIT_STATUS
#undef LLM_INT_U_SHOW_GIT_STATUS
#endif
#ifdef LLM_INT_U_GIT_COMPOSE
#undef LLM_INT_U_GIT_COMPOSE
#endif
#ifdef LLM_INT_U_GIT_CONTEXT
#undef LLM_INT_U_GIT_CONTEXT
#endif
#ifdef LLM_INT_U_GIT_CHANGED
#undef LLM_INT_U_GIT_CHANGED
#endif
#ifdef LLM_INT_U_GIT_DIFF
#undef LLM_INT_U_GIT_DIFF
#endif
#ifdef LLM_INT_U_GIT_STATUS
#undef LLM_INT_U_GIT_STATUS
#endif
#ifdef LLM_INT_U_GIT_REFRESH
#undef LLM_INT_U_GIT_REFRESH
#endif

#endif
