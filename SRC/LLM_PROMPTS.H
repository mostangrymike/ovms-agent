#ifndef LLM_PROMPTS_H
#define LLM_PROMPTS_H

const char *llm_prompt_read_only(void);
const char *llm_prompt_plan(void);
const char *llm_prompt_write(void);
const char *llm_prompt_create(void);

/*
 * The agent loop includes the internal header before this header. Keep the
 * supervised-repair guard file-local and intercept only write calls made by
 * that loop.
 */
#if defined(LLM_INTERNAL_H)
#include "LLM_GOAL_GUARD.INC"

static int llm_guard_write_kind(
    const llm_tool_descriptor *descriptor,
    int build_after_write)
{
    if (llm_tool_is_replace(descriptor)) {
        return 1;
    }

    return !build_after_write &&
           descriptor != NULL &&
           descriptor->kind == LLM_TOOL_CREATE_FILE;
}

static int llm_guard_allow_write(
    const llm_tool_descriptor *descriptor,
    int build_after_write)
{
    if (!build_after_write &&
        descriptor != NULL &&
        descriptor->kind == LLM_TOOL_CREATE_FILE) {
        return llm_auto_allow_create();
    }

    return llm_auto_allow_write();
}

static llm_replace_result llm_guard_rep_text(
    const char *goal_text,
    int enforce,
    const llm_tool_descriptor *descriptor,
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    char reason[256];

    if (!enforce &&
        descriptor != NULL &&
        descriptor->kind == LLM_TOOL_CREATE_FILE) {
        llm_create_result create_result;

        (void)state;
        create_result = execute_create_file_tool(
            arguments,
            display_path
        );

        if (create_result == LLM_CREATE_CREATED) {
            return LLM_REPLACE_APPLIED;
        }
        if (create_result == LLM_CREATE_DECLINED) {
            return LLM_REPLACE_DECLINED;
        }
        return LLM_REPLACE_ERROR;
    }

    if (enforce &&
        !goal_guard_args_ok(
            goal_text,
            arguments,
            0,
            reason,
            sizeof(reason))) {
        char *path;

        path = extract_string_argument(arguments, "path");
        if (display_path != NULL) {
            *display_path = path != NULL ?
                llm_duplicate_text(path) : NULL;
        }
        free(path);

        (void)printf(
            "Goal preservation check rejected patch: %s\n",
            reason[0] != '\0' ? reason :
                "the proposed replacement removes a required goal term."
        );
        return LLM_REPLACE_ERROR;
    }

    return execute_replace_text_tool(state, arguments, display_path);
}

static llm_replace_result llm_guard_rep_lines(
    const char *goal_text,
    int enforce,
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    char reason[256];

    if (enforce &&
        !goal_guard_args_ok(
            goal_text,
            arguments,
            1,
            reason,
            sizeof(reason))) {
        char *path;

        path = extract_string_argument(arguments, "path");
        if (display_path != NULL) {
            *display_path = path != NULL ?
                llm_duplicate_text(path) : NULL;
        }
        free(path);

        (void)printf(
            "Goal preservation check rejected patch: %s\n",
            reason[0] != '\0' ? reason :
                "the proposed replacement removes a required goal term."
        );
        return LLM_REPLACE_ERROR;
    }

    return execute_replace_lines_tool(state, arguments, display_path);
}

#define llm_tool_is_replace(descriptor) \
    llm_guard_write_kind(descriptor, build_after_write)

#define llm_auto_allow_write() \
    llm_guard_allow_write(descriptor, build_after_write)

#define execute_replace_text_tool(state, arguments, display_path) \
    llm_guard_rep_text( \
        goal, build_after_write, descriptor, state, arguments, display_path)

#define execute_replace_lines_tool(state, arguments, display_path) \
    llm_guard_rep_lines( \
        goal, build_after_write, state, arguments, display_path)
#endif

#endif /* LLM_PROMPTS_H */
