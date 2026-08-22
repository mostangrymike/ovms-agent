#ifndef LLM_PROMPTS_H
#define LLM_PROMPTS_H

const char *llm_prompt_read_only(void);
const char *llm_prompt_plan(void);
const char *llm_prompt_write(void);
const char *llm_prompt_create(void);

/*
 * The agent loop includes the internal header before this header. Keep the
 * supervised-repair guard file-local and intercept only replacement calls
 * made by that loop.
 */
#if defined(LLM_INTERNAL_H)
#include "LLM_GOAL_GUARD.INC"

static llm_replace_result llm_guard_rep_text(
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

#define execute_replace_text_tool(state, arguments, display_path) \
    llm_guard_rep_text( \
        goal, build_after_write, state, arguments, display_path)

#define execute_replace_lines_tool(state, arguments, display_path) \
    llm_guard_rep_lines( \
        goal, build_after_write, state, arguments, display_path)
#endif

#endif /* LLM_PROMPTS_H */
