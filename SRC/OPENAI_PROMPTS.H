#ifndef OPENAI_PROMPTS_H
#define OPENAI_PROMPTS_H

const char *openai_prompt_read_only(void);
const char *openai_prompt_plan(void);
const char *openai_prompt_write(void);
const char *openai_prompt_create(void);

/*
 * The agent loop includes the internal header before this header. Keep the
 * supervised-repair guard file-local and intercept only replacement calls
 * made by that loop. During M267, accept either the legacy compatibility
 * guard or the provider-neutral LLM internal guard.
 */
#if defined(OPENAI_INTERNAL_H) || defined(LLM_INTERNAL_H)
#include "OPENAI_GOAL_GUARD.INC"

static openai_replace_result openai_guard_rep_text(
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
                openai_duplicate_text(path) : NULL;
        }
        free(path);

        (void)printf(
            "Goal preservation check rejected patch: %s\n",
            reason[0] != '\0' ? reason :
                "the proposed replacement removes a required goal term."
        );
        return OPENAI_REPLACE_ERROR;
    }

    return execute_replace_text_tool(state, arguments, display_path);
}

static openai_replace_result openai_guard_rep_lines(
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
                openai_duplicate_text(path) : NULL;
        }
        free(path);

        (void)printf(
            "Goal preservation check rejected patch: %s\n",
            reason[0] != '\0' ? reason :
                "the proposed replacement removes a required goal term."
        );
        return OPENAI_REPLACE_ERROR;
    }

    return execute_replace_lines_tool(state, arguments, display_path);
}

#define execute_replace_text_tool(state, arguments, display_path) \
    openai_guard_rep_text( \
        goal, build_after_write, state, arguments, display_path)

#define execute_replace_lines_tool(state, arguments, display_path) \
    openai_guard_rep_lines( \
        goal, build_after_write, state, arguments, display_path)
#endif

#endif /* OPENAI_PROMPTS_H */
