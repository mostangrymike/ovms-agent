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
#include "openai_tool_registry.h"
#include "openai_response.h"
#include "openai_json_parse.h"
#include "openai_json_write.h"
#include "openai_transport.h"
#include "openai_request_build.h"
#include "openai_request_agent.h"
#include "openai_request_basic.h"
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
void openai_send(agent_state *state,
                        const char *prompt,
                        int continue_conversation);
int openai_contains_ignore_case(const char *text,
                                       const char *pattern);
char *openai_read_text_file(const char *path);
char *make_tool_error(const char *message,
                             const char *path);
const char *openai_cache_lookup(
    const openai_file_cache_entry *cache,
    const char *path);
char *openai_duplicate_text(const char *text);
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
char *execute_run_build_tool(int *build_status);
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
