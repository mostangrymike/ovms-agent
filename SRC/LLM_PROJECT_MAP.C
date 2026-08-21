/*
 * M267 provider-neutral PROJECT_MAP transition wrapper.
 *
 * The implementation body is retained verbatim in LLM_PROJECT_MAP_CORE.INC
 * while the compiled public namespace is neutralized here.
 */
#define openai_project_refresh       llm_project_refresh
#define openai_project_map_text      llm_project_map_text
#define openai_project_files_text    llm_project_files_text
#define openai_project_src_text      llm_project_src_text
#define openai_project_tests_text    llm_project_tests_text
#define openai_project_build_text    llm_project_build_text
#define openai_project_context       llm_project_context
#define openai_project_compose       llm_project_compose
#define openai_show_project_map      llm_show_project_map
#define openai_show_project_ctx      llm_show_project_ctx
#define openai_project_refresh_cmd   llm_project_refresh_cmd

#include "LLM_PROJECT_MAP_CORE.INC"

#undef openai_project_refresh
#undef openai_project_map_text
#undef openai_project_files_text
#undef openai_project_src_text
#undef openai_project_tests_text
#undef openai_project_build_text
#undef openai_project_context
#undef openai_project_compose
#undef openai_show_project_map
#undef openai_show_project_ctx
#undef openai_project_refresh_cmd
