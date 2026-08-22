#include "llm_internal.h"
#include "LLM_PROMPTS.H"
#include "LLM_EXECUTE.H"

const char *llm_workflow_name(int workflow)
{
    switch (workflow) {
    case LLM_WORKFLOW_ASK:
        return "ASK";
    case LLM_WORKFLOW_CHAT:
        return "CHAT";
    case LLM_WORKFLOW_REVIEW:
        return "REVIEW";
    case LLM_WORKFLOW_AGENT:
        return "AGENT";
    case LLM_WORKFLOW_WRITE:
        return "AGENT/WRITE";
    case LLM_WORKFLOW_FIX:
        return "AGENT/FIX";
    case LLM_WORKFLOW_BUILD:
        return "AGENT/BUILD";
    case LLM_WORKFLOW_RETRY:
        return "AGENT/RETRY";
    case LLM_WORKFLOW_SELFTEST:
        return "AGENT/SELFTEST";
    case LLM_WORKFLOW_VERIFY:
        return "AGENT/VERIFY";
    case LLM_WORKFLOW_CREATE:
        return "AGENT/CREATE";
    case LLM_WORKFLOW_PLAN:
        return "AGENT/PLAN";
    case LLM_WORKFLOW_EXECUTE:
        return "AGENT/EXECUTE";
    default:
        return "none";
    }
}

const char *llm_rollback_name(int rollback_state)
{
    switch (rollback_state) {
    case LLM_ROLLBACK_NOT_NEEDED:
        return "not needed";
    case LLM_ROLLBACK_SUCCEEDED:
        return "succeeded";
    case LLM_ROLLBACK_FAILED:
        return "failed";
    case LLM_ROLLBACK_DECLINED:
        return "declined";
    default:
        return "none recorded";
    }
}

void llm_status(const agent_state *state)
{
    const char *model;
    const char *api_key;

    llm_load_state();

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
        "Provider API key defined:   %s\n",
        api_key != NULL && *api_key != '\0' ?
            "yes" : "no"
    );

    (void)printf(
        "LLM model:             %s\n",
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
        llm_persist_workflow(
            llm_state_valid,
            llm_saved_workflow)
    );

    if (!llm_state_valid) {
        (void)puts(
            "Last persisted build:     unavailable"
        );
    } else if (llm_saved_build_known) {
        (void)printf(
            "Last persisted build:     %s (status %d)\n",
            (llm_saved_build_status & 1) != 0 ?
                "success" : "failure",
            llm_saved_build_status
        );
    } else {
        (void)puts(
            "Last persisted build:     not recorded"
        );
    }

    (void)printf(
        "Last persisted rollback:  %s\n",
        llm_persist_rollback(
            llm_state_valid,
            llm_saved_rollback)
    );

    (void)printf(
        "Persistent state file:    %s\n",
        LLM_STATE_FILE
    );

    (void)printf(
        "Persistent state valid:   %s\n",
        llm_state_valid ? "yes" : "no"
    );

    (void)printf(
        "Persistent state source:  %s\n",
        llm_state_source_name(
            llm_state_valid,
            llm_state_recovered)
    );

    (void)printf(
        "Last state save attempt: %s\n",
        llm_state_save_name(
            llm_state_save_known,
            llm_state_save_succeeded)
    );

    llm_plan_approval_report();
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

static void llm_selftest_report(const char *name,
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

#include "LLM_SELFTEST_FILE_EXISTS.INC"

unsigned int llm_run_selftest(agent_state *state)
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
    const llm_tool_descriptor *read_descriptor;
    const llm_tool_descriptor *replace_descriptor;
    const llm_tool_descriptor *build_descriptor;
    llm_file_cache_entry test_cache[LLM_AGENT_CACHE_SIZE];
    const char *cached_text;
    char *malformed_text;
    long integer_value;
    unsigned int passed_count;
    unsigned int failed_count;

    passed_count = 0U;
    failed_count = 0U;

    (void)puts("OVMS Agent non-destructive self-test");
    (void)puts("------------------------------------");

    llm_selftest_report(
        "project state available",
        state != NULL &&
        state->project_root != NULL &&
        *state->project_root != '\0',
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "safe project path accepted",
        llm_path_is_safe("SRC/MAIN.C"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "parent traversal rejected",
        !llm_path_is_safe("../LOGIN.COM"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "device-qualified path rejected",
        !llm_path_is_safe("SYS$LOGIN:LOGIN.COM"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "API-key filename blocked",
        llm_path_is_sensitive("OPENAIKEY.TXT"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "header filename blocked",
        llm_path_is_sensitive("OVMS_AGENT_HEADERS.TXT"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "ordinary source not sensitive",
        !llm_path_is_sensitive("SRC/LLM.C"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "lowercase key filename blocked",
        llm_path_is_sensitive("config/server.key"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "object listing entry hidden",
        llm_listing_entry_hidden("LLM_AGENT.OBJ"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "backup listing entries hidden",
        llm_listing_entry_hidden(
            "LLM_TOOLS.C_M251_14_BACKUP") &&
        llm_listing_entry_hidden(
            "LLM_STATUS_BEFORE_M96_REPAIR.C") &&
        llm_listing_entry_hidden("LEGACY.C.BAK") &&
        llm_listing_entry_hidden("LEGACY.C.OLD") &&
        !llm_listing_entry_hidden("LLM_TOOLS.C"),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "truncated listing guidance present",
        strstr(llm_prompt_read_only(),
               "do not infer that an unlisted path is absent") != NULL,
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "live source preference guidance present",
        strstr(llm_prompt_read_only(),
               "prefer live source over backup") != NULL &&
        strstr(llm_prompt_read_only(),
               "user explicitly asks to inspect") != NULL &&
        strstr(llm_prompt_read_only(),
               "does not make it preferred evidence") != NULL,
        &passed_count,
        &failed_count
    );

    read_descriptor = llm_tool_find("read_file");
    replace_descriptor = llm_tool_find("replace_text");
    build_descriptor = llm_tool_find("run_build");

    llm_selftest_report(
        "read tool registry metadata",
        read_descriptor != NULL &&
        read_descriptor->kind == LLM_TOOL_READ_FILE &&
        read_descriptor->allows_read &&
        !read_descriptor->allows_write &&
        !read_descriptor->requires_confirmation,
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "write tool registry metadata",
        replace_descriptor != NULL &&
        replace_descriptor->kind == LLM_TOOL_REPLACE_TEXT &&
        !replace_descriptor->allows_read &&
        replace_descriptor->allows_write &&
        replace_descriptor->requires_confirmation &&
        llm_tool_is_replace(replace_descriptor),
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "build tool registry metadata",
        build_descriptor != NULL &&
        build_descriptor->kind == LLM_TOOL_RUN_BUILD &&
        build_descriptor->requires_build,
        &passed_count,
        &failed_count
    );

    llm_selftest_report(
        "unknown tool rejected",
        llm_tool_find("not_a_real_tool") == NULL,
        &passed_count,
        &failed_count
    );

    llm_cache_init(test_cache);
    llm_selftest_report(
        "case-insensitive cache lookup",
        llm_cache_store(
            test_cache,
            "SRC/SELFTEST.C",
            "cached self-test text") &&
        (cached_text = llm_cache_lookup(
            test_cache,
            "src/selftest.c")) != NULL &&
        strcmp(cached_text, "cached self-test text") == 0,
        &passed_count,
        &failed_count
    );
    llm_cache_free(test_cache);

    malformed_text = extract_output_text_from_json(malformed_json);
    llm_selftest_report(
        "malformed JSON rejected",
        malformed_text == NULL,
        &passed_count,
        &failed_count
    );
    free(malformed_text);

    integer_value = 0L;
    llm_selftest_report(
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

    llm_selftest_report(
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

    llm_selftest_report(
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

        llm_selftest_report(
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
        llm_file_cache_entry large_cache[LLM_AGENT_CACHE_SIZE];
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

        llm_cache_init(large_cache);
        large_display = NULL;
        large_cache_hit = 0;
        large_output = large_created ?
            execute_read_file_tool(
                "{\"path\":\"M251_14_LARGE.TMP\"}",
                large_cache,
                &large_cache_hit,
                &large_display
            ) : NULL;

        llm_selftest_report(
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
        llm_cache_free(large_cache);
        (void)remove(large_path);
    }

    build_file = fopen("BUILD.COM", "r");

    llm_selftest_report(
        "fixed BUILD.COM is readable",
        build_file != NULL,
        &passed_count,
        &failed_count
    );

    if (build_file != NULL) {
        (void)fclose(build_file);
    }
    #include "LLM_TEST_OPERATION_COUNT.INC"
    #include "LLM_TEST_MISSING_END.INC"
    #include "LLM_TEST_BAD_TYPE.INC"
    #include "LLM_TEST_DUP_TYPE.INC"
    #include "LLM_TEST_DUP_FIELD_PATH.INC"
    #include "LLM_TEST_BLOCK_BEFORE_TYPE.INC"
    #include "LLM_TEST_PATH_BEFORE_TYPE.INC"
    #include "LLM_TEST_NEW_BEFORE_OLD.INC"
    #include "LLM_TEST_PATH_AFTER_OLD.INC"
    #include "LLM_TEST_TYPE_AFTER_OLD.INC"
    #include "LLM_TEST_FIELD_AFTER_NEW.INC"
    #include "LLM_TEST_NESTED_BEGIN.INC"
    #include "LLM_TEST_SECOND_COUNT.INC"
    #include "LLM_TEST_COUNT_AFTER_BEGIN.INC"
    #include "LLM_TEST_FIELD_BEFORE_BEGIN.INC"
    #include "LLM_TEST_BLOCK_OUTSIDE.INC"
    #include "LLM_TEST_TRAILING_SYNTAX.INC"
    #include "LLM_TEST_PROSE_ONLY_OPS.INC"
    #include "LLM_TEST_LEGACY_WITH_BLOCK.INC"
    #include "LLM_TEST_BLOCK_WITH_TEXT.INC"
    #include "LLM_TEST_UNEXPECTED_FIELD.INC"
    #include "LLM_TEST_UNEXPECTED_TOPLEVEL.INC"
    #include "LLM_TEST_VALID_PREAMBLE.INC"
    #include "LLM_TEST_BLANK_BETWEEN_OPS.INC"
    #include "LLM_TEST_TWO_VALID_OPS.INC"
    #include "LLM_TEST_M103.INC"
    #include "LLM_TEST_M106.INC"
    #include "LLM_TEST_M107.INC"
    #include "LLM_TEST_M108.INC"
    #include "LLM_TEST_M109.INC"
    #include "LLM_TEST_M110.INC"
    #include "LLM_TEST_M111.INC"
    #include "LLM_TEST_M112.INC"
    #include "LLM_TEST_M113.INC"
    #include "LLM_TEST_M114.INC"
    #include "LLM_TEST_M115.INC"
    #include "LLM_TEST_M116.INC"
    #include "LLM_TEST_M117.INC"
    #include "LLM_TEST_M118.INC"
    #include "LLM_TEST_M119.INC"
    #include "LLM_TEST_M120.INC"
    #include "LLM_TEST_M121.INC"
    #include "LLM_TEST_M122.INC"
    #include "LLM_TEST_M123.INC"
    #include "LLM_TEST_M124.INC"
    #include "LLM_TEST_M125.INC"
    #include "LLM_TEST_M126.INC"
    #include "LLM_TEST_M127.INC"
    #include "LLM_TEST_M128.INC"
    #include "LLM_TEST_M129.INC"
    #include "LLM_TEST_M130.INC"
    #include "LLM_TEST_M131.INC"
    #include "LLM_TEST_M132.INC"
    #include "LLM_TEST_M133.INC"
    #include "LLM_TEST_M134.INC"
    #include "LLM_TEST_M135.INC"
    #include "LLM_TEST_M136.INC"
    #include "LLM_TEST_M137.INC"
    #include "LLM_TEST_M138.INC"
    #include "LLM_TEST_M139.INC"
    #include "LLM_TEST_M140.INC"
    #include "LLM_TEST_M141.INC"
    #include "LLM_TEST_M142.INC"
    #include "LLM_TEST_M143.INC"
    #include "LLM_TEST_M144.INC"
    #include "LLM_TEST_M145.INC"
    #include "LLM_TEST_M146.INC"
    #include "LLM_TEST_M147.INC"
    #include "LLM_TEST_M148.INC"
    #include "LLM_TEST_M149.INC"
    #include "LLM_TEST_M150.INC"
    #include "LLM_TEST_M150B.INC"
    #include "LLM_TEST_M150C.INC"
    #include "LLM_TEST_M150D.INC"
    #include "LLM_TEST_BLOCK_BEFORE_PATH.INC"
    #include "LLM_TEST_MULTILINE.INC"
    #include "LLM_TEST_DUP_PATH.INC"
    #include "LLM_TEST_SAME_FILE_CHAIN.INC"
    #include "LLM_TEST_DRY_STAGE.INC"
    #include "LLM_TEST_DRY_UNCHANGED.INC"
    #include "LLM_TEST_DRY_STALE.INC"
    #include "LLM_TEST_DRY_MISSING.INC"
    #include "LLM_TEST_DRY_NONUNIQUE.INC"
    #include "LLM_TEST_FAILED_SAME_FILE_CHAIN.INC"
    #include "LLM_TEST_NONUNIQUE_CHAIN.INC"
    #include "LLM_TEST_OVERSIZED_COUNT.INC"
    #include "LLM_TEST_MULTILINE_BLOCK.INC"
    #include "LLM_TEST_BLOCK_MISSING_OLD_END.INC"
    #include "LLM_TEST_BLOCK_MISSING_NEW_END.INC"
    #include "LLM_TEST_BLOCK_EMPTY_OLD.INC"
    #include "LLM_TEST_BLOCK_WITH_LEGACY_TYPE.INC"
    #include "LLM_TEST_BLOCK_MIXED_LEGACY.INC"
    #include "LLM_TEST_BLOCK_ESCAPED_END.INC"
    #include "LLM_TEST_BLOCK_EMPTY_NEW.INC"

    (void)puts("------------------------------------");
    (void)printf(
        "Self-test result: %u passed, %u failed.\n",
        passed_count,
        failed_count
    );

    llm_log_event(
        llm_workflow_name(llm_last_workflow),
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

void llm_selftest(agent_state *state)
{
    llm_last_workflow = LLM_WORKFLOW_SELFTEST;
    llm_log_event(
        llm_workflow_name(llm_last_workflow),
        "start",
        0
    );
    (void)llm_run_selftest(state);
}

void llm_verify(agent_state *state)
{
    unsigned int selftest_failures;
    char *build_output;
    int build_status;
    int build_passed;

    llm_last_workflow = LLM_WORKFLOW_VERIFY;
    llm_log_event(
        llm_workflow_name(llm_last_workflow),
        "start",
        0
    );
    llm_last_rollback = LLM_ROLLBACK_NONE;

    (void)puts("OVMS Agent verification gate");
    (void)puts("============================");

    selftest_failures = llm_run_selftest(state);

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

    llm_log_event(
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

void llm_review_file(agent_state *state, const char *path)
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

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
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

    if (!llm_path_is_safe(path)) {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    source = llm_read_text_file(path);

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

    llm_send(state, prompt, 0);

    free(prompt);
    free(source);
}

void llm_ask(agent_state *state, const char *prompt)
{
    llm_send(state, prompt, 0);
}

void llm_chat(agent_state *state, const char *prompt)
{
    llm_send(state, prompt, 1);
}

void llm_chat_reset(void)
{
    previous_response_id[0] = '\0';
    (void)puts("LLM conversation reset.");
}

int llm_chat_active(void)
{
    return previous_response_id[0] != '\0';
}
