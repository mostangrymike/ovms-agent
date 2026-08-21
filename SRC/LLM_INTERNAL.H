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

#ifndef openai_instr_reload
#define openai_instr_reload llm_instr_reload
#define LLM_INT_U_INSTR_RELOAD 1
#endif
#ifndef openai_instr_compose
#define openai_instr_compose llm_instr_compose
#define LLM_INT_U_INSTR_COMPOSE 1
#endif
#ifndef openai_instr_status_text
#define openai_instr_status_text llm_instr_status_text
#define LLM_INT_U_INSTR_STATUS 1
#endif
#ifndef openai_instr_show_text
#define openai_instr_show_text llm_instr_show_text
#define LLM_INT_U_INSTR_SHOW 1
#endif
#ifndef openai_show_instr_status
#define openai_show_instr_status llm_show_instr_status
#define LLM_INT_U_SHOW_INSTR_STATUS 1
#endif
#ifndef openai_show_instr
#define openai_show_instr llm_show_instr
#define LLM_INT_U_SHOW_INSTR 1
#endif
#ifndef openai_instr_reload_cmd
#define openai_instr_reload_cmd llm_instr_reload_cmd
#define LLM_INT_U_INSTR_RELOAD_CMD 1
#endif
#ifndef openai_test_instr_path
#define openai_test_instr_path llm_test_instr_path
#define LLM_INT_U_TEST_INSTR_PATH 1
#endif

#ifndef openai_git_refresh
#define openai_git_refresh llm_git_refresh
#define LLM_INT_U_GIT_REFRESH 1
#endif
#ifndef openai_git_status_text
#define openai_git_status_text llm_git_status_text
#define LLM_INT_U_GIT_STATUS 1
#endif
#ifndef openai_git_diff_text
#define openai_git_diff_text llm_git_diff_text
#define LLM_INT_U_GIT_DIFF 1
#endif
#ifndef openai_git_changed_text
#define openai_git_changed_text llm_git_changed_text
#define LLM_INT_U_GIT_CHANGED 1
#endif
#ifndef openai_git_context
#define openai_git_context llm_git_context
#define LLM_INT_U_GIT_CONTEXT 1
#endif
#ifndef openai_git_compose
#define openai_git_compose llm_git_compose
#define LLM_INT_U_GIT_COMPOSE 1
#endif
#ifndef openai_show_git_status
#define openai_show_git_status llm_show_git_status
#define LLM_INT_U_SHOW_GIT_STATUS 1
#endif
#ifndef openai_show_git_diff
#define openai_show_git_diff llm_show_git_diff
#define LLM_INT_U_SHOW_GIT_DIFF 1
#endif
#ifndef openai_show_git_changed
#define openai_show_git_changed llm_show_git_changed
#define LLM_INT_U_SHOW_GIT_CHANGED 1
#endif
#ifndef openai_show_git_context
#define openai_show_git_context llm_show_git_context
#define LLM_INT_U_SHOW_GIT_CONTEXT 1
#endif
#ifndef openai_git_refresh_cmd
#define openai_git_refresh_cmd llm_git_refresh_cmd
#define LLM_INT_U_GIT_REFRESH_CMD 1
#endif
#ifndef openai_test_git_data
#define openai_test_git_data llm_test_git_data
#define LLM_INT_U_TEST_GIT_DATA 1
#endif

#include "LLM_INTERNAL_CORE.H"

int llm_git_rms_copy(const char *path, const char *target);

#ifdef LLM_INT_U_TEST_GIT_DATA
#undef openai_test_git_data
#undef LLM_INT_U_TEST_GIT_DATA
#endif
#ifdef LLM_INT_U_GIT_REFRESH_CMD
#undef openai_git_refresh_cmd
#undef LLM_INT_U_GIT_REFRESH_CMD
#endif
#ifdef LLM_INT_U_SHOW_GIT_CONTEXT
#undef openai_show_git_context
#undef LLM_INT_U_SHOW_GIT_CONTEXT
#endif
#ifdef LLM_INT_U_SHOW_GIT_CHANGED
#undef openai_show_git_changed
#undef LLM_INT_U_SHOW_GIT_CHANGED
#endif
#ifdef LLM_INT_U_SHOW_GIT_DIFF
#undef openai_show_git_diff
#undef LLM_INT_U_SHOW_GIT_DIFF
#endif
#ifdef LLM_INT_U_SHOW_GIT_STATUS
#undef openai_show_git_status
#undef LLM_INT_U_SHOW_GIT_STATUS
#endif
#ifdef LLM_INT_U_GIT_COMPOSE
#undef openai_git_compose
#undef LLM_INT_U_GIT_COMPOSE
#endif
#ifdef LLM_INT_U_GIT_CONTEXT
#undef openai_git_context
#undef LLM_INT_U_GIT_CONTEXT
#endif
#ifdef LLM_INT_U_GIT_CHANGED
#undef openai_git_changed_text
#undef LLM_INT_U_GIT_CHANGED
#endif
#ifdef LLM_INT_U_GIT_DIFF
#undef openai_git_diff_text
#undef LLM_INT_U_GIT_DIFF
#endif
#ifdef LLM_INT_U_GIT_STATUS
#undef openai_git_status_text
#undef LLM_INT_U_GIT_STATUS
#endif
#ifdef LLM_INT_U_GIT_REFRESH
#undef openai_git_refresh
#undef LLM_INT_U_GIT_REFRESH
#endif

#ifdef LLM_INT_U_TEST_INSTR_PATH
#undef openai_test_instr_path
#undef LLM_INT_U_TEST_INSTR_PATH
#endif
#ifdef LLM_INT_U_INSTR_RELOAD_CMD
#undef openai_instr_reload_cmd
#undef LLM_INT_U_INSTR_RELOAD_CMD
#endif
#ifdef LLM_INT_U_SHOW_INSTR
#undef openai_show_instr
#undef LLM_INT_U_SHOW_INSTR
#endif
#ifdef LLM_INT_U_SHOW_INSTR_STATUS
#undef openai_show_instr_status
#undef LLM_INT_U_SHOW_INSTR_STATUS
#endif
#ifdef LLM_INT_U_INSTR_SHOW
#undef openai_instr_show_text
#undef LLM_INT_U_INSTR_SHOW
#endif
#ifdef LLM_INT_U_INSTR_STATUS
#undef openai_instr_status_text
#undef LLM_INT_U_INSTR_STATUS
#endif
#ifdef LLM_INT_U_INSTR_COMPOSE
#undef openai_instr_compose
#undef LLM_INT_U_INSTR_COMPOSE
#endif
#ifdef LLM_INT_U_INSTR_RELOAD
#undef openai_instr_reload
#undef LLM_INT_U_INSTR_RELOAD
#endif

#endif
