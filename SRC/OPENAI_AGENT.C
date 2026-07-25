#include "openai_internal.h"
#include "openai_prompts.h"

#ifndef OPENAI_PLAN_MAX_TURNS
#define OPENAI_PLAN_MAX_TURNS 24
#endif

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
    turn_limit = workflow == OPENAI_WORKFLOW_PLAN ?
        OPENAI_PLAN_MAX_TURNS : OPENAI_AGENT_MAX_TURNS;
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
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;

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

                free(plan_text);
                free(json);
                remove_temporary_files();
                free(previous_id);
                openai_cache_free(cache);
                return;
            }
        } else if (display_output_text_from_json(json)) {
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

        if (strcmp(name, "list_directory") == 0) {
            char *display_path;

            display_path = NULL;
            tool_output = execute_list_directory_tool(
                arguments,
                &display_path
            );

            (void)printf("Tool executed: list_directory %s\n",
                         display_path != NULL ?
                             display_path : "");
            free(display_path);
        } else if (strcmp(name, "read_file") == 0) {
            int cache_hit;
            char *display_path;

            display_path = NULL;
            tool_output = execute_read_file_tool(
                arguments,
                cache,
                &cache_hit,
                &display_path
            );

            (void)printf("Tool executed: read_file %s%s\n",
                         display_path != NULL ? display_path : "",
                         cache_hit ? " [cache]" : "");
            free(display_path);
        } else if (strcmp(name, "read_file_range") == 0) {
            char *display_path;
            long display_start;
            long display_end;

            display_path = NULL;
            display_start = 0L;
            display_end = 0L;

            tool_output = execute_read_file_range_tool(
                arguments,
                &display_path,
                &display_start,
                &display_end
            );

            (void)printf(
                "Tool executed: read_file_range %s %ld-%ld\n",
                display_path != NULL ? display_path : "",
                display_start,
                display_end
            );

            free(display_path);
        } else if (strcmp(name, "search_file") == 0) {
            char *display_path;
            char *display_pattern;

            display_path = NULL;
            display_pattern = NULL;
            tool_output = execute_search_file_tool(
                arguments,
                &display_path,
                &display_pattern
            );

            (void)printf("Tool executed: search_file %s \"%s\"\n",
                         display_path != NULL ? display_path : "",
                         display_pattern != NULL ?
                             display_pattern : "");
            free(display_path);
            free(display_pattern);
        } else if (allow_write &&
                   (strcmp(name, "replace_text") == 0 ||
                    strcmp(name, "replace_lines") == 0)) {
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
            use_lines = strcmp(name, "replace_lines") == 0;
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

void openai_agent(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 0, 0, OPENAI_WORKFLOW_AGENT);
}

void openai_agent_plan(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 0, 0, OPENAI_WORKFLOW_PLAN);
}

void openai_agent_write(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 1, 0, OPENAI_WORKFLOW_WRITE);
}

void openai_agent_fix(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 1, 1, OPENAI_WORKFLOW_FIX);
}

void openai_agent_create(agent_state *state, const char *goal)
{
    const char *instructions;

    const char *api_key;
    const char *model;
    char *previous_id;
    char *tool_output;
    char *call_id;
    unsigned int turn;
    int create_attempted;
    openai_file_cache_entry cache[OPENAI_AGENT_CACHE_SIZE];

    openai_last_workflow = OPENAI_WORKFLOW_CREATE;
    openai_log_event("AGENT/CREATE", "start", 0);

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/CREATE goal");
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

    instructions = openai_prompt_create();

    if (!write_headers(api_key)) {
        return;
    }

    previous_id = NULL;
    tool_output = NULL;
    call_id = NULL;
    create_attempted = 0;
    openai_cache_init(cache);

    (void)puts("Starting guarded file-creation agent...");

    for (turn = 0U; turn < OPENAI_AGENT_MAX_TURNS; ++turn) {
        char *json;
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;

        if (!write_create_agent_request(
                model,
                instructions,
                goal,
                previous_id,
                call_id,
                tool_output)) {
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

        if (display_output_text_from_json(json)) {
            free(json);
            remove_temporary_files();
            free(previous_id);
            openai_cache_free(cache);
            return;
        }

        name = NULL;
        new_call_id = NULL;
        arguments = NULL;

        if (!extract_function_call(
                json,
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

        if (strcmp(name, "list_directory") == 0) {
            char *display_path;

            display_path = NULL;
            tool_output = execute_list_directory_tool(
                arguments,
                &display_path
            );

            (void)printf(
                "Tool executed: list_directory %s\n",
                display_path != NULL ? display_path : ""
            );
            free(display_path);
        } else if (strcmp(name, "read_file") == 0) {
            int cache_hit;
            char *display_path;

            display_path = NULL;
            tool_output = execute_read_file_tool(
                arguments,
                cache,
                &cache_hit,
                &display_path
            );

            (void)printf(
                "Tool executed: read_file %s%s\n",
                display_path != NULL ? display_path : "",
                cache_hit ? " [cache]" : ""
            );
            free(display_path);
        } else if (strcmp(name, "read_file_range") == 0) {
            char *display_path;
            long display_start;
            long display_end;

            display_path = NULL;
            display_start = 0L;
            display_end = 0L;

            tool_output = execute_read_file_range_tool(
                arguments,
                &display_path,
                &display_start,
                &display_end
            );

            (void)printf(
                "Tool executed: read_file_range %s %ld-%ld\n",
                display_path != NULL ? display_path : "",
                display_start,
                display_end
            );

            free(display_path);
        } else if (strcmp(name, "search_file") == 0) {
            char *display_path;
            char *display_pattern;

            display_path = NULL;
            display_pattern = NULL;
            tool_output = execute_search_file_tool(
                arguments,
                &display_path,
                &display_pattern
            );

            (void)printf(
                "Tool executed: search_file %s \"%s\"\n",
                display_path != NULL ? display_path : "",
                display_pattern != NULL ? display_pattern : ""
            );
            free(display_path);
            free(display_pattern);
        } else if (strcmp(name, "create_file") == 0) {
            char *display_path;
            openai_create_result create_result;

            if (create_attempted) {
                (void)puts(
                    "Second create request rejected; only one new file "
                    "is allowed per AGENT/CREATE run."
                );
                free(name);
                free(arguments);
                free(new_call_id);
                break;
            }

            create_attempted = 1;
            display_path = NULL;
            create_result = execute_create_file_tool(
                arguments,
                &display_path
            );

            (void)printf(
                "Tool requested: create_file %s\n",
                display_path != NULL ? display_path : ""
            );

            free(display_path);
            free(name);
            free(arguments);
            free(new_call_id);

            if (create_result == OPENAI_CREATE_CREATED) {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_created",
                    1
                );
                (void)puts("File created. Agent complete.");
            } else if (create_result == OPENAI_CREATE_DECLINED) {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_create_declined",
                    0
                );
                (void)puts("File creation cancelled. Agent complete.");
            } else {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_create_failed",
                    0
                );
                (void)puts("File creation failed. Agent complete.");
            }

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
            (void)printf(
                "Unsupported tool requested: %s\n",
                name
            );
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

    (void)puts(
        "Create agent stopped before producing a final result "
        "(tool-turn limit or error)."
    );

    remove_temporary_files();
    free(previous_id);
    free(call_id);
    free(tool_output);
    openai_cache_free(cache);
}

void openai_agent_retry(agent_state *state, const char *goal)
{
    static const char introduction[] =
        "The current project build has just failed. This is a fresh "
        "supervised repair attempt. Use the current build diagnostics below "
        "as evidence. Inspect the current source before proposing one new "
        "smallest patch. Apply at most one replace_text or replace_lines "
        "operation and "
        "then run the controlled build. Do not propose unrelated cleanup."
        "\n\nUser goal:\n";
    static const char log_label[] =
        "\n\nCurrent failed build result:\n";
    char *build_output;
    char *combined_goal;
    size_t combined_size;
    int build_status;

    openai_last_workflow = OPENAI_WORKFLOW_RETRY;
    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/RETRY goal");
        return;
    }

    if (openai_path_is_sensitive(goal)) {
        (void)puts(
            "Request denied because it references a sensitive path."
        );
        return;
    }

    (void)puts("Checking the current project build before retrying...");

    build_output = execute_run_build_tool(&build_status);

    if (build_output == NULL) {
        (void)puts("Unable to capture the current build result.");
        return;
    }

    (void)printf(
        "Tool executed: run_build [%s, status %d]\n",
        (build_status & 1) != 0 ? "success" : "failure",
        build_status
    );

    if ((build_status & 1) != 0) {
        (void)puts(
            "Current build succeeds. The previous failure has already "
            "been resolved or rolled back; no repair is required."
        );
        free(build_output);
        return;
    }

    combined_size =
        strlen(introduction) +
        strlen(goal) +
        strlen(log_label) +
        strlen(build_output) +
        1U;

    combined_goal = malloc(combined_size);

    if (combined_goal == NULL) {
        (void)puts("Insufficient memory for retry prompt.");
        free(build_output);
        return;
    }

    (void)strcpy(combined_goal, introduction);
    (void)strcat(combined_goal, goal);
    (void)strcat(combined_goal, log_label);
    (void)strcat(combined_goal, build_output);

    (void)puts(
        "Current build still fails. Starting a fresh supervised retry..."
    );

    /*
     * A retry remains one patch and one post-patch controlled build. The
     * existing supervised mode enforces local confirmation and rollback.
     */
    openai_agent_mode(state, combined_goal, 1, 1, OPENAI_WORKFLOW_FIX);

    free(combined_goal);
    free(build_output);
}

