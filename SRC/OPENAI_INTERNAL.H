#ifndef OPENAI_INTERNAL_H
#define OPENAI_INTERNAL_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openai.h"
#include "openai_log.h"
#include "openai_path.h"
#include "openai_cache.h"
#include "openai_tool_schema.h"
#include "openai_plan.h"
#include "openai_execute.h"
#include "openai_state.h"
#include "project.h"

#define OPENAI_REQUEST_FILE  "OVMS_AGENT_REQUEST.JSON"
#define OPENAI_RESPONSE_FILE "OVMS_AGENT_RESPONSE.JSON"
#define OPENAI_HEADERS_FILE  "OVMS_AGENT_HEADERS.TXT"
#define OPENAI_BUILD_LOG_FILE "OVMS_AGENT_BUILD.LOG"
#define OPENAI_STATE_FILE "OVMS_AGENT.STATE"
#define OPENAI_BUILD_OUTPUT_LIMIT 65536
#define OPENAI_RESPONSE_ID_SIZE 128

#define OPENAI_WORKFLOW_NONE       0
#define OPENAI_WORKFLOW_ASK        1
#define OPENAI_WORKFLOW_CHAT       2
#define OPENAI_WORKFLOW_REVIEW     3
#define OPENAI_WORKFLOW_AGENT      4
#define OPENAI_WORKFLOW_WRITE      5
#define OPENAI_WORKFLOW_FIX        6
#define OPENAI_WORKFLOW_BUILD      7
#define OPENAI_WORKFLOW_RETRY      8
#define OPENAI_WORKFLOW_SELFTEST   9
#define OPENAI_WORKFLOW_VERIFY    10
#define OPENAI_WORKFLOW_CREATE    11
#define OPENAI_WORKFLOW_PLAN      12

#define OPENAI_ROLLBACK_NONE        0
#define OPENAI_ROLLBACK_NOT_NEEDED  1
#define OPENAI_ROLLBACK_SUCCEEDED   2
#define OPENAI_ROLLBACK_FAILED      3
#define OPENAI_ROLLBACK_DECLINED    4

#define OPENAI_AGENT_MAX_TURNS 12
#define OPENAI_PLAN_MAX_TURNS 24
#define OPENAI_SEARCH_OUTPUT_LIMIT 32768
#define OPENAI_LIST_OUTPUT_LIMIT 32768
#define OPENAI_CREATE_MAX_BYTES 65536U
#define OPENAI_RANGE_OUTPUT_LIMIT 65536U

typedef enum openai_replace_result {
    OPENAI_REPLACE_ERROR = 0,
    OPENAI_REPLACE_APPLIED = 1,
    OPENAI_REPLACE_DECLINED = 2
} openai_replace_result;

typedef enum openai_create_result {
    OPENAI_CREATE_ERROR = 0,
    OPENAI_CREATE_CREATED = 1,
    OPENAI_CREATE_DECLINED = 2
} openai_create_result;

extern char previous_response_id[OPENAI_RESPONSE_ID_SIZE];

extern int openai_last_workflow;
extern int openai_last_build_known;
extern int openai_last_build_status;
extern int openai_last_rollback;
extern int openai_state_loaded;
extern int openai_state_valid;

void openai_save_state(void);
void openai_state_save(void);
void openai_load_state(void);
void openai_show_state(void);
void openai_clear_state(void);
int json_write_escaped(FILE *file, const char *text);
int write_request(const char *model,
                         const char *prompt,
                         const char *previous_id);
int write_headers(const char *api_key);
void remove_temporary_files(void);
char *read_entire_file(const char *path, size_t *length_out);
const char *skip_space(const char *position);
int hex_value(int ch);
int append_utf8(char **destination,
                       size_t *remaining,
                       unsigned int codepoint);
char *json_decode_string(const char *start,
                                const char **after);
int json_key_matches(const char *position,
                            const char *key,
                            const char **value_start);
const char *find_string_value(const char *start,
                                     const char *key);
const char *find_output_text_object(const char *json);
int save_response_id(const char *json);
int display_clean_response(void);
void display_raw_response(void);
void openai_send(agent_state *state,
                        const char *prompt,
                        int continue_conversation);
int openai_contains_ignore_case(const char *text,
                                       const char *pattern);
char *openai_read_text_file(const char *path);
int write_create_agent_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output);
int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt);
int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *context_items,
    const char *call_id,
    const char *tool_output);
int write_agent_request_mode(const char *model,
                                    const char *instructions,
                                    const char *user_prompt,
                                    const char *previous_id,
                                    const char *call_id,
                                    const char *tool_output,
                                    int allow_write);
int write_agent_request(const char *model,
                               const char *instructions,
                               const char *user_prompt,
                               const char *previous_id,
                               const char *call_id,
                               const char *tool_output);
int perform_openai_request(void);
char *extract_top_level_id(const char *json);
const char *find_function_call_object(const char *json);
int extract_function_call(const char *json,
                                 char **name,
                                 char **call_id,
                                 char **arguments);
char *extract_path_argument(const char *arguments);
char *make_tool_error(const char *message,
                             const char *path);
char *extract_string_argument(const char *arguments,
                                     const char *name);
const char *openai_cache_lookup(
    const openai_file_cache_entry *cache,
    const char *path);
char *openai_duplicate_text(const char *text);
int extract_integer_argument(const char *arguments,
                                    const char *name,
                                    long *value_out);
char *execute_read_file_range_tool(
    const char *arguments,
    char **display_path,
    long *display_start,
    long *display_end);
char *execute_read_file_tool(
    const char *arguments,
    openai_file_cache_entry *cache,
    int *cache_hit,
    char **display_path);
int openai_join_path(const char *parent,
                            const char *child,
                            char *output,
                            size_t output_size);
char *execute_list_directory_tool(const char *arguments,
                                         char **display_path);
char *execute_search_file_tool(const char *arguments,
                                      char **display_path,
                                      char **display_pattern);
int display_api_error_from_json(const char *json);
openai_create_result execute_create_file_tool(
    const char *arguments,
    char **display_path);
openai_replace_result execute_replace_text_tool(
    agent_state *state,
    const char *arguments,
    char **display_path);
openai_replace_result execute_replace_lines_tool(
    agent_state *state,
    const char *arguments,
    char **display_path);
char *extract_output_items_json(const char *json);
char *execute_run_build_tool(int *build_status);
char *extract_output_text_from_json(const char *json);
int display_output_text_from_json(const char *json);
int openai_confirm_restore(const char *path);
int openai_restore_previous_version(const char *path);
void openai_agent_mode(agent_state *state,
                       const char *goal,
                       int allow_write,
                       int build_after_write,
                       int workflow);
void openai_agent(agent_state *state, const char *goal);
void openai_agent_plan(agent_state *state, const char *goal);
void openai_agent_write(agent_state *state, const char *goal);
void openai_agent_fix(agent_state *state, const char *goal);
void openai_agent_create(agent_state *state, const char *goal);
void openai_agent_build(agent_state *state, const char *goal);
void openai_agent_retry(agent_state *state, const char *goal);
const char *openai_workflow_name(int workflow);
const char *openai_rollback_name(int rollback_state);
void openai_status(const agent_state *state);
void openai_selftest_report(const char *name,
                                   int passed,
                                   unsigned int *passed_count,
                                   unsigned int *failed_count);
unsigned int openai_run_selftest(agent_state *state);
void openai_selftest(agent_state *state);
void openai_verify(agent_state *state);
void openai_review_file(agent_state *state, const char *path);
void openai_ask(agent_state *state, const char *prompt);
void openai_chat(agent_state *state, const char *prompt);
void openai_chat_reset(void);
int openai_chat_active(void);

#endif /* OPENAI_INTERNAL_H */
