#include "openai_internal.h"
#include "openai_execute.h"

const char *openai_workflow_name(int workflow)
{
    switch (workflow) {
    case OPENAI_WORKFLOW_ASK:
        return "ASK";
    case OPENAI_WORKFLOW_CHAT:
        return "CHAT";
    case OPENAI_WORKFLOW_REVIEW:
        return "REVIEW";
    case OPENAI_WORKFLOW_AGENT:
        return "AGENT";
    case OPENAI_WORKFLOW_WRITE:
        return "AGENT/WRITE";
    case OPENAI_WORKFLOW_FIX:
        return "AGENT/FIX";
    case OPENAI_WORKFLOW_BUILD:
        return "AGENT/BUILD";
    case OPENAI_WORKFLOW_RETRY:
        return "AGENT/RETRY";
    case OPENAI_WORKFLOW_SELFTEST:
        return "AGENT/SELFTEST";
    case OPENAI_WORKFLOW_VERIFY:
        return "AGENT/VERIFY";
    case OPENAI_WORKFLOW_CREATE:
        return "AGENT/CREATE";
    case OPENAI_WORKFLOW_PLAN:
        return "AGENT/PLAN";
    case OPENAI_WORKFLOW_EXECUTE:
        return "AGENT/EXECUTE";
    default:
        return "none";
    }
}

const char *openai_rollback_name(int rollback_state)
{
    switch (rollback_state) {
    case OPENAI_ROLLBACK_NOT_NEEDED:
        return "not needed";
    case OPENAI_ROLLBACK_SUCCEEDED:
        return "succeeded";
    case OPENAI_ROLLBACK_FAILED:
        return "failed";
    case OPENAI_ROLLBACK_DECLINED:
        return "declined";
    default:
        return "none recorded";
    }
}

void openai_status(const agent_state *state)
{
    const char *model;
    const char *api_key;

    openai_load_state();

    model = getenv("OVMS_AGENT_MODEL");
    api_key = getenv("OPENAI_API_KEY");

    (void)puts("OVMS Agent capability status");
    (void)puts("----------------------------");

    (void)printf(
        "Project root configured:  %s\n",
        state != NULL &&
        state->project_root != NULL &&
        *state->project_root != '\0' ?
            "yes" : "no"
    );

    (void)printf(
        "OpenAI API key defined:   %s\n",
        api_key != NULL && *api_key != '\0' ?
            "yes" : "no"
    );

    (void)printf(
        "OpenAI model:             %s\n",
        model != NULL && *model != '\0' ?
            model : "<not defined>"
    );

    (void)printf(
        "Chat conversation active: %s\n",
        previous_response_id[0] != '\0' ?
            "yes" : "no (not persisted)"
    );

    (void)printf(
        "Last persisted workflow:  %s\n",
        openai_persist_workflow(
            openai_state_valid,
            openai_saved_workflow)
    );

    if (!openai_state_valid) {
        (void)puts(
            "Last persisted build:     unavailable"
        );
    } else if (openai_saved_build_known) {
        (void)printf(
            "Last persisted build:     %s (status %d)\n",
            (openai_saved_build_status & 1) != 0 ?
                "success" : "failure",
            openai_saved_build_status
        );
    } else {
        (void)puts(
            "Last persisted build:     not recorded"
        );
    }

    (void)printf(
        "Last persisted rollback:  %s\n",
        openai_persist_rollback(
            openai_state_valid,
            openai_saved_rollback)
    );

    (void)printf(
        "Persistent state file:    %s\n",
        OPENAI_STATE_FILE
    );

    (void)printf(
        "Persistent state valid:   %s\n",
        openai_state_valid ? "yes" : "no"
    );

    (void)printf(
        "Persistent state source:  %s\n",
        openai_state_source_name(
            openai_state_valid,
            openai_state_recovered)
    );

    (void)printf(
        "Last state save attempt: %s\n",
        openai_state_save_name(
            openai_state_save_known,
            openai_state_save_succeeded)
    );

    openai_plan_approval_report();
    (void)puts("");
    (void)puts("Enabled modes:");
    (void)puts("  ASK, CHAT, CHAT/RESET, REVIEW");
    (void)puts(
        "  AGENT, AGENT/PLAN, AGENT/APPROVE, AGENT/EXECUTE, AGENT/WRITE, "
        "AGENT/CREATE, AGENT/BUILD"
    );
    (void)puts("  AGENT/FIX, AGENT/RETRY, AGENT/SELFTEST");
    (void)puts("  AGENT/STATUS, AGENT/VERIFY, AGENT/LOG");
    (void)puts("  AGENT/LOG/OLD, AGENT/LOG/CLEAR");
    (void)puts("  AGENT/METRICS, AGENT/STATE, AGENT/STATE/CLEAR");
    (void)puts("");
    (void)puts("Authority boundaries:");
    (void)puts("  AGENT is read-only.");
    (void)puts("  Writes require exact-match local confirmation.");
    (void)puts("  Builds invoke only the fixed BUILD.COM.");
    (void)puts("  Failed supervised fixes may offer local rollback.");
}

static void openai_selftest_report(const char *name,
                                   int passed,
                                   unsigned int *passed_count,
                                   unsigned int *failed_count)
{
    (void)printf(
        "%-36s %s\n",
        name,
        passed ? "PASS" : "FAIL"
    );

    if (passed) {
        ++(*passed_count);
    } else {
        ++(*failed_count);
    }
}

#include "openai_selftest_file_exists.inc"

unsigned int openai_run_selftest(agent_state *state)
{
    static const char output_json[] =
        "{"
        "\"id\":\"resp_test\","
        "\"output\":[{"
        "\"type\":\"message\","
        "\"content\":[{"
        "\"type\":\"output_text\","
        "\"text\":\"alpha\\nbeta\""
        "}]"
        "}]"
        "}";
    static const char function_json[] =
        "{"
        "\"id\":\"resp_test\","
        "\"output\":[{"
        "\"type\":\"function_call\","
        "\"name\":\"read_file\","
        "\"call_id\":\"call_test\","
        "\"arguments\":\"{\\\"path\\\":\\\"SRC/MAIN.C\\\"}\""
        "}]"
        "}";
    static const char malformed_json[] =
        "{\"output\":[{\"type\":\"output_text\","
        "\"text\":\"unterminated}";
    static const char integer_arguments[] =
        "{\"start_line\":12,\"end_line\":34}";
    const char *object;
    const char *value;
    char *decoded;
    char *name;
    char *call_id;
    char *arguments;
    FILE *build_file;
    const openai_tool_descriptor *read_descriptor;
    const openai_tool_descriptor *replace_descriptor;
    const openai_tool_descriptor *build_descriptor;
    openai_file_cache_entry test_cache[OPENAI_AGENT_CACHE_SIZE];
    const char *cached_text;
    char *malformed_text;
    long integer_value;
    unsigned int passed_count;
    unsigned int failed_count;

    passed_count = 0U;
    failed_count = 0U;

    (void)puts("OVMS Agent non-destructive self-test");
    (void)puts("------------------------------------");

    openai_selftest_report(
        "project state available",
        state != NULL &&
        state->project_root != NULL &&
        *state->project_root != '\0',
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "safe project path accepted",
        openai_path_is_safe("SRC/MAIN.C"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "parent traversal rejected",
        !openai_path_is_safe("../LOGIN.COM"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "device-qualified path rejected",
        !openai_path_is_safe("SYS$LOGIN:LOGIN.COM"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "API-key filename blocked",
        openai_path_is_sensitive("OPENAIKEY.TXT"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "header filename blocked",
        openai_path_is_sensitive("OVMS_AGENT_HEADERS.TXT"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "ordinary source not sensitive",
        !openai_path_is_sensitive("SRC/OPENAI.C"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "lowercase key filename blocked",
        openai_path_is_sensitive("config/server.key"),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "object listing entry hidden",
        openai_listing_entry_hidden("OPENAI_AGENT.OBJ"),
        &passed_count,
        &failed_count
    );

    read_descriptor = openai_tool_find("read_file");
    replace_descriptor = openai_tool_find("replace_text");
    build_descriptor = openai_tool_find("run_build");

    openai_selftest_report(
        "read tool registry metadata",
        read_descriptor != NULL &&
        read_descriptor->kind == OPENAI_TOOL_READ_FILE &&
        read_descriptor->allows_read &&
        !read_descriptor->allows_write &&
        !read_descriptor->requires_confirmation,
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "write tool registry metadata",
        replace_descriptor != NULL &&
        replace_descriptor->kind == OPENAI_TOOL_REPLACE_TEXT &&
        !replace_descriptor->allows_read &&
        replace_descriptor->allows_write &&
        replace_descriptor->requires_confirmation &&
        openai_tool_is_replace(replace_descriptor),
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "build tool registry metadata",
        build_descriptor != NULL &&
        build_descriptor->kind == OPENAI_TOOL_RUN_BUILD &&
        build_descriptor->requires_build,
        &passed_count,
        &failed_count
    );

    openai_selftest_report(
        "unknown tool rejected",
        openai_tool_find("not_a_real_tool") == NULL,
        &passed_count,
        &failed_count
    );

    openai_cache_init(test_cache);
    openai_selftest_report(
        "case-insensitive cache lookup",
        openai_cache_store(
            test_cache,
            "SRC/SELFTEST.C",
            "cached self-test text") &&
        (cached_text = openai_cache_lookup(
            test_cache,
            "src/selftest.c")) != NULL &&
        strcmp(cached_text, "cached self-test text") == 0,
        &passed_count,
        &failed_count
    );
    openai_cache_free(test_cache);

    malformed_text = extract_output_text_from_json(malformed_json);
    openai_selftest_report(
        "malformed JSON rejected",
        malformed_text == NULL,
        &passed_count,
        &failed_count
    );
    free(malformed_text);

    integer_value = 0L;
    openai_selftest_report(
        "integer JSON extraction",
        extract_integer_argument(
            integer_arguments,
            "end_line",
            &integer_value) &&
        integer_value == 34L,
        &passed_count,
        &failed_count
    );

    object = find_output_text_object(output_json);
    value = object != NULL ?
        find_string_value(object, "text") : NULL;
    decoded = value != NULL ?
        json_decode_string(value, NULL) : NULL;

    openai_selftest_report(
        "output_text JSON extraction",
        decoded != NULL &&
        strcmp(decoded, "alpha\nbeta") == 0,
        &passed_count,
        &failed_count
    );
    free(decoded);

    name = NULL;
    call_id = NULL;
    arguments = NULL;

    openai_selftest_report(
        "function-call JSON extraction",
        extract_function_call(
            function_json,
            &name,
            &call_id,
            &arguments
        ) &&
        name != NULL &&
        call_id != NULL &&
        arguments != NULL &&
        strcmp(name, "read_file") == 0 &&
        strcmp(call_id, "call_test") == 0 &&
        strstr(arguments, "SRC/MAIN.C") != NULL,
        &passed_count,
        &failed_count
    );

    free(name);
    free(call_id);
    free(arguments);

    {
        char *range_output;
        char *range_path;
        long range_start;
        long range_end;

        range_path = NULL;
        range_start = 0L;
        range_end = 0L;

        range_output = execute_read_file_range_tool(
            "{\"path\":\"SRC/MAIN.C\","
            "\"start_line\":1,"
            "\"end_line\":3}",
            &range_path,
            &range_start,
            &range_end
        );

        openai_selftest_report(
            "ranged source read",
            range_output != NULL &&
            range_path != NULL &&
            strcmp(range_path, "SRC/MAIN.C") == 0 &&
            range_start == 1L &&
            range_end == 3L,
            &passed_count,
            &failed_count
        );

        free(range_output);
        free(range_path);
    }

    {
        const char *large_path;
        FILE *large_file;
        unsigned long count;
        openai_file_cache_entry large_cache[OPENAI_AGENT_CACHE_SIZE];
        char *large_output;
        char *large_display;
        int large_cache_hit;
        int large_created;

        large_path = "M251_14_LARGE.TMP";
        large_file = fopen(large_path, "w");
        large_created = large_file != NULL;
        if (large_file != NULL) {
            for (count = 0UL; count < 65537UL; ++count) {
                if (fputc('X', large_file) == EOF) {
                    large_created = 0;
                    break;
                }
            }
            if (fclose(large_file) != 0) {
                large_created = 0;
            }
        }

        openai_cache_init(large_cache);
        large_display = NULL;
        large_cache_hit = 0;
        large_output = large_created ?
            execute_read_file_tool(
                "{\"path\":\"M251_14_LARGE.TMP\"}",
                large_cache,
                &large_cache_hit,
                &large_display
            ) : NULL;

        openai_selftest_report(
            "oversized read suggests ranged fallback",
            large_output != NULL &&
            strstr(large_output, "Whole-file read exceeds 65536 bytes") != NULL &&
            strstr(large_output, "search_file") != NULL &&
            strstr(large_output, "read_file_range") != NULL &&
            large_cache_hit == 0,
            &passed_count,
            &failed_count
        );

        free(large_output);
        free(large_display);
        openai_cache_free(large_cache);
        (void)remove(large_path);
    }

    build_file = fopen("BUILD.COM", "r");

    openai_selftest_report(
        "fixed BUILD.COM is readable",
        build_file != NULL,
        &passed_count,
        &failed_count
    );

    if (build_file != NULL) {
        (void)fclose(build_file);
    }
    #include "openai_test_operation_count.inc"
    #include "openai_test_missing_end.inc"
    #include "openai_test_bad_type.inc"
    #include "openai_test_dup_type.inc"
    #include "openai_test_dup_field_path.inc"
    #include "openai_test_block_before_type.inc"
    #include "openai_test_path_before_type.inc"
    #include "openai_test_new_before_old.inc"
    #include "openai_test_path_after_old.inc"
    #include "openai_test_type_after_old.inc"
    #include "openai_test_field_after_new.inc"
    #include "openai_test_nested_begin.inc"
    #include "openai_test_second_count.inc"
    #include "openai_test_count_after_begin.inc"
    #include "openai_test_field_before_begin.inc"
    #include "openai_test_block_outside.inc"
    #include "openai_test_trailing_syntax.inc"
    #include "openai_test_prose_only_ops.inc"
    #include "openai_test_legacy_with_block.inc"
    #include "openai_test_block_with_text.inc"
    #include "openai_test_unexpected_field.inc"
    #include "openai_test_unexpected_toplevel.inc"
    #include "openai_test_valid_preamble.inc"
    #include "openai_test_blank_between_ops.inc"
    #include "openai_test_two_valid_ops.inc"
    #include "openai_test_m103.inc"
    #include "openai_test_m106.inc"
    #include "openai_test_m107.inc"
    #include "openai_test_m108.inc"
    #include "openai_test_m109.inc"
    #include "openai_test_m110.inc"
    #include "openai_test_m111.inc"
    #include "openai_test_m112.inc"
    #include "openai_test_m113.inc"
    #include "openai_test_m114.inc"
    #include "openai_test_m115.inc"
    #include "openai_test_m116.inc"
    #include "openai_test_m117.inc"
    #include "openai_test_m118.inc"
    #include "openai_test_m119.inc"
    #include "openai_test_m120.inc"
    #include "openai_test_m121.inc"
    #include "openai_test_m122.inc"
    #include "openai_test_m123.inc"
    #include "openai_test_m124.inc"
    #include "openai_test_m125.inc"
    #include "openai_test_m126.inc"
    #include "openai_test_m127.inc"
    #include "openai_test_m128.inc"
    #include "openai_test_m129.inc"
    #include "openai_test_m130.inc"
    #include "openai_test_m131.inc"
    #include "openai_test_m132.inc"
    #include "openai_test_m133.inc"
    #include "openai_test_m134.inc"
    #include "openai_test_m135.inc"
    #include "openai_test_m136.inc"
    #include "openai_test_m137.inc"
    #include "openai_test_m138.inc"
    #include "openai_test_m139.inc"
    #include "openai_test_m140.inc"
    #include "openai_test_m141.inc"
    #include "openai_test_m142.inc"
    #include "openai_test_m143.inc"
    #include "openai_test_m144.inc"
    #include "openai_test_m145.inc"
    #include "openai_test_m146.inc"
    #include "openai_test_m147.inc"
    #include "openai_test_m148.inc"
    #include "openai_test_m149.inc"
    #include "openai_test_m150.inc"
    #include "openai_test_m150b.inc"
    #include "openai_test_m150c.inc"
    #include "openai_test_m150d.inc"
    #include "openai_test_block_before_path.inc"
    #include "openai_test_multiline.inc"
    #include "openai_test_dup_path.inc"
    #include "openai_test_same_file_chain.inc"
    #include "openai_test_dry_stage.inc"
    #include "openai_test_dry_unchanged.inc"
    #include "openai_test_dry_stale.inc"
    #include "openai_test_dry_missing.inc"
    #include "openai_test_dry_nonunique.inc"
    #include "openai_test_failed_same_file_chain.inc"
    #include "openai_test_nonunique_chain.inc"
    #include "openai_test_oversized_count.inc"
    #include "openai_test_multiline_block.inc"
    #include "openai_test_block_missing_old_end.inc"
    #include "openai_test_block_missing_new_end.inc"
    #include "openai_test_block_empty_old.inc"
    #include "openai_test_block_with_legacy_type.inc"
    #include "openai_test_block_mixed_legacy.inc"
    #include "openai_test_block_escaped_end.inc"
    #include "openai_test_block_empty_new.inc"

    (void)puts("------------------------------------");
    (void)printf(
        "Self-test result: %u passed, %u failed.\n",
        passed_count,
        failed_count
    );

    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        failed_count == 0U ?
            "selftest_pass" : "selftest_fail",
        (int)failed_count
    );

    if (failed_count == 0U) {
        (void)puts("All non-destructive checks passed.");
    } else {
        (void)puts(
            "One or more checks failed. Do not enable additional "
            "agent authority until these failures are understood."
        );
    }

    return failed_count;
}

void openai_selftest(agent_state *state)
{
    openai_last_workflow = OPENAI_WORKFLOW_SELFTEST;
    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );
    (void)openai_run_selftest(state);
}

void openai_verify(agent_state *state)
{
    unsigned int selftest_failures;
    char *build_output;
    int build_status;
    int build_passed;

    openai_last_workflow = OPENAI_WORKFLOW_VERIFY;
    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    (void)puts("OVMS Agent verification gate");
    (void)puts("============================");

    selftest_failures = openai_run_selftest(state);

    (void)puts("");
    (void)puts("Controlled build verification");
    (void)puts("-----------------------------");

    build_output = execute_run_build_tool(&build_status);
    build_passed = build_output != NULL &&
                   (build_status & 1) != 0;

    (void)printf(
        "fixed BUILD.COM execution             %s\n",
        build_passed ? "PASS" : "FAIL"
    );

    (void)printf(
        "OpenVMS build status                  %d\n",
        build_status
    );

    if (!build_passed && build_output != NULL) {
        (void)puts("");
        (void)puts("Captured build output:");
        (void)puts(build_output);
    }

    free(build_output);

    (void)puts("============================");

    openai_log_event(
        "AGENT/VERIFY",
        selftest_failures == 0U && build_passed ?
            "verify_pass" : "verify_fail",
        build_status
    );

    if (selftest_failures == 0U && build_passed) {
        (void)puts(
            "Verification result: PASS. Safety checks and controlled "
            "build are healthy."
        );
    } else {
        (void)printf(
            "Verification result: FAIL. Self-test failures: %u; "
            "build: %s.\n",
            selftest_failures,
            build_passed ? "pass" : "fail"
        );
        (void)puts(
            "Do not add more agent authority until verification passes."
        );
    }
}

void openai_review_file(agent_state *state, const char *path)
{
    static const char introduction[] =
        "Review this OpenVMS C source file. Identify correctness bugs, "
        "OpenVMS portability problems, security issues, and maintainability "
        "problems. Prioritize concrete findings. Do not rewrite the entire "
        "file unless necessary.\n\nFile: ";
    static const char separator[] = "\n\nSource:\n";
    char *source;
    char *prompt;
    size_t prompt_size;

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
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

    if (!openai_path_is_safe(path)) {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    source = openai_read_text_file(path);

    if (source == NULL) {
        return;
    }

    prompt_size =
        strlen(introduction) +
        strlen(path) +
        strlen(separator) +
        strlen(source) +
        1U;

    prompt = malloc(prompt_size);

    if (prompt == NULL) {
        (void)puts("Insufficient memory for review prompt.");
        free(source);
        return;
    }

    (void)strcpy(prompt, introduction);
    (void)strcat(prompt, path);
    (void)strcat(prompt, separator);
    (void)strcat(prompt, source);

    openai_send(state, prompt, 0);

    free(prompt);
    free(source);
}

void openai_ask(agent_state *state, const char *prompt)
{
    openai_send(state, prompt, 0);
}

void openai_chat(agent_state *state, const char *prompt)
{
    openai_send(state, prompt, 1);
}

void openai_chat_reset(void)
{
    previous_response_id[0] = '\0';
    (void)puts("OpenAI conversation reset.");
}

int openai_chat_active(void)
{
    return previous_response_id[0] != '\0';
}

