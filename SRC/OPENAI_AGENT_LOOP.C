#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_prompts.h"

void openai_agent_mode(agent_state *state,
                       const char *goal,
                       int allow_write,
                       int build_after_write,
                       int workflow)
{
    const char *api_key;
    const char *model;
    const char *instructions;
    char *previous_id;
    char *tool_output;
    char *call_id;
    unsigned int turn;
    unsigned int turn_limit;
    int write_attempted;
    openai_file_cache_entry cache[OPENAI_AGENT_CACHE_SIZE];

    if (workflow == OPENAI_WORKFLOW_PLAN) {
        openai_last_workflow = OPENAI_WORKFLOW_PLAN;
    } else if (build_after_write) {
        openai_last_workflow = OPENAI_WORKFLOW_FIX;
        openai_last_rollback = OPENAI_ROLLBACK_NONE;
    } else if (allow_write) {
        openai_last_workflow = OPENAI_WORKFLOW_WRITE;
    } else {
        openai_last_workflow = OPENAI_WORKFLOW_AGENT;
    }

    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        if (workflow == OPENAI_WORKFLOW_PLAN) {
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

    if (openai_path_is_sensitive(goal)) {
        (void)puts(
            "Request denied because it references a sensitive path."
        );
        return;
    }

    api_key = getenv("OPENAI_API_KEY");
    model = getenv("OVMS_AGENT_MODEL");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("OPENAI_API_KEY is not defined.");
        return;
    }

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        return;
    }

    if (!write_headers(api_key)) {
        return;
    }

    if (workflow == OPENAI_WORKFLOW_PLAN) {
        instructions = openai_prompt_plan();
    } else {
        instructions = allow_write ?
            openai_prompt_write() : openai_prompt_read_only();
    }

    previous_id = NULL;
    tool_output = NULL;
    call_id = NULL;
    write_attempted = 0;
    openai_auto_begin(workflow);
    turn_limit = openai_auto_turn_limit(workflow);
    openai_cache_init(cache);

    if (build_after_write) {
        (void)puts("Starting supervised fix-and-build agent...");
    } else {
        (void)puts(allow_write ?
            "Starting guarded write agent..." :
            "Starting read-only agent...");
    }

    for (turn = 0U; turn < turn_limit; ++turn) {
        char *json;

        openai_auto_note_turn();
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;
        const openai_tool_descriptor *descriptor;

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
            (void)puts("OpenAI request failed.");
            break;
        }

        json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);

        if (json == NULL) {
            (void)puts("Unable to read OpenAI response.");
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

        if (workflow == OPENAI_WORKFLOW_PLAN) {
            char *plan_text;

            plan_text = extract_output_text_from_json(json);

            if (plan_text != NULL) {
                (void)puts("");
                (void)puts(plan_text);

                if (openai_plan_save(goal, plan_text)) {
                    (void)puts("");
                    (void)puts(
                        "Implementation plan saved to "
                        "OVMS_AGENT_PLAN.TXT."
                    );
                } else {
                    (void)puts("");
                    (void)puts("Unable to save implementation plan.");
                }

                openai_auto_finish("final");
                openai_tx_loop_event("agent", "final");
                free(plan_text);
                free(json);
                remove_temporary_files();
                free(previous_id);
                openai_cache_free(cache);
                return;
            }
        } else if (display_output_text_from_json(json)) {
            openai_auto_finish("final");
            openai_tx_loop_event("agent", "final");
            free(json);
            remove_temporary_files();
            free(previous_id);
            openai_cache_free(cache);
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

        descriptor = openai_tool_find(name);
        openai_auto_note_tool();
        openai_tx_model_call(name, arguments);

        if (openai_tool_is_read(descriptor)) {
            tool_output = openai_tool_execute_read(
                descriptor,
                arguments,
                cache
            );
            openai_tx_model_result(
                name,
                tool_output != NULL ? "ok" : "error",
                tool_output != NULL ? tool_output : "No tool output."
            );
        } else if (allow_write &&
                   openai_tool_is_replace(descriptor) &&
                   !build_after_write) {
            char *display_path;
            openai_replace_result replace_result;
            int use_lines;

            if (!openai_auto_allow_write()) {
                tool_output = make_tool_error(
                    "Autonomous write limit reached",
                    name
                );
                openai_tx_model_result(
                    name, "limit", tool_output
                );
            } else {
                display_path = NULL;
                use_lines =
                    descriptor->kind == OPENAI_TOOL_REPLACE_LINES;

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

                if (replace_result == OPENAI_REPLACE_APPLIED) {
                    openai_log_event(
                        openai_workflow_name(openai_last_workflow),
                        "patch_applied",
                        1
                    );
                    tool_output = openai_duplicate_text(
                        "Patch applied successfully. Continue inspecting "
                        "the project if needed, make another bounded patch "
                        "only when necessary, or return the final answer."
                    );
                    openai_tx_model_result(
                        name, "applied", tool_output
                    );
                } else if (replace_result == OPENAI_REPLACE_DECLINED) {
                    openai_log_event(
                        openai_workflow_name(openai_last_workflow),
                        "patch_declined",
                        0
                    );
                    tool_output = make_tool_error(
                        "Patch declined by local user; do not assume it "
                        "was applied",
                        display_path != NULL ? display_path : name
                    );
                    openai_tx_model_result(
                        name, "declined", tool_output
                    );
                } else {
                    openai_log_event(
                        openai_workflow_name(openai_last_workflow),
                        "patch_failed",
                        0
                    );
                    tool_output = make_tool_error(
                        "Patch failed",
                        display_path != NULL ? display_path : name
                    );
                    openai_tx_model_result(
                        name, "error", tool_output
                    );
                }

                free(display_path);
            }
        } else if (allow_write &&
                   openai_tool_is_replace(descriptor)) {
            char *display_path;
            openai_replace_result replace_result;
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
                descriptor->kind == OPENAI_TOOL_REPLACE_LINES;
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

            if (replace_result == OPENAI_REPLACE_APPLIED) {
                openai_log_event(
                    openai_workflow_name(openai_last_workflow),
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
                    openai_last_rollback =
                        OPENAI_ROLLBACK_NOT_NEEDED;

                    (void)puts("Patch applied. Running controlled build...");
                    build_output = execute_run_build_tool(&build_status);

                    (void)printf(
                        "Tool executed: run_build [%s, status %d]\n",
                        (build_status & 1) != 0 ?
                            "success" : "failure",
                        build_status
                    );

                    if ((build_status & 1) == 0 &&
                        display_path != NULL) {
                        if (openai_confirm_restore(display_path)) {
                            if (openai_restore_previous_version(
                                    display_path)) {
                                openai_last_rollback =
                                    OPENAI_ROLLBACK_SUCCEEDED;
                                openai_log_event(
                                    openai_workflow_name(
                                        openai_last_workflow),
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
                                openai_last_rollback =
                                    OPENAI_ROLLBACK_FAILED;
                                openai_log_event(
                                    openai_workflow_name(
                                        openai_last_workflow),
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
                            openai_last_rollback =
                                OPENAI_ROLLBACK_DECLINED;
                            openai_log_event(
                                openai_workflow_name(
                                    openai_last_workflow),
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
                            "\n\nAnalyze the following OpenVMS build result. "
                            "State whether it succeeded. If it failed, identify "
                            "the first actionable compiler or linker diagnostic "
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
                            openai_send(state, summary_prompt, 0);
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
            } else if (replace_result == OPENAI_REPLACE_DECLINED) {
                openai_log_event(
                    openai_workflow_name(openai_last_workflow),
                    "patch_declined",
                    0
                );
                free(display_path);
                display_path = NULL;
                (void)puts(build_after_write ?
                    "Patch cancelled. Build not run. Agent complete." :
                    "Patch cancelled. Agent complete.");
            } else {
                openai_log_event(
                    openai_workflow_name(openai_last_workflow),
                    "patch_failed",
                    0
                );
                free(display_path);
                display_path = NULL;
                (void)puts(build_after_write ?
                    "Patch failed. Build not run. Agent complete." :
                    "Patch failed. Agent complete.");
            }

            if (replace_result == OPENAI_REPLACE_APPLIED) {
                openai_tx_model_result(
                    name, "applied",
                    "Supervised patch workflow completed."
                );
                openai_auto_finish("supervised");
            } else if (replace_result == OPENAI_REPLACE_DECLINED) {
                openai_tx_model_result(
                    name, "declined",
                    "Supervised patch was declined."
                );
                openai_auto_finish("declined");
            } else {
                openai_tx_model_result(
                    name, "error",
                    "Supervised patch failed."
                );
                openai_auto_finish("error");
            }
            openai_tx_loop_event("agent", "supervised");
            remove_temporary_files();
            free(previous_id);
            free(call_id);
            free(tool_output);
            openai_cache_free(cache);
            return;
        } else {
            tool_output = make_tool_error(
                "Unsupported tool requested",
                name
            );
            openai_tx_model_result(
                name, "unsupported", tool_output
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

    openai_auto_finish(
        turn >= turn_limit ? "turn-limit" : "error"
    );
    openai_tx_loop_event(
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
    openai_cache_free(cache);
}
