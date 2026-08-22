#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_PROMPTS.H"


static char *write_prompt_rules(const char *base)
{
    static const char rule[] =
        "\n\nBOUNDED WRITE WORKFLOW: Inspect only files directly needed for "
        "the requested change. Reuse prior tool results and do not reread an "
        "unchanged file unless new evidence makes that necessary. Once enough "
        "evidence exists for one bounded patch, propose the patch instead of "
        "continuing exploratory reads.";
    char *combined;
    size_t size;

    if (base == NULL) {
        return NULL;
    }

    size = strlen(base) + strlen(rule) + 1U;
    combined = (char *)malloc(size);
    if (combined == NULL) {
        return NULL;
    }

    (void)strcpy(combined, base);
    (void)strcat(combined, rule);
    return combined;
}

void llm_agent_mode(agent_state *state,
                       const char *goal,
                       int allow_write,
                       int build_after_write,
                       int workflow)
{
    const char *api_key;
    const char *model;
    const char *instructions;
    char *owned_instructions;
    char *previous_id;
    char *tool_output;
    char *call_id;
    unsigned int turn;
    unsigned int turn_limit;
    int write_attempted;
    llm_file_cache_entry cache[LLM_AGENT_CACHE_SIZE];

    if (workflow == LLM_WORKFLOW_PLAN) {
        llm_last_workflow = LLM_WORKFLOW_PLAN;
    } else if (build_after_write) {
        llm_last_workflow = LLM_WORKFLOW_FIX;
        llm_last_rollback = LLM_ROLLBACK_NONE;
    } else if (allow_write) {
        llm_last_workflow = LLM_WORKFLOW_WRITE;
    } else {
        llm_last_workflow = LLM_WORKFLOW_AGENT;
    }

    llm_log_event(
        llm_workflow_name(llm_last_workflow),
        "start",
        0
    );

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)printf("Project root: %s\n", state->project_root);

    if (goal == NULL || *goal == '\0') {
        if (workflow == LLM_WORKFLOW_PLAN) {
            (void)puts("Usage: AGENT/PLAN goal");
        } else if (build_after_write) {
            (void)puts("Usage: AGENT/FIX goal");
        } else {
            (void)puts(allow_write ?
                "Usage: AGENT/WRITE goal" :
                "Usage: AGENT goal");
        }
        return;
    }

    api_key = getenv("OPENAI_API_KEY");
    model = getenv("OVMS_AGENT_MODEL");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("Provider API key is not defined.");
        return;
    }

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        return;
    }

    if (!write_headers(api_key)) {
        return;
    }

    owned_instructions = NULL;
    if (workflow == LLM_WORKFLOW_PLAN) {
        instructions = llm_prompt_plan();
    } else if (allow_write) {
        owned_instructions = write_prompt_rules(llm_prompt_write());
        if (owned_instructions == NULL) {
            (void)puts("Unable to prepare write-agent instructions.");
            return;
        }
        instructions = owned_instructions;
    } else {
        instructions = llm_prompt_read_only();
    }

    previous_id = NULL;
    tool_output = NULL;
    call_id = NULL;
    write_attempted = 0;
    llm_auto_begin(workflow);
    turn_limit = llm_auto_turn_limit(workflow);
    llm_cache_init(cache);

    if (build_after_write) {
        (void)puts("Starting supervised fix-and-build agent...");
    } else {
        (void)puts(allow_write ?
            "Starting guarded write agent..." :
            "Starting read-only agent...");
    }

    for (turn = 0U; turn < turn_limit; ++turn) {
        char *json;

        llm_auto_note_turn();
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;
        const llm_tool_descriptor *descriptor;

        if (!write_agent_request_mode(model,
                                      instructions,
                                      goal,
                                      previous_id,
                                      call_id,
                                      tool_output,
                                      allow_write)) {
            break;
        }

        free(call_id);
        call_id = NULL;
        free(tool_output);
        tool_output = NULL;

        if (!perform_openai_request()) {
            (void)puts("LLM request failed.");
            break;
        }

        json = read_entire_file(LLM_RESPONSE_FILE, NULL);

        if (json == NULL) {
            (void)puts("Unable to read LLM response.");
            break;
        }

        response_id = extract_top_level_id(json);

        if (response_id == NULL) {
            if (!display_api_error_from_json(json)) {
                (void)puts(
                    "Response did not contain an id or readable API error."
                );
            }

            free(json);
            break;
        }

        free(previous_id);
        previous_id = response_id;

        if (workflow == LLM_WORKFLOW_PLAN) {
            char *plan_text;

            plan_text = extract_output_text_from_json(json);

            if (plan_text != NULL) {
                (void)puts("");
                (void)puts(plan_text);

                if (llm_plan_save(goal, plan_text)) {
                    (void)puts("");
                    (void)puts(
                        "Implementation plan saved to "
                        "OVMS_AGENT_PLAN.TXT."
                    );
                } else {
                    (void)puts("");
                    (void)puts("Unable to save implementation plan.");
                }

                llm_auto_finish("final");
                llm_tx_loop_event("agent", "final");
                free(plan_text);
                free(json);
                remove_temporary_files();
                free(previous_id);
                llm_cache_free(cache);
                free(owned_instructions);
                return;
            }
        } else if (display_output_text_from_json(json)) {
            llm_auto_finish("final");
            llm_tx_loop_event("agent", "final");
            free(json);
            remove_temporary_files();
            free(previous_id);
            llm_cache_free(cache);
            free(owned_instructions);
            return;
        }

        name = NULL;
        new_call_id = NULL;
        arguments = NULL;

        if (!extract_function_call(json,
                                   &name,
                                   &new_call_id,
                                   &arguments)) {
            (void)puts(
                "Response contained neither output text nor a function call."
            );
            free(json);
            break;
        }

        free(json);

        descriptor = llm_tool_find(name);
        llm_auto_note_tool();
        llm_tx_model_call(name, arguments);

        if (llm_tool_is_read(descriptor)) {
            char *raw_output;

            raw_output = llm_tool_execute_read(
                descriptor,
                arguments,
                cache
            );

            tool_output = llm_result_make(
                name,
                raw_output != NULL ? "ok" : "error",
                "read",
                raw_output != NULL ? 1 : 0,
                arguments,
                raw_output != NULL ? raw_output : "No tool output."
            );

            if (tool_output == NULL && raw_output != NULL) {
                tool_output = raw_output;
                raw_output = NULL;
            }

            free(raw_output);

            llm_tx_model_result(
                name,
                tool_output != NULL ? "ok" : "error",
                tool_output != NULL ?
                    tool_output : "Unable to normalize tool output."
            );
        } else if (allow_write &&
                   strcmp(name, "structured_patch") == 0 &&
                   !build_after_write) {
            char patch_summary[2048];
            int patch_ok;

            if (!llm_auto_allow_write()) {
                tool_output = llm_result_make(
                    name, "limit", "write", 0,
                    arguments,
                    "Autonomous write limit reached."
                );
                llm_tx_model_result(
                    name, "limit",
                    tool_output != NULL ?
                        tool_output : "Autonomous write limit reached."
                );
            } else {
                patch_ok = llm_patch_apply_json(
                    arguments,
                    patch_summary,
                    sizeof(patch_summary)
                );

                (void)printf(
                    "Tool requested: structured_patch [%s]\n",
                    patch_ok ? "applied" : "rejected"
                );

                if (patch_ok) {
                    llm_cache_free(cache);
                    llm_cache_init(cache);
                    (void)llm_git_refresh(state);
                    llm_log_event(
                        llm_workflow_name(llm_last_workflow),
                        "patch_applied",
                        1
                    );
                    tool_output = llm_result_make(
                        name, "applied", "write", 1,
                        arguments, patch_summary
                    );
                    if (tool_output == NULL) {
                        tool_output = llm_duplicate_text(patch_summary);
                    }
                    llm_tx_model_result(
                        name, "applied",
                        tool_output != NULL ? tool_output : patch_summary
                    );
                } else {
                    llm_log_event(
                        llm_workflow_name(llm_last_workflow),
                        "patch_failed",
                        0
                    );
                    tool_output = llm_result_make(
                        name, "error", "write", 0,
                        arguments, patch_summary
                    );
                    if (tool_output == NULL) {
                        tool_output = make_tool_error(
                            patch_summary, "structured_patch"
                        );
                    }
                    llm_tx_model_result(
                        name, "error",
                        tool_output != NULL ? tool_output : patch_summary
                    );
                }
            }
        } else if (allow_write &&
                   llm_tool_is_replace(descriptor) &&
                   !build_after_write) {
            char *display_path;
            llm_replace_result replace_result;
            int use_lines;

            if (!llm_auto_allow_write()) {
                tool_output = llm_result_make(
                    name, "limit", "write", 0,
                    arguments,
                    "Autonomous write limit reached."
                );
                llm_tx_model_result(
                    name, "limit",
                    tool_output != NULL ?
                        tool_output : "Autonomous write limit reached."
                );
            } else {
                display_path = NULL;
                use_lines =
                    descriptor->kind == LLM_TOOL_REPLACE_LINES;

                replace_result = use_lines ?
                    execute_replace_lines_tool(
                        state, arguments, &display_path) :
                    execute_replace_text_tool(
                        state, arguments, &display_path);

                (void)printf(
                    "Tool requested: %s %s\n",
                    use_lines ? "replace_lines" : "replace_text",
                    display_path != NULL ? display_path : ""
                );

                if (replace_result == LLM_REPLACE_APPLIED) {
                    llm_cache_free(cache);
                    llm_cache_init(cache);
                    (void)llm_git_refresh(state);
                    llm_log_event(
                        llm_workflow_name(llm_last_workflow),
                        "patch_applied",
                        1
                    );
                    tool_output = llm_result_make(
                        name, "applied", "write", 1,
                        arguments,
                        "Patch applied successfully. Git context was "
                        "refreshed. Continue inspecting the project if "
                        "needed, make another bounded patch only when "
                        "necessary, or return the final answer."
                    );
                    llm_tx_model_result(
                        name, "applied",
                        tool_output != NULL ?
                            tool_output : "Patch applied successfully."
                    );
                } else if (replace_result == LLM_REPLACE_DECLINED) {
                    llm_log_event(
                        llm_workflow_name(llm_last_workflow),
                        "patch_declined",
                        0
                    );
                    tool_output = llm_result_make(
                        name, "declined", "write", 0,
                        arguments,
                        "Patch declined by local user; do not assume it "
                        "was applied."
                    );
                    llm_tx_model_result(
                        name, "declined",
                        tool_output != NULL ? tool_output : "Patch declined."
                    );
                } else {
                    llm_log_event(
                        llm_workflow_name(llm_last_workflow),
                        "patch_failed",
                        0
                    );
                    tool_output = llm_result_make(
                        name, "error", "write", 0,
                        arguments, "Patch failed."
                    );
                    llm_tx_model_result(
                        name, "error",
                        tool_output != NULL ? tool_output : "Patch failed."
                    );
                }

                free(display_path);
            }
        } else if (allow_write &&
                   llm_tool_is_replace(descriptor)) {
            char *display_path;
            llm_replace_result replace_result;
            int use_lines;

            if (write_attempted) {
                (void)puts(
                    "Second write request rejected; only one patch "
                    "attempt is allowed per AGENT/WRITE run."
                );
                free(name);
                free(arguments);
                free(new_call_id);
                break;
            }

            write_attempted = 1;
            display_path = NULL;
            use_lines =
                descriptor->kind == LLM_TOOL_REPLACE_LINES;
            replace_result = use_lines ?
                execute_replace_lines_tool(
                    state, arguments, &display_path) :
                execute_replace_text_tool(
                    state, arguments, &display_path);

            (void)printf("Tool requested: %s %s\n",
                         use_lines ? "replace_lines" : "replace_text",
                         display_path != NULL ?
                             display_path : "");
            free(name);
            free(arguments);
            free(new_call_id);

            if (replace_result == LLM_REPLACE_APPLIED) {
                llm_log_event(
                    llm_workflow_name(llm_last_workflow),
                    "patch_applied",
                    1
                );

                if (build_after_write) {
                    char *build_output;
                    int build_status;
                    const char *rollback_summary;

                    rollback_summary =
                        "Rollback status: not needed because the build "
                        "succeeded.";
                    llm_last_rollback =
                        LLM_ROLLBACK_NOT_NEEDED;

                    (void)puts("Patch applied. Running controlled build...");
                    build_output = execute_run_build_tool(&build_status);

                    (void)printf(
                        "Tool executed: run_build [%s, status %d]\n",
                        (build_status & 1) != 0 ?
                            "success" : "failure",
                        build_status
                    );

                    if (build_output != NULL) {
                        char *normalized_build;

                        normalized_build = llm_build_result(
                            build_output, build_status
                        );

                        if (normalized_build != NULL) {
                            free(build_output);
                            build_output = normalized_build;
                        }

                        llm_tx_model_result(
                            "run_build",
                            (build_status & 1) != 0 ?
                                "success" : "failure",
                            build_output
                        );
                    } else {
                        char *normalized_build;

                        normalized_build = llm_build_result(
                            NULL, build_status
                        );

                        if (normalized_build != NULL) {
                            build_output = normalized_build;
                            llm_tx_model_result(
                                "run_build",
                                (build_status & 1) != 0 ?
                                    "success" : "failure",
                                build_output
                            );
                        }
                    }

                    if ((build_status & 1) == 0 &&
                        display_path != NULL) {
                        if (llm_confirm_restore(display_path)) {
                            if (llm_restore_previous_version(
                                    display_path)) {
                                llm_last_rollback =
                                    LLM_ROLLBACK_SUCCEEDED;
                                llm_log_event(
                                    llm_workflow_name(
                                        llm_last_workflow),
                                    "rollback_succeeded",
                                    1
                                );
                                rollback_summary =
                                    "Rollback status: successful. The build "
                                    "failed, but the previous source contents "
                                    "were restored as the latest OpenVMS file "
                                    "version.";
                                (void)puts(
                                    "Rollback complete. The prior contents "
                                    "are now the latest file version."
                                );
                            } else {
                                llm_last_rollback =
                                    LLM_ROLLBACK_FAILED;
                                llm_log_event(
                                    llm_workflow_name(
                                        llm_last_workflow),
                                    "rollback_failed",
                                    0
                                );
                                rollback_summary =
                                    "Rollback status: requested but failed. "
                                    "The build failed and the patched source "
                                    "may still be the latest version.";
                                (void)puts(
                                    "Rollback was requested but failed."
                                );
                            }
                        } else {
                            llm_last_rollback =
                                LLM_ROLLBACK_DECLINED;
                            llm_log_event(
                                llm_workflow_name(
                                    llm_last_workflow),
                                "rollback_declined",
                                0
                            );
                            rollback_summary =
                                "Rollback status: declined. The build failed "
                                "and the patched source remains the latest "
                                "version.";
                            (void)puts(
                                "Rollback declined. The patched version "
                                "remains current."
                            );
                        }
                    }

                    if (build_output != NULL) {
                        static const char prefix[] =
                            "A supervised exact patch was applied for this "
                            "goal:\n\n";
                        static const char middle[] =
                            "\n\nAnalyze the following normalized OpenVMS "
                            "run_build tool result. Use its status, code, "
                            "truncation flag, and captured output as the "
                            "execution evidence. If it failed, identify the "
                            "first actionable compiler or linker diagnostic "
                            "and recommend the smallest next edit. Also report "
                            "the rollback status exactly as supplied below. "
                            "Do not claim to have made another edit.\n\n";
                        static const char rollback_label[] =
                            "\n\n";
                        size_t prompt_size;
                        char *summary_prompt;

                        prompt_size = strlen(prefix) + strlen(goal) +
                                      strlen(middle) + strlen(build_output) +
                                      strlen(rollback_label) +
                                      strlen(rollback_summary) + 1U;
                        summary_prompt = malloc(prompt_size);

                        if (summary_prompt != NULL) {
                            (void)strcpy(summary_prompt, prefix);
                            (void)strcat(summary_prompt, goal);
                            (void)strcat(summary_prompt, middle);
                            (void)strcat(summary_prompt, build_output);
                            (void)strcat(summary_prompt, rollback_label);
                            (void)strcat(summary_prompt, rollback_summary);
                            llm_send(state, summary_prompt, 0);
                            free(summary_prompt);
                        } else {
                            (void)puts(
                                "Unable to allocate build-summary prompt."
                            );
                        }

                        free(build_output);
                    } else {
                        (void)puts("Unable to capture build output.");
                    }

                    (void)puts("Fix-and-build agent complete.");
                } else {
                    (void)puts("Patch applied. Agent complete.");
                }

                free(display_path);
                display_path = NULL;
            } else if (replace_result == LLM_REPLACE_DECLINED) {
                llm_log_event(
                    llm_workflow_name(llm_last_workflow),
                    "patch_declined",
                    0
                );
                free(display_path);
                display_path = NULL;
                (void)puts(build_after_write ?
                    "Patch cancelled. Build not run. Agent complete." :
                    "Patch cancelled. Agent complete.");
            } else {
                llm_log_event(
                    llm_workflow_name(llm_last_workflow),
                    "patch_failed",
                    0
                );
                free(display_path);
                display_path = NULL;
                (void)puts(build_after_write ?
                    "Patch failed. Build not run. Agent complete." :
                    "Patch failed. Agent complete.");
            }

            if (replace_result == LLM_REPLACE_APPLIED) {
                llm_tx_model_result(
                    name, "applied",
                    "Supervised patch workflow completed."
                );
                llm_auto_finish("supervised");
            } else if (replace_result == LLM_REPLACE_DECLINED) {
                llm_tx_model_result(
                    name, "declined",
                    "Supervised patch was declined."
                );
                llm_auto_finish("declined");
            } else {
                llm_tx_model_result(
                    name, "error",
                    "Supervised patch failed."
                );
                llm_auto_finish("error");
            }
            llm_tx_loop_event("agent", "supervised");
            remove_temporary_files();
            free(previous_id);
            free(call_id);
            free(tool_output);
            llm_cache_free(cache);
            free(owned_instructions);
            return;
        } else {
            tool_output = llm_result_make(
                name, "unsupported", "unknown", 0,
                arguments, "Unsupported tool requested."
            );
            if (tool_output == NULL) {
                tool_output = make_tool_error(
                    "Unsupported tool requested", name
                );
            }
            llm_tx_model_result(
                name, "unsupported",
                tool_output != NULL ?
                    tool_output : "Unsupported tool requested."
            );
            (void)printf("Unsupported tool requested: %s\n", name);
        }

        free(name);
        free(arguments);

        if (tool_output == NULL) {
            free(new_call_id);
            (void)puts("Unable to produce tool output.");
            break;
        }

        call_id = new_call_id;
    }

    if (turn >= turn_limit &&
        previous_id != NULL &&
        call_id != NULL &&
        tool_output != NULL) {
        char *json;

        (void)puts(
            "Tool-turn limit reached; requesting final synthesis..."
        );

        if (write_agent_final_request(
                model,
                previous_id,
                call_id,
                tool_output)) {
            free(call_id);
            call_id = NULL;
            free(tool_output);
            tool_output = NULL;

            if (perform_openai_request()) {
                json = read_entire_file(LLM_RESPONSE_FILE, NULL);

                if (json != NULL) {
                    if (workflow == LLM_WORKFLOW_PLAN) {
                        char *plan_text;

                        plan_text = extract_output_text_from_json(json);

                        if (plan_text != NULL) {
                            (void)puts("");
                            (void)puts(plan_text);

                            if (llm_plan_save(goal, plan_text)) {
                                (void)puts("");
                                (void)puts(
                                    "Implementation plan saved to "
                                    "OVMS_AGENT_PLAN.TXT."
                                );
                            } else {
                                (void)puts("");
                                (void)puts(
                                    "Unable to save implementation plan."
                                );
                            }

                            llm_auto_finish("final");
                            llm_tx_loop_event("agent", "final");
                            free(plan_text);
                            free(json);
                            remove_temporary_files();
                            free(previous_id);
                            llm_cache_free(cache);
                            free(owned_instructions);
                            return;
                        }
                    } else if (display_output_text_from_json(json)) {
                        llm_auto_finish("final");
                        llm_tx_loop_event("agent", "final");
                        free(json);
                        remove_temporary_files();
                        free(previous_id);
                        llm_cache_free(cache);
                        free(owned_instructions);
                        return;
                    }

                    if (!display_api_error_from_json(json)) {
                        (void)puts(
                            "Final synthesis response contained no output text."
                        );
                    }
                    free(json);
                } else {
                    (void)puts("Unable to read final synthesis response.");
                }
            } else {
                (void)puts("Final synthesis request failed.");
            }
        }
    }

    llm_auto_finish(
        turn >= turn_limit ? "turn-limit" : "error"
    );
    llm_tx_loop_event(
        "agent",
        turn >= turn_limit ? "turn-limit" : "error"
    );

    (void)puts(
        "Agent stopped before producing a final answer "
        "(tool-turn limit or error)."
    );

    remove_temporary_files();
    free(previous_id);
    free(call_id);
    free(tool_output);
    llm_cache_free(cache);
    free(owned_instructions);
}
