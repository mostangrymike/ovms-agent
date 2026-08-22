#ifndef LLM_INTERNAL_H
#define LLM_INTERNAL_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "LLM.H"
#include "LLM_LOG.H"
#include "LLM_PATH.H"
#include "LLM_CACHE.H"
#include "LLM_TOOL_SCHEMA.H"
#include "LLM_TOOL_REGISTRY.H"
#include "LLM_RESPONSE.H"
#include "LLM_JSON_PARSE.H"
#include "LLM_JSON_WRITE.H"
#include "LLM_TRANSPORT.H"
#include "LLM_REQUEST_BUILD.H"
#include "LLM_REQUEST_AGENT.H"
#include "LLM_REQUEST_BASIC.H"
#include "LLM_PLAN.H"
#include "LLM_EXECUTE.H"
#include "LLM_STATE.H"
#include "LLM_TYPES.H"
#include "project.h"

typedef char *(*llm_test_build_hook_fn)(int *build_status);

#define LLM_REQUEST_FILE  "OVMS_AGENT_REQUEST.JSON"
#define LLM_RESPONSE_FILE "OVMS_AGENT_RESPONSE.JSON"
#define LLM_HEADERS_FILE  "OVMS_AGENT_HEADERS.TXT"
#define LLM_BUILD_LOG_FILE "OVMS_AGENT_BUILD.LOG"
#define LLM_STATE_FILE "OVMS_AGENT.STATE"
#define LLM_BUILD_OUTPUT_LIMIT 65536
#define LLM_RESPONSE_ID_SIZE 128



#define LLM_AGENT_MAX_TURNS 12
#define LLM_PLAN_MAX_TURNS 24
#define LLM_SEARCH_OUTPUT_LIMIT 32768
#define LLM_LIST_OUTPUT_LIMIT 32768
#define LLM_CREATE_MAX_BYTES 65536U
#define LLM_RANGE_OUTPUT_LIMIT 65536U

typedef enum llm_replace_result {
    LLM_REPLACE_ERROR = 0,
    LLM_REPLACE_APPLIED = 1,
    LLM_REPLACE_DECLINED = 2
} llm_replace_result;

typedef enum llm_create_result {
    LLM_CREATE_ERROR = 0,
    LLM_CREATE_CREATED = 1,
    LLM_CREATE_DECLINED = 2
} llm_create_result;

extern char previous_response_id[LLM_RESPONSE_ID_SIZE];

extern int llm_last_workflow;
extern int llm_last_build_known;
extern int llm_last_build_status;
extern int llm_last_rollback;
extern int llm_saved_workflow;
extern int llm_saved_build_known;
extern int llm_saved_build_status;
extern int llm_saved_rollback;
extern int llm_state_loaded;
extern int llm_state_valid;
extern int llm_state_recovered;
extern int llm_state_save_known;
extern int llm_state_save_succeeded;
const char *llm_state_source_name(int valid, int recovered);
const char *llm_state_save_name(int known, int succeeded);
const char *llm_persist_workflow(int valid, int workflow);
const char *llm_persist_build(
    int valid, int build_known, int build_status);
const char *llm_persist_rollback(int valid, int rollback);

void llm_save_state(void); void llm_state_save(void);
void llm_load_state(void);
void llm_show_state(void);
void llm_show_memory(void);
void llm_clear_state(void);
void llm_log_repair_attempt(unsigned int attempt,
                               unsigned long plan_hash,
                               int build_status,
                               int rollback,
                               const char *outcome);
int llm_last_repair_record(char *output,
                              size_t output_size);
int llm_repair_status_text(char *output,
                              size_t output_size);
void llm_show_repair_status(void);
int llm_repair_history_text(char *output,
                               size_t output_size);
void llm_show_repair_history(void);
int llm_repair_failures_text(char *output,
                                size_t output_size);
void llm_show_repair_failures(void);
int llm_repair_show_text(unsigned long plan_hash,
                            char *output,
                            size_t output_size);
void llm_show_repair_plan(const char *arguments);
int llm_repair_export_file(const char *path,
                              int allow_overwrite);
void llm_export_repair_history(const char *arguments);
int llm_repair_stats_text(char *output,
                             size_t output_size);
void llm_show_repair_stats(void);
int llm_repair_config_text(char *output,
                              size_t output_size);
int llm_test_repair_config_text(const char *value,
                                   char *output,
                                   size_t output_size);
void llm_show_repair_config(void);
int llm_repair_info_text(char *output,
                            size_t output_size);
void llm_show_repair_info(void);
int llm_repair_check_text(char *output,
                             size_t output_size);
void llm_show_repair_check(void);
int llm_repair_diag_text(char *output,
                            size_t output_size);
void llm_show_repair_diag(void);
int llm_repair_count_text(char *output,
                             size_t output_size);
void llm_show_repair_count(void);
int llm_repair_latest_text(char *output,
                              size_t output_size);
void llm_show_repair_latest(void);
int llm_repair_oldest_text(char *output,
                              size_t output_size);
void llm_show_repair_oldest(void);
int llm_query_outcome_text(const char *arguments,
                              char *output,
                              size_t output_size);
void llm_show_query_outcome(const char *arguments);
int llm_query_attempts_text(const char *arguments,
                               char *output,
                               size_t output_size);
void llm_show_query_attempts(const char *arguments);
int llm_query_plan_text(const char *arguments,
                           char *output,
                           size_t output_size);
void llm_show_query_plan(const char *arguments);
int llm_query_since_text(const char *arguments,
                            char *output,
                            size_t output_size);
void llm_show_query_since(const char *arguments);
int llm_report_text(char *output,
                       size_t output_size);
void llm_show_report(void);
int llm_summary_text(char *output,
                        size_t output_size);
void llm_show_summary(void);
int llm_csv_file(const char *path,
                    int allow_overwrite);
int llm_kv_file(const char *path,
                   int allow_overwrite);
void llm_export_csv(const char *arguments);
void llm_export_kv(const char *arguments);
int llm_compare_text(char *output,
                        size_t output_size);
void llm_show_compare(void);
int llm_streak_text(char *output,
                       size_t output_size);
void llm_show_streak(void);
int llm_recovery_text(char *output,
                         size_t output_size);
void llm_show_recovery(void);
int llm_trend_text(char *output,
                      size_t output_size);
void llm_show_trend(void);
int llm_first_text(const char *arguments,
                      char *output,
                      size_t output_size);
void llm_show_first(const char *arguments);
int llm_last_text(const char *arguments,
                     char *output,
                     size_t output_size);
void llm_show_last(const char *arguments);
int llm_range_text(const char *arguments,
                      char *output,
                      size_t output_size);
void llm_show_range(const char *arguments);
int llm_index_text(const char *arguments,
                      char *output,
                      size_t output_size);
void llm_show_index(const char *arguments);
int llm_clear_repair_history(int approved);
void llm_clear_repair_cmd(void);
unsigned int llm_test_history_limit(const char *value);
void llm_test_set_history_limit(unsigned int limit);
void llm_test_set_log_path(const char *path);
void llm_send(agent_state *state,
                        const char *prompt,
                        int continue_conversation);
#include "LLM_PATH.H"
char *llm_read_text_file(const char *path);
char *make_tool_error(const char *message,
                             const char *path);
const char *llm_cache_lookup(
    const llm_file_cache_entry *cache,
    const char *path);
char *llm_duplicate_text(const char *text);
char *execute_read_file_range_tool(
    const char *arguments,
    char **display_path,
    long *display_start,
    long *display_end);
char *execute_read_file_tool(
    const char *arguments,
    llm_file_cache_entry *cache,
    int *cache_hit,
    char **display_path);
int llm_join_path(const char *parent,
                            const char *child,
                            char *output,
                            size_t output_size);
char *execute_list_directory_tool(const char *arguments,
                                         char **display_path);
char *execute_search_file_tool(const char *arguments,
                                      char **display_path,
                                      char **display_pattern);
llm_create_result execute_create_file_tool(
    const char *arguments,
    char **display_path);
llm_replace_result execute_replace_text_tool(
    agent_state *state,
    const char *arguments,
    char **display_path);
llm_replace_result execute_replace_lines_tool(
    agent_state *state,
    const char *arguments,
    char **display_path);
char *execute_run_build_tool(int *build_status);
int llm_confirm_restore(const char *path);
int llm_restore_previous_version(const char *path);
void llm_agent_mode(agent_state *state,
                       const char *goal,
                       int allow_write,
                       int build_after_write,
                       int workflow);
void llm_agent(agent_state *state, const char *goal);
void llm_agent_plan(agent_state *state, const char *goal);
void llm_plan_execute_dry(agent_state *state);
void llm_plan_execute(agent_state *state);
int llm_exec_validate_ops_file(const char *path);
int llm_m150_create_valid(void);
int llm_m150_create_old_reject(void);
int llm_m150_create_missing_new(void);
int llm_m150b_create_commit(void);
int llm_m150b_create_rollback(void);
int llm_m150b_existing_reject(void);
int llm_m150b_mixed_rollback(void);
int llm_m150b_same_path_reject(void);
int llm_m150c_has_create(void);
int llm_m150c_guards_target(void);
int llm_m150c_keeps_replace(void);
int llm_m150c_blocks_legacy(void);
int llm_m150d_success(void);
int llm_m150d_rollback(void);
int llm_m150d_tamper(void);
int llm_exec_stage_ops_file(const char *path, const char *expected_text);
int llm_exec_dry_validate(const char *operations_path, const char *target_path, const char *expected_original_text);
int llm_exec_stage_expect_fail(const char *operations_path, const char *target_path, const char *expected_original_text);
int llm_exec_rollback_validate(const char *operations_path, const char *target_path, const char *expected_original_text, int simulated_build_status);
int llm_m107_build_evidence(const char *path);
int llm_m107_ops_evidence(const char *path);
int llm_m107_state_serialize(const char *path);
int llm_m107_state_reload(const char *path);
int llm_m109_state_writer(const char *path);
int llm_m109_state_reader(const char *path);
int llm_m109_invalid_workflow(const char *path);
int llm_m109_incomplete_state(const char *path);
int llm_m110_dup_workflow(const char *path);
int llm_m110_dup_rollback(const char *path);
int llm_m110_unknown_field(const char *path);
int llm_m110_value_suffix(const char *path);
int llm_m111_workflow_first(const char *path);
int llm_m111_status_before_known(const char *path);
int llm_m111_rollback_first(const char *path);
int llm_m111_canonical_order(const char *path);
int llm_m112_big_workflow(const char *path);
int llm_m112_big_unknown(const char *path);
int llm_m112_unterm_middle(const char *path);
int llm_m112_unterm_final(const char *path);
int llm_m113_signed_workflow(const char *path);
int llm_m113_zero_workflow(const char *path);
int llm_m113_spaced_value(const char *path);
int llm_m113_zero_values(const char *path);
int llm_m114_unknown_nonzero(const char *path);
int llm_m114_unknown_zero(const char *path);
int llm_m114_known_success(const char *path);
int llm_m114_known_failure(const char *path);
int llm_m115_bad_workflow(const char *path);
int llm_m115_bad_build_flag(const char *path);
int llm_m115_bad_build_state(const char *path);
int llm_m115_bad_rollback(const char *path);
int llm_m116_atomic_create(const char *path);
int llm_m116_tmp_fail_keeps(const char *path);
int llm_m116_bad_values_keep(const char *path);
int llm_m116_tmp_versions_clean(const char *path);
int llm_m117_tmp_is_unique(const char *path);
int llm_m117_tmp_truncation(const char *path);
int llm_m117_tmp_no_collision(const char *path);
int llm_m117_atomic_valid(const char *path);
int llm_m118_purge_versions(const char *path);
int llm_m118_purge_missing(const char *path);
int llm_m118_purge_empty(void);
int llm_m118_state_stays_clear(const char *path);
int llm_m119_writer_negative(const char *path);
int llm_m119_loader_negative(const char *path);
int llm_m119_known_zero(const char *path);
int llm_m119_writer_reloads(const char *path);
int llm_m120_recovery_version(const char *path);
int llm_m120_versions_bounded(const char *path);
int llm_m120_newest_preserved(const char *path);
int llm_m120_unsafe_rejected(void);
int llm_m121_success_flags(void);
int llm_m121_failure_valid(void);
int llm_m121_failure_loaded(void);
int llm_m121_disk_preserved(const char *path);
int llm_m122_zero_is_newest(const char *path);
int llm_m122_minus_one_prior(const char *path);
int llm_m122_missing_prior_fails(const char *path);
int llm_m122_probe_read_only(const char *path);
int llm_m123_newest_preferred(const char *path);
int llm_m123_fallback_prior(const char *path);
int llm_m123_both_invalid(const char *path);
int llm_m123_read_only(const char *path);
int llm_m124_normal_load(const char *path);
int llm_m124_fallback_marked(const char *path);
int llm_m124_save_clears(void);
int llm_m124_clear_clears(void);
int llm_m125_fail_clears(const char *path);
int llm_m125_fail_invalid(const char *path);
int llm_m125_normal_resets(const char *path);
int llm_m125_recovery_sets(const char *path);
int llm_m126_invalid_source(void);
int llm_m126_newest_source(void);
int llm_m126_recovery_source(void);
int llm_m126_invalid_overrides(void);
int llm_m127_hide_workflow(void);
int llm_m127_hide_build(void);
int llm_m127_hide_rollback(void);
int llm_m127_valid_details(void);
int llm_m128_load_snapshot(const char *path);
int llm_m128_current_isolated(void);
int llm_m128_save_refreshes(const char *path);
int llm_m128_fail_hides_current(void);
int llm_m129_fail_keeps_valid(void);
int llm_m129_fail_keeps_values(void);
int llm_m129_failure_reported(void);
int llm_m129_success_clears(void);
int llm_m130_first_fail_invalid(void);
int llm_m130_later_fail_valid(void);
int llm_m130_success_valid(void);
int llm_m130_load_resets_save(const char *path);
int llm_m131_clear_workflow(const char *path);
int llm_m131_clear_build(const char *path);
int llm_m131_clear_rollback(const char *path);
int llm_m131_save_preserves(void);
int llm_m132_clear_current_work(const char *path);
int llm_m132_clear_current_build(const char *path);
int llm_m132_clear_current_roll(const char *path);
int llm_m132_fail_keeps_current(void);
int llm_m133_reset_current(void);
int llm_m133_reset_snapshot(void);
int llm_m133_reset_flags(void);
int llm_m133_load_repopulates(const char *path);
int llm_m134_reset_consolidated(void);
int llm_m134_models_distinct(void);
int llm_m134_invalid_suppressed(void);
int llm_m134_final_audit(void);
int llm_m108_build_writer(const char *path);
int llm_m108_ops_writer(const char *path);
int llm_m108_null_path(void);
int llm_m108_null_ops(const char *path);
int llm_m135_shared_validation(const char *operations_path, const char *target_path);
int llm_m135_prewrite_validation(const char *operations_path, const char *target_path);
int llm_m135_chain_deterministic(const char *operations_path, const char *target_path);
int llm_m135_rollback_all(const char *operations_path, const char *target_path);
int llm_m136_evidence_rollback(const char *target_path, const char *evidence_path);
int llm_m136_commit_closes(const char *target_path);
int llm_m136_consume_restore(const char *plan_path, const char *new_path, const char *backup_path);
int llm_m136_final_state(const char *path);
int llm_m137_success_cleanup(const char *plan_path, const char *new_path, const char *backup_path);
int llm_m137_failure_restore(const char *plan_path, const char *new_path, const char *backup_path);
int llm_m137_consumed_rejected(const char *plan_path);
int llm_m137_artifact_recovery(const char *plan_path, const char *new_path, const char *backup_path);
void llm_agent_write(agent_state *state, const char *goal);
void llm_agent_fix(agent_state *state, const char *goal);
void llm_agent_create(agent_state *state, const char *goal);
void llm_agent_build(agent_state *state, const char *goal);
void llm_agent_retry(agent_state *state, const char *goal);
void llm_agent_repair(agent_state *state, const char *goal);
void llm_test_set_repair_plan(const char *plan_text,
                                 int auto_approve);
void llm_test_set_repair_plans(const char *plan_text1,
                                  const char *plan_text2,
                                  int auto_approve);
void llm_test_set_build_hook(llm_test_build_hook_fn hook);
char *llm_build_repair_prompt(const char *goal,
                                 const char *build_output);
char *llm_build_retry_prompt(const char *goal,
                                       const char *build_output,
                                       const char *previous_plan);
const char *llm_test_get_repair_prompt(unsigned int attempt);
const char *llm_workflow_name(int workflow);
const char *llm_rollback_name(int rollback_state);
void llm_status(const agent_state *state);
unsigned int llm_run_selftest(agent_state *state);
void llm_selftest(agent_state *state);
void llm_verify(agent_state *state);
void llm_review_file(agent_state *state, const char *path);
void llm_ask(agent_state *state, const char *prompt);
void llm_chat(agent_state *state, const char *prompt);
void llm_chat_reset(void);
int llm_chat_active(void);
int llm_plan_file_current(const char *plan_path, int verbose);
int llm_plan_is_current(int verbose);
int llm_plan_checksum_write(const char *plan_path);
int llm_plan_checksum_valid(const char *plan_path);
extern int llm_plan_approved;
extern unsigned long llm_approved_hash;
extern int llm_approval_invalidated;
const char *llm_plan_approval_name(void);
void llm_plan_approval_report(void);
int llm_m141_status_unapproved(void);
int llm_m141_status_approved(void);
int llm_m141_show_no_grant(void);
int llm_m141_validate_no_grant(void);
int llm_m141_dry_unapproved(void);
int llm_m141_dry_approved(void);
int llm_m141_change_invalidated(const char *path);
int llm_m141_refusal_message(void);
int llm_m142_cancel_unapproved(void);
int llm_m142_fail_unapproved(void);
int llm_m142_validate_keeps(void);
int llm_m142_prewrite_keeps(void);
int llm_m142_postwrite_clears(void);
int llm_m142_success_clears(void);
int llm_m142_checksum_invalid(const char *path);
int llm_m142_reapprove_clears(const char *path);
void llm_plan_recovery_sync(const char *plan_path);
void llm_m143_log_reset(void);
unsigned int llm_m143_inv_logs(void);
unsigned int llm_m143_cancel_logs(void);
unsigned int llm_m143_fail_logs(void);
void llm_m143_emit_cancel(void);
void llm_m143_emit_fail(void);
int llm_m143_first_inv_once(const char *path);
int llm_m143_repeat_no_dup(const char *path);
int llm_m143_cancel_once(void);
int llm_m143_fail_once(void);
int llm_m143_same_recovery(const char *path);
int llm_m143_diff_recovery(const char *path);
int llm_m143_consumed_clear(const char *path);
int llm_m143_partial_clear_safe(void);
void llm_plan_approval_consume(void);
int llm_m144_approved_fixture(void);
int llm_m144_invalid_fixture(const char *path);
int llm_m144_unapproved_fixture(void);
int llm_m144_reapprove_fixture(const char *path);
int llm_m144_prewrite_prod(void);
int llm_m144_status_prod(void);
int llm_m144_recovery_prod(const char *path);
int llm_m144_no_direct_globals(void);
void llm_plan_session_reset(void);
int llm_plan_recover_paths(const char *plan_path,
    const char *new_path, const char *backup_path);
int llm_state_load_path(const char *path);
int llm_m145_fresh_unapproved(void);
int llm_m145_active_no_restore(const char *path);
int llm_m145_consumed_no_restore(const char *path);
int llm_m145_new_no_restore(const char *plan_path,
    const char *new_path, const char *old_path);
int llm_m145_old_no_restore(const char *plan_path,
    const char *new_path, const char *old_path);
int llm_m145_state_no_restore(const char *path);
int llm_m145_logs_no_restore(void);
int llm_m145_reset_authority(const char *path);
int llm_plan_confirm_text(const char *text);
int llm_m146_lower_y(void);
int llm_m146_upper_y(void);
int llm_m146_yes_reject(void);
int llm_m146_empty_reject(void);
int llm_m146_space_reject(void);
int llm_m146_eof_reject(void);
int llm_m146_long_reject(void);
int llm_m146_alt_reject(void);
int command_args_empty(const char *arguments);
int command_agent_approve_args(const char *arguments);
int llm_m147_exact_dispatch(void);
int llm_m147_lower_dispatch(void);
int llm_m147_abbrev_reject(void);
int llm_m147_suffix_reject(void);
int llm_m147_argument_reject(void);
int llm_m147_duplicate_reject(void);
int llm_m147_space_accept(void);
int llm_m147_reject_unchanged(void);
int command_line_complete(const char *input,
    size_t input_size, int reached_eof);
int command_read_stream(FILE *stream,
    char *input, size_t input_size);
int llm_m148_complete_accept(void);
int llm_m148_exact_safe(void);
int llm_m148_prefix_blocked(void);
int llm_m148_remainder_drained(void);
int llm_m148_approval_unchanged(void);
int llm_m148_execute_no_action(void);
int llm_m148_eof_accept(void);
int llm_m148_empty_eof(void);
int llm_m149_single_line(void);
int llm_m149_two_line(void);
int llm_m195_normalize_one(void);
int llm_m195_normalize_two(void);
int llm_m195_normalize_correct(void);
int llm_m195_norm_inspect(void);
int llm_m195_reject_missing(void);
int llm_m195_reject_duplicate(void);
int llm_m195_reject_markers(void);
int llm_m195_reject_early_begin(void);
int llm_m195_failure_preserves(void);
int llm_m149_three_line(void);
int llm_m149_marker_removed(void);
int llm_m149_separator(void);
int llm_m149_boundary_space(void);
int llm_m149_odd_backslash(void);
int llm_m149_even_backslash(void);
int llm_m149_pending_eof(void);
int llm_m149_complete_eof(void);
int llm_m149_overflow(void);
int llm_m149_drain_next(void);
int llm_m149_single_dispatch(void);
void llm_plan_approval_clear(void);
int llm_plan_approve_file(const char *plan_path);
int llm_plan_approval_valid(const char *plan_path);
void llm_plan_approve(void);
int llm_m140_unapproved_reject(const char *path);
int llm_m140_approved_accept(const char *path);
int llm_m140_change_reject(const char *path);
int llm_m140_new_plan_clears(const char *path);
int llm_m140_clear_clears(const char *path);
int llm_m140_consume_clears(void);
int llm_m140_restart_unapproved(void);
int llm_m140_prewrite_guard(const char *path);
int llm_plan_clear_files(const char *plan_path);
int llm_m139_clear_sidecar(const char *plan_path);
int llm_m139_missing_checksum(const char *plan_path);
int llm_m139_orphan_checksum(const char *plan_path);
int llm_m139_consumed_checksum(const char *plan_path, const char *new_path, const char *backup_path);
int llm_m138_body_tamper(const char *plan_path);
int llm_m138_meta_tamper(const char *plan_path);
int llm_m138_partial_reject(const char *plan_path);
int llm_m138_save_preserves(const char *plan_path);

int llm_tools_text(char *output, size_t output_size);
int llm_tools_ext_text(char *output, size_t output_size);
void llm_show_tools(void);
void llm_show_tools_ext(void);
int llm_lang_list_text(char *output, size_t output_size);
int llm_lang_info_text(const char *arguments,
                          char *output, size_t output_size);
int llm_lang_detect_text(const char *path,
                            char *output, size_t output_size);
int llm_lang_policy_text(char *output, size_t output_size);
void llm_show_langs(void);
void llm_show_lang_info(const char *arguments);
void llm_show_lang_detect(const char *path);
void llm_show_lang_policy(void);

#define LLM_GH_RESULT_MAX 2048U

typedef int (*llm_gh_executor_fn)(const char *operation,
                                     const char *arguments,
                                     char *result,
                                     size_t result_size,
                                     void *context);

int llm_github_text(char *output, size_t output_size);
int llm_gh_run_text(const char *operation,
                       const char *arguments,
                       llm_gh_executor_fn executor,
                       void *context,
                       char *output, size_t output_size);
void llm_show_github(const char *operation,
                        const char *arguments);
int llm_tool_info_text(const char *arguments,
                          char *output, size_t output_size);
void llm_show_tool_info(const char *arguments);
int llm_mcp_catalog_text(const char *config,
                            char *output, size_t output_size);
int llm_mcp_info_text(const char *config,
                         const char *arguments,
                         char *output, size_t output_size);
int llm_mcp_text(char *output, size_t output_size);
int llm_mcp_call_text(const char *config,
                         const char *arguments,
                         char *output, size_t output_size);
void llm_show_mcp(void);
void llm_show_mcp_info(const char *arguments);
void llm_show_mcp_call(const char *arguments);

typedef int (*llm_mcp_executor_fn)(
    const char *target,
    const char *tool,
    const char *arguments,
    char *result,
    size_t result_size,
    void *context);

int llm_mcp_execute_text(const char *config,
                            const char *arguments,
                            llm_mcp_executor_fn executor,
                            void *context,
                            char *output,
                            size_t output_size);
int llm_mcp_exec_transport_text(const char *config,
                                   const char *arguments,
                                   llm_mcp_executor_fn stdio_executor,
                                   llm_mcp_executor_fn http_executor,
                                   void *context,
                                   char *output,
                                   size_t output_size);
int llm_mcp_exec_all_text(const char *config,
                             const char *arguments,
                             llm_mcp_executor_fn stdio_executor,
                             llm_mcp_executor_fn http_executor,
                             llm_mcp_executor_fn sse_executor,
                             void *context,
                             char *output,
                             size_t output_size);

#define LLM_MCP_RES_TEXT_MAX 2048U

typedef enum llm_mcp_res_status {
    LLM_MCP_RES_INVALID = 0,
    LLM_MCP_RES_SUCCESS = 1,
    LLM_MCP_RES_FAILED = 2,
    LLM_MCP_RES_REFUSED = 3
} llm_mcp_res_status;

typedef struct llm_mcp_result {
    llm_mcp_res_status status;
    char server[64];
    char transport[16];
    char tool[64];
    char detail[LLM_MCP_RES_TEXT_MAX];
} llm_mcp_result;

int llm_mcp_run_result(const char *config,
                          const char *arguments,
                          llm_mcp_executor_fn stdio_executor,
                          llm_mcp_executor_fn http_executor,
                          llm_mcp_executor_fn sse_executor,
                          void *context,
                          llm_mcp_result *result);
int llm_mcp_result_text(const llm_mcp_result *result,
                           char *output, size_t output_size);
int llm_mcp_feedback_text(const llm_mcp_result *result,
                            char *output, size_t output_size);
int llm_mcp_record_result(const char *arguments,
                             const llm_mcp_result *result,
                             char *output, size_t output_size);
int llm_mcp_agent_call(const char *arguments,
                          char *output, size_t output_size);
void llm_show_mcp_execute(const char *arguments);
int llm_approval_text(char *output, size_t output_size);
void llm_show_approval(void);
int llm_set_approval(const char *arguments);
void llm_set_approval_cmd(const char *arguments);
void llm_reset_approval(void);
int llm_context_text(const agent_state *state,
                        char *output, size_t output_size);
void llm_show_context(const agent_state *state);
int llm_parity_text(char *output, size_t output_size);
void llm_show_parity(void);
int llm_final_parity_text(char *output, size_t output_size);
void llm_show_final_parity(void);
void llm_exec_goal(agent_state *state, const char *goal);
void llm_exec_dry(agent_state *state, const char *goal);
void llm_test_reset_approval(void);

const char *llm_approval_name(void);

int llm_session_new(const char *arguments, char created_id[9]);
int llm_session_list_text(char *output, size_t output_size);
int llm_session_show_text(const char *arguments,
                             char *output, size_t output_size);
int llm_session_resume(const char *arguments);
int llm_session_fork(const char *arguments, char created_id[9]);
int llm_session_rename(const char *arguments);
int llm_session_archive(const char *arguments);
int llm_session_unarchive(const char *arguments);
int llm_session_delete(const char *arguments);
int llm_session_current_text(char *output, size_t output_size);
int llm_session_note_goal(const char *goal);

void llm_show_session_list(void);
void llm_show_session(const char *arguments);
void llm_show_session_current(void);
void llm_session_new_cmd(const char *arguments);
void llm_session_resume_cmd(const char *arguments);
void llm_session_fork_cmd(const char *arguments);
void llm_session_rename_cmd(const char *arguments);
void llm_session_archive_cmd(const char *arguments);
void llm_session_unarc_cmd(const char *arguments);
void llm_session_delete_cmd(const char *arguments);

void llm_test_session_paths(const char *data_path,
                               const char *current_path);

int llm_session_current_id(char id[9]);

int llm_tool_run(agent_state *state, const char *arguments);
void llm_tool_run_cmd(agent_state *state, const char *arguments);
int llm_tool_last_text(char *output, size_t output_size);
int llm_tool_hist_text(char *output, size_t output_size);
void llm_show_tool_last(void);
void llm_show_tool_hist(void);
int llm_tool_clear(const char *arguments);
void llm_tool_clear_cmd(const char *arguments);

int llm_session_hist_text(const char *arguments,
                             char *output, size_t output_size);
void llm_show_session_hist(const char *arguments);
int llm_session_export(const char *arguments);
void llm_session_export_cmd(const char *arguments);
int llm_session_exec(agent_state *state, const char *arguments);
void llm_session_exec_cmd(agent_state *state, const char *arguments);

int llm_exec_resume(agent_state *state, const char *goal);
void llm_exec_resume_cmd(agent_state *state, const char *goal);
int llm_exec_fork(agent_state *state, const char *arguments);
void llm_exec_fork_cmd(agent_state *state, const char *arguments);

void llm_tx_note_exec(const char *name,
                         const char *goal,
                         const char *status);
void llm_test_tx_path(const char *path);

void llm_auto_begin(int workflow);
unsigned int llm_auto_turn_limit(int workflow);
void llm_auto_note_turn(void);
void llm_auto_note_tool(void);
int llm_auto_allow_write(void);
void llm_auto_finish(const char *reason);
int llm_auto_limits_text(char *output, size_t output_size);
int llm_auto_status_text(char *output, size_t output_size);
void llm_show_auto_limits(void);
void llm_show_auto_status(void);
void llm_auto_reset(void);
void llm_auto_test_limits(unsigned int turns,
                             unsigned int writes);

void llm_tx_model_call(const char *name,
                          const char *arguments);
void llm_tx_model_result(const char *name,
                            const char *status,
                            const char *output);
void llm_tx_loop_event(const char *name,
                          const char *status);

int llm_session_parent(const char *arguments, char parent[9]);

int llm_context_evidence_text(const char *session,
                                 char *output, size_t output_size);
int llm_context_build(const char *goal,
                         char *output, size_t output_size);
int llm_context_current(char *output, size_t output_size);
void llm_show_context_current(void);
void llm_exec_context(agent_state *state, const char *goal);

int llm_instr_reload(const agent_state *state);
int llm_instr_compose(const agent_state *state,
                         const char *goal,
                         char *output, size_t output_size);
int llm_instr_status_text(const agent_state *state,
                             char *output, size_t output_size);
int llm_instr_show_text(const agent_state *state,
                           char *output, size_t output_size);
void llm_show_instr_status(const agent_state *state);
void llm_show_instr(const agent_state *state);
void llm_instr_reload_cmd(const agent_state *state);
void llm_test_instr_path(const char *path);

int llm_project_refresh(const agent_state *state);
int llm_project_map_text(const agent_state *state,
                            char *output, size_t output_size);
int llm_project_files_text(const agent_state *state,
                              char *output, size_t output_size);
int llm_project_src_text(const agent_state *state,
                            char *output, size_t output_size);
int llm_project_tests_text(const agent_state *state,
                              char *output, size_t output_size);
int llm_project_build_text(const agent_state *state,
                              char *output, size_t output_size);
int llm_project_context(const agent_state *state,
                           char *output, size_t output_size);
int llm_project_compose(const agent_state *state,
                           const char *goal,
                           char *output, size_t output_size);
void llm_show_project_map(const agent_state *state,
                             int filter);
void llm_show_project_ctx(const agent_state *state);
void llm_project_refresh_cmd(const agent_state *state);

int llm_git_refresh(const agent_state *state);
int llm_git_status_text(const agent_state *state,
                           char *output, size_t output_size);
int llm_git_diff_text(const agent_state *state,
                         char *output, size_t output_size);
int llm_git_changed_text(const agent_state *state,
                            char *output, size_t output_size);
int llm_git_context(const agent_state *state,
                       char *output, size_t output_size);
int llm_git_compose(const agent_state *state,
                       const char *goal,
                       char *output, size_t output_size);
void llm_show_git_status(const agent_state *state);
void llm_show_git_diff(const agent_state *state);
void llm_show_git_changed(const agent_state *state);
void llm_show_git_context(const agent_state *state);
void llm_git_refresh_cmd(const agent_state *state);
void llm_test_git_data(const char *status_text,
                          const char *diff_text);

int llm_patch_apply_json(const char *arguments,
                            char *output, size_t output_size);
int llm_patch_validate(const char *spec_path,
                          char *output, size_t output_size);
int llm_patch_dry(const char *spec_path,
                     char *output, size_t output_size);
int llm_patch_apply(const char *spec_path,
                       char *output, size_t output_size);
int llm_patch_last_text(char *output, size_t output_size);
void llm_patch_validate_cmd(const char *arguments);
void llm_patch_dry_cmd(const char *arguments);
void llm_patch_apply_cmd(const char *arguments);
void llm_patch_last_cmd(void);

char *llm_result_make(const char *tool,
                         const char *status,
                         const char *effect,
                         int code,
                         const char *arguments,
                         const char *output);
char *llm_build_result(const char *output, int status);
int llm_result_last_text(char *output, size_t output_size);
void llm_show_result_last(void);

int llm_session_results_text(const char *arguments,
                                char *output, size_t output_size);
int llm_session_result_last(const char *arguments,
                               char *output, size_t output_size);
int llm_session_clear_results(const char *arguments);
void llm_show_session_results(const char *arguments);
void llm_show_session_result(const char *arguments);
void llm_session_clear_res_cmd(const char *arguments);

#endif /* LLM_INTERNAL_H */