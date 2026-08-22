#include "llm_internal.h"

#define LLM_CONTEXT_MAX 8192U
#define LLM_CONTEXT_META 2048U
#define LLM_CONTEXT_HIST 32768U
#define LLM_CONTEXT_TAIL 2400U
#define LLM_CONTEXT_PARENT 1800U
#define LLM_CONTEXT_RESULTS 1800U
#define LLM_CONTEXT_PARENT_RES 1200U

static int llm_context_append(char *output,
                                 size_t output_size,
                                 size_t *used,
                                 const char *text)
{
    size_t available;
    size_t length;

    if (output == NULL || used == NULL ||
        text == NULL || *used >= output_size) {
        return 0;
    }

    available = output_size - *used - 1U;
    length = strlen(text);

    if (length > available) {
        length = available;
    }

    if (length > 0U) {
        (void)memcpy(output + *used, text, length);
        *used += length;
    }

    output[*used] = '\0';
    return 1;
}

static const char *llm_context_tail(const char *text,
                                       size_t maximum)
{
    size_t length;
    const char *start;

    if (text == NULL) {
        return "";
    }

    length = strlen(text);

    if (length <= maximum) {
        return text;
    }

    start = text + (length - maximum);

    while (*start != '\0' && *start != '\n') {
        ++start;
    }

    if (*start == '\n') {
        ++start;
    }

    return start;
}

int llm_context_evidence_text(const char *session,
                                 char *output,
                                 size_t output_size)
{
    char recent[LLM_CONTEXT_HIST];
    char last[LLM_CONTEXT_HIST];
    const char *tail;
    size_t used;

    if (session == NULL || strlen(session) != 8U ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    used = 0U;

    if (openai_session_result_last(
            session, last, sizeof(last))) {
        (void)llm_context_append(
            output, output_size, &used,
            "LATEST NORMALIZED RESULT\n");
        tail = llm_context_tail(
            last, LLM_CONTEXT_PARENT_RES);
        (void)llm_context_append(
            output, output_size, &used, tail);
    }

    if (openai_session_results_text(
            session, recent, sizeof(recent))) {
        (void)llm_context_append(
            output, output_size, &used,
            "RECENT NORMALIZED RESULTS\n");
        tail = llm_context_tail(
            recent, LLM_CONTEXT_RESULTS);
        (void)llm_context_append(
            output, output_size, &used, tail);
    }

    return output[0] != '\0';
}

int llm_context_build(const char *goal,
                         char *output,
                         size_t output_size)
{
    char session[9];
    char parent[9];
    char meta[LLM_CONTEXT_META];
    char history[LLM_CONTEXT_HIST];
    char parent_history[LLM_CONTEXT_HIST];
    char results[LLM_CONTEXT_HIST];
    char parent_results[LLM_CONTEXT_HIST];
    char lang_policy[4096];
    const char *tail;
    size_t used;

    if (goal == NULL || *goal == '\0' ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    used = 0U;

    if (!openai_session_current_id(session)) {
        if (openai_lang_policy_text(
                lang_policy, sizeof(lang_policy))) {
            (void)llm_context_append(
                output, output_size, &used, lang_policy);
            (void)llm_context_append(
                output, output_size, &used, "\nCURRENT REQUEST\n");
        }
        return llm_context_append(
            output, output_size, &used, goal
        );
    }

    if (!openai_session_show_text(
            session, meta, sizeof(meta))) {
        return 0;
    }

    if (!openai_session_hist_text(
            session, history, sizeof(history))) {
        history[0] = '\0';
    }

    (void)llm_context_append(
        output, output_size, &used,
        "Persistent OVMS Agent session context follows.\n"
        "Treat it as historical context for continuity. "
        "The CURRENT REQUEST at the end is authoritative.\n\n"
        "SESSION METADATA\n"
    );
    (void)llm_context_append(
        output, output_size, &used, meta
    );

    if (openai_session_parent(session, parent) &&
        strcmp(parent, "-") != 0 &&
        strlen(parent) == 8U &&
        openai_session_hist_text(
            parent, parent_history, sizeof(parent_history))) {
        (void)llm_context_append(
            output, output_size, &used,
            "\nPARENT SESSION RECENT TRANSCRIPT\n"
        );
        tail = llm_context_tail(
            parent_history, LLM_CONTEXT_PARENT
        );
        (void)llm_context_append(
            output, output_size, &used, tail
        );

        if (llm_context_evidence_text(
                parent, parent_results, sizeof(parent_results))) {
            (void)llm_context_append(
                output, output_size, &used,
                "\nPARENT SESSION PRIORITIZED TOOL EVIDENCE\n"
            );
            tail = llm_context_tail(
                parent_results, LLM_CONTEXT_PARENT_RES
            );
            (void)llm_context_append(
                output, output_size, &used, tail
            );
        }
    }

    if (history[0] != '\0') {
        (void)llm_context_append(
            output, output_size, &used,
            "\nCURRENT SESSION RECENT TRANSCRIPT\n"
        );
        tail = llm_context_tail(
            history, LLM_CONTEXT_TAIL
        );
        (void)llm_context_append(
            output, output_size, &used, tail
        );
    }

    if (llm_context_evidence_text(
            session, results, sizeof(results))) {
        (void)llm_context_append(
            output, output_size, &used,
            "\nCURRENT SESSION PRIORITIZED TOOL EVIDENCE\n"
        );
        (void)llm_context_append(
            output, output_size, &used, results
        );
    }

    if (openai_lang_policy_text(
            lang_policy, sizeof(lang_policy))) {
        (void)llm_context_append(
            output, output_size, &used,
            "\nMULTILINGUAL LANGUAGE POLICY\n");
        (void)llm_context_append(
            output, output_size, &used, lang_policy);
    }

    (void)llm_context_append(
        output, output_size, &used,
        "\nCURRENT REQUEST\n"
    );
    (void)llm_context_append(
        output, output_size, &used, goal
    );

    return 1;
}

int llm_context_current(char *output,
                           size_t output_size)
{
    char session[9];
    char meta[LLM_CONTEXT_META];
    char history[LLM_CONTEXT_HIST];
    char results[LLM_CONTEXT_HIST];
    const char *tail;
    size_t used;

    if (output == NULL || output_size == 0U ||
        !openai_session_current_id(session) ||
        !openai_session_show_text(
            session, meta, sizeof(meta))) {
        return 0;
    }

    output[0] = '\0';
    used = 0U;

    (void)llm_context_append(
        output, output_size, &used,
        "OVMS Agent current continuation context\n"
        "---------------------------------------\n"
    );
    (void)llm_context_append(
        output, output_size, &used, meta
    );

    {
        char instructions[4352];

        if (llm_instr_show_text(
                NULL, instructions, sizeof(instructions))) {
            (void)llm_context_append(
                output, output_size, &used,
                "\nProject instructions:\n"
            );
            (void)llm_context_append(
                output, output_size, &used,
                instructions
            );
        }
    }

    if (openai_session_hist_text(
            session, history, sizeof(history))) {
        (void)llm_context_append(
            output, output_size, &used,
            "\nRecent transcript:\n"
        );
        tail = llm_context_tail(
            history, LLM_CONTEXT_TAIL
        );
        (void)llm_context_append(
            output, output_size, &used, tail
        );
    }

    if (llm_context_evidence_text(
            session, results, sizeof(results))) {
        (void)llm_context_append(
            output, output_size, &used,
            "\nPrioritized normalized tool evidence:\n"
        );
        (void)llm_context_append(
            output, output_size, &used, results
        );
    }

    return 1;
}

void llm_show_context_current(void)
{
    char output[LLM_CONTEXT_MAX];

    if (!llm_context_current(
            output, sizeof(output))) {
        (void)puts(
            "No current persistent session context."
        );
        return;
    }

    (void)fputs(output, stdout);
}
