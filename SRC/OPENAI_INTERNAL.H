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

typedef char *(*openai_test_build_hook_fn)(int *build_status);

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
#define OPENAI_WORKFLOW_EXECUTE   13

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
extern int openai_saved_workflow;
extern int openai_saved_build_known;
extern int openai_saved_build_status;
extern int openai_saved_rollback;
extern int openai_state_loaded;
extern int openai_state_valid;
extern int openai_state_recovered;
extern int openai_state_save_known;
extern int openai_state_save_succeeded;
const char *openai_state_source_name(int valid, int recovered);
const char *openai_state_save_name(int known, int succeeded);
const char *openai_persist_workflow(int valid, int workflow);
const char *openai_persist_build(
    int valid, int build_known, int build_status);
const char *openai_persist_rollback(int valid, int rollback);

void openai_save_state(void); void openai_state_save(void);
void openai_load_state(void);
void openai_show_state(void);
void openai_show_memory(void);
void openai_clear_state(void);
void openai_log_repair_attempt(unsigned int attempt,
                               unsigned long plan_hash,
                               int build_status,
                               int rollback,
                               const char *outcome);
int openai_last_repair_record(char *output,
                              size_t output_size);
int openai_repair_status_text(char *output,
                              size_t output_size);
void openai_show_repair_status(void);
int openai_repair_history_text(char *output,
                               size_t output_size);
void openai_show_repair_history(void);
int openai_repair_failures_text(char *output,
                                size_t output_size);
void openai_show_repair_failures(void);
int openai_repair_show_text(unsigned long plan_hash,
                            char *output,
                            size_t output_size);
void openai_show_repair_plan(const char *arguments);
int openai_repair_export_file(const char *path,
                              int allow_overwrite);
void openai_export_repair_history(const char *arguments);
void openai_test_set_log_path(const char *path);
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
void openai_plan_execute_dry(agent_state *state);
void openai_plan_execute(agent_state *state);
int openai_exec_validate_ops_file(const char *path);
int openai_m150_create_valid(void);
int openai_m150_create_old_reject(void);
int openai_m150_create_missing_new(void);
int openai_m150b_create_commit(void);
int openai_m150b_create_rollback(void);
int openai_m150b_existing_reject(void);
int openai_m150b_mixed_rollback(void);
int openai_m150b_same_path_reject(void);
int openai_m150c_has_create(void);
int openai_m150c_guards_target(void);
int openai_m150c_keeps_replace(void);
int openai_m150c_blocks_legacy(void);
int openai_m150d_success(void);
int openai_m150d_rollback(void);
int openai_m150d_tamper(void);
int openai_exec_stage_ops_file(const char *path, const char *expected_text);
int openai_exec_dry_validate(const char *operations_path, const char *target_path, const char *expected_original_text);
int openai_exec_stage_expect_fail(const char *operations_path, const char *target_path, const char *expected_original_text);
int openai_exec_rollback_validate(const char *operations_path, const char *target_path, const char *expected_original_text, int simulated_build_status);
int openai_m107_build_evidence(const char *path);
int openai_m107_ops_evidence(const char *path);
int openai_m107_state_serialize(const char *path);
int openai_m107_state_reload(const char *path);
int openai_m109_state_writer(const char *path);
int openai_m109_state_reader(const char *path);
int openai_m109_invalid_workflow(const char *path);
int openai_m109_incomplete_state(const char *path);
int openai_m110_dup_workflow(const char *path);
int openai_m110_dup_rollback(const char *path);
int openai_m110_unknown_field(const char *path);
int openai_m110_value_suffix(const char *path);
int openai_m111_workflow_first(const char *path);
int openai_m111_status_before_known(const char *path);
int openai_m111_rollback_first(const char *path);
int openai_m111_canonical_order(const char *path);
int openai_m112_big_workflow(const char *path);
int openai_m112_big_unknown(const char *path);
int openai_m112_unterm_middle(const char *path);
int openai_m112_unterm_final(const char *path);
int openai_m113_signed_workflow(const char *path);
int openai_m113_zero_workflow(const char *path);
int openai_m113_spaced_value(const char *path);
int openai_m113_zero_values(const char *path);
int openai_m114_unknown_nonzero(const char *path);
int openai_m114_unknown_zero(const char *path);
int openai_m114_known_success(const char *path);
int openai_m114_known_failure(const char *path);
int openai_m115_bad_workflow(const char *path);
int openai_m115_bad_build_flag(const char *path);
int openai_m115_bad_build_state(const char *path);
int openai_m115_bad_rollback(const char *path);
int openai_m116_atomic_create(const char *path);
int openai_m116_tmp_fail_keeps(const char *path);
int openai_m116_bad_values_keep(const char *path);
int openai_m116_tmp_versions_clean(const char *path);
int openai_m117_tmp_is_unique(const char *path);
int openai_m117_tmp_truncation(const char *path);
int openai_m117_tmp_no_collision(const char *path);
int openai_m117_atomic_valid(const char *path);
int openai_m118_purge_versions(const char *path);
int openai_m118_purge_missing(const char *path);
int openai_m118_purge_empty(void);
int openai_m118_state_stays_clear(const char *path);
int openai_m119_writer_negative(const char *path);
int openai_m119_loader_negative(const char *path);
int openai_m119_known_zero(const char *path);
int openai_m119_writer_reloads(const char *path);
int openai_m120_recovery_version(const char *path);
int openai_m120_versions_bounded(const char *path);
int openai_m120_newest_preserved(const char *path);
int openai_m120_unsafe_rejected(void);
int openai_m121_success_flags(void);
int openai_m121_failure_valid(void);
int openai_m121_failure_loaded(void);
int openai_m121_disk_preserved(const char *path);
int openai_m122_zero_is_newest(const char *path);
int openai_m122_minus_one_prior(const char *path);
int openai_m122_missing_prior_fails(const char *path);
int openai_m122_probe_read_only(const char *path);
int openai_m123_newest_preferred(const char *path);
int openai_m123_fallback_prior(const char *path);
int openai_m123_both_invalid(const char *path);
int openai_m123_read_only(const char *path);
int openai_m124_normal_load(const char *path);
int openai_m124_fallback_marked(const char *path);
int openai_m124_save_clears(void);
int openai_m124_clear_clears(void);
int openai_m125_fail_clears(const char *path);
int openai_m125_fail_invalid(const char *path);
int openai_m125_normal_resets(const char *path);
int openai_m125_recovery_sets(const char *path);
int openai_m126_invalid_source(void);
int openai_m126_newest_source(void);
int openai_m126_recovery_source(void);
int openai_m126_invalid_overrides(void);
int openai_m127_hide_workflow(void);
int openai_m127_hide_build(void);
int openai_m127_hide_rollback(void);
int openai_m127_valid_details(void);
int openai_m128_load_snapshot(const char *path);
int openai_m128_current_isolated(void);
int openai_m128_save_refreshes(const char *path);
int openai_m128_fail_hides_current(void);
int openai_m129_fail_keeps_valid(void);
int openai_m129_fail_keeps_values(void);
int openai_m129_failure_reported(void);
int openai_m129_success_clears(void);
int openai_m130_first_fail_invalid(void);
int openai_m130_later_fail_valid(void);
int openai_m130_success_valid(void);
int openai_m130_load_resets_save(const char *path);
int openai_m131_clear_workflow(const char *path);
int openai_m131_clear_build(const char *path);
int openai_m131_clear_rollback(const char *path);
int openai_m131_save_preserves(void);
int openai_m132_clear_current_work(const char *path);
int openai_m132_clear_current_build(const char *path);
int openai_m132_clear_current_roll(const char *path);
int openai_m132_fail_keeps_current(void);
int openai_m133_reset_current(void);
int openai_m133_reset_snapshot(void);
int openai_m133_reset_flags(void);
int openai_m133_load_repopulates(const char *path);
int openai_m134_reset_consolidated(void);
int openai_m134_models_distinct(void);
int openai_m134_invalid_suppressed(void);
int openai_m134_final_audit(void);
int openai_m108_build_writer(const char *path);
int openai_m108_ops_writer(const char *path);
int openai_m108_null_path(void);
int openai_m108_null_ops(const char *path);
int openai_m135_shared_validation(const char *operations_path, const char *target_path);
int openai_m135_prewrite_validation(const char *operations_path, const char *target_path);
int openai_m135_chain_deterministic(const char *operations_path, const char *target_path);
int openai_m135_rollback_all(const char *operations_path, const char *target_path);
int openai_m136_evidence_rollback(const char *target_path, const char *evidence_path);
int openai_m136_commit_closes(const char *target_path);
int openai_m136_consume_restore(const char *plan_path, const char *new_path, const char *backup_path);
int openai_m136_final_state(const char *path);
int openai_m137_success_cleanup(const char *plan_path, const char *new_path, const char *backup_path);
int openai_m137_failure_restore(const char *plan_path, const char *new_path, const char *backup_path);
int openai_m137_consumed_rejected(const char *plan_path);
int openai_m137_artifact_recovery(const char *plan_path, const char *new_path, const char *backup_path);
void openai_agent_write(agent_state *state, const char *goal);
void openai_agent_fix(agent_state *state, const char *goal);
void openai_agent_create(agent_state *state, const char *goal);
void openai_agent_build(agent_state *state, const char *goal);
void openai_agent_retry(agent_state *state, const char *goal);
void openai_agent_repair(agent_state *state, const char *goal);
void openai_test_set_repair_plan(const char *plan_text,
                                 int auto_approve);
void openai_test_set_repair_plans(const char *plan_text1,
                                  const char *plan_text2,
                                  int auto_approve);
void openai_test_set_build_hook(openai_test_build_hook_fn hook);
char *openai_build_repair_prompt(const char *goal,
                                 const char *build_output);
char *openai_build_retry_prompt(const char *goal,
                                       const char *build_output,
                                       const char *previous_plan);
const char *openai_test_get_repair_prompt(unsigned int attempt);
const char *openai_workflow_name(int workflow);
const char *openai_rollback_name(int rollback_state);
void openai_status(const agent_state *state);
unsigned int openai_run_selftest(agent_state *state);
void openai_selftest(agent_state *state);
void openai_verify(agent_state *state);
void openai_review_file(agent_state *state, const char *path);
void openai_ask(agent_state *state, const char *prompt);
void openai_chat(agent_state *state, const char *prompt);
void openai_chat_reset(void);
int openai_chat_active(void);
int openai_plan_file_current(const char *plan_path, int verbose);
int openai_plan_is_current(int verbose);
int openai_plan_checksum_write(const char *plan_path);
int openai_plan_checksum_valid(const char *plan_path);
extern int openai_plan_approved;
extern unsigned long openai_approved_hash;
extern int openai_approval_invalidated;
const char *openai_plan_approval_name(void);
void openai_plan_approval_report(void);
int openai_m141_status_unapproved(void);
int openai_m141_status_approved(void);
int openai_m141_show_no_grant(void);
int openai_m141_validate_no_grant(void);
int openai_m141_dry_unapproved(void);
int openai_m141_dry_approved(void);
int openai_m141_change_invalidated(const char *path);
int openai_m141_refusal_message(void);
int openai_m142_cancel_unapproved(void);
int openai_m142_fail_unapproved(void);
int openai_m142_validate_keeps(void);
int openai_m142_prewrite_keeps(void);
int openai_m142_postwrite_clears(void);
int openai_m142_success_clears(void);
int openai_m142_checksum_invalid(const char *path);
int openai_m142_reapprove_clears(const char *path);
void openai_plan_recovery_sync(const char *plan_path);
void openai_m143_log_reset(void);
unsigned int openai_m143_inv_logs(void);
unsigned int openai_m143_cancel_logs(void);
unsigned int openai_m143_fail_logs(void);
void openai_m143_emit_cancel(void);
void openai_m143_emit_fail(void);
int openai_m143_first_inv_once(const char *path);
int openai_m143_repeat_no_dup(const char *path);
int openai_m143_cancel_once(void);
int openai_m143_fail_once(void);
int openai_m143_same_recovery(const char *path);
int openai_m143_diff_recovery(const char *path);
int openai_m143_consumed_clear(const char *path);
int openai_m143_partial_clear_safe(void);
void openai_plan_approval_consume(void);
int openai_m144_approved_fixture(void);
int openai_m144_invalid_fixture(const char *path);
int openai_m144_unapproved_fixture(void);
int openai_m144_reapprove_fixture(const char *path);
int openai_m144_prewrite_prod(void);
int openai_m144_status_prod(void);
int openai_m144_recovery_prod(const char *path);
int openai_m144_no_direct_globals(void);
void openai_plan_session_reset(void);
int openai_plan_recover_paths(const char *plan_path,
    const char *new_path, const char *backup_path);
int openai_state_load_path(const char *path);
int openai_m145_fresh_unapproved(void);
int openai_m145_active_no_restore(const char *path);
int openai_m145_consumed_no_restore(const char *path);
int openai_m145_new_no_restore(const char *plan_path,
    const char *new_path, const char *old_path);
int openai_m145_old_no_restore(const char *plan_path,
    const char *new_path, const char *old_path);
int openai_m145_state_no_restore(const char *path);
int openai_m145_logs_no_restore(void);
int openai_m145_reset_authority(const char *path);
int openai_plan_confirm_text(const char *text);
int openai_m146_lower_y(void);
int openai_m146_upper_y(void);
int openai_m146_yes_reject(void);
int openai_m146_empty_reject(void);
int openai_m146_space_reject(void);
int openai_m146_eof_reject(void);
int openai_m146_long_reject(void);
int openai_m146_alt_reject(void);
int command_args_empty(const char *arguments);
int command_agent_approve_args(const char *arguments);
int openai_m147_exact_dispatch(void);
int openai_m147_lower_dispatch(void);
int openai_m147_abbrev_reject(void);
int openai_m147_suffix_reject(void);
int openai_m147_argument_reject(void);
int openai_m147_duplicate_reject(void);
int openai_m147_space_accept(void);
int openai_m147_reject_unchanged(void);
int command_line_complete(const char *input,
    size_t input_size, int reached_eof);
int command_read_stream(FILE *stream,
    char *input, size_t input_size);
int openai_m148_complete_accept(void);
int openai_m148_exact_safe(void);
int openai_m148_prefix_blocked(void);
int openai_m148_remainder_drained(void);
int openai_m148_approval_unchanged(void);
int openai_m148_execute_no_action(void);
int openai_m148_eof_accept(void);
int openai_m148_empty_eof(void);
int openai_m149_single_line(void);
int openai_m149_two_line(void);
int openai_m195_normalize_one(void);
int openai_m195_normalize_two(void);
int openai_m195_normalize_correct(void);
int openai_m195_norm_inspect(void);
int openai_m195_reject_missing(void);
int openai_m195_reject_duplicate(void);
int openai_m195_reject_markers(void);
int openai_m195_failure_preserves(void);
int openai_m149_three_line(void);
int openai_m149_marker_removed(void);
int openai_m149_separator(void);
int openai_m149_boundary_space(void);
int openai_m149_odd_backslash(void);
int openai_m149_even_backslash(void);
int openai_m149_pending_eof(void);
int openai_m149_complete_eof(void);
int openai_m149_overflow(void);
int openai_m149_drain_next(void);
int openai_m149_single_dispatch(void);
void openai_plan_approval_clear(void);
int openai_plan_approve_file(const char *plan_path);
int openai_plan_approval_valid(const char *plan_path);
void openai_plan_approve(void);
int openai_m140_unapproved_reject(const char *path);
int openai_m140_approved_accept(const char *path);
int openai_m140_change_reject(const char *path);
int openai_m140_new_plan_clears(const char *path);
int openai_m140_clear_clears(const char *path);
int openai_m140_consume_clears(void);
int openai_m140_restart_unapproved(void);
int openai_m140_prewrite_guard(const char *path);
int openai_plan_clear_files(const char *plan_path);
int openai_m139_clear_sidecar(const char *plan_path);
int openai_m139_missing_checksum(const char *plan_path);
int openai_m139_orphan_checksum(const char *plan_path);
int openai_m139_consumed_checksum(const char *plan_path, const char *new_path, const char *backup_path);
int openai_m138_body_tamper(const char *plan_path);
int openai_m138_meta_tamper(const char *plan_path);
int openai_m138_partial_reject(const char *plan_path);
int openai_m138_save_preserves(const char *plan_path);

#endif /* OPENAI_INTERNAL_H */
