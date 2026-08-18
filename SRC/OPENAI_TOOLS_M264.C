#include "cobol_guard.h"

#define execute_replace_text_tool m263_rep_text
#define execute_replace_lines_tool m263_rep_lines
#include "OPENAI_TOOLS.C"
#undef execute_replace_text_tool
#undef execute_replace_lines_tool

static int m264_guard_text(const char *arguments)
{
    char *path;
    char *old_text;
    char *new_text;
    char *original;
    char reason[256];
    int safe;

    path = extract_string_argument(arguments, "path");
    old_text = extract_string_argument(arguments, "old_text");
    new_text = extract_string_argument(arguments, "new_text");
    if (path == NULL || old_text == NULL || new_text == NULL ||
        *old_text == '\0' || !openai_path_is_safe(path) ||
        openai_path_is_sensitive(path)) {
        free(path);
        free(old_text);
        free(new_text);
        return 1;
    }

    original = openai_read_text_file(path);
    if (original == NULL) {
        free(path);
        free(old_text);
        free(new_text);
        return 1;
    }

    safe = cobol_text_safe(path, original, old_text, new_text,
                           reason, sizeof(reason));
    free(original);
    free(path);
    free(old_text);
    free(new_text);
    if (!safe) {
        (void)printf("COBOL structural edit refused: %s\n", reason);
    }
    return safe;
}

static char *m264_lines_candidate(const char *arguments,
                                  char **path_out)
{
    char *path;
    char *new_text;
    char *content;
    char *result;
    long first_line;
    long last_line;
    long line_number;
    size_t actual;
    size_t position;
    size_t start_offset;
    size_t end_offset;
    size_t replacement_len;
    size_t result_len;

    *path_out = NULL;
    path = extract_string_argument(arguments, "path");
    new_text = extract_string_argument(arguments, "new_text");
    if (path == NULL || new_text == NULL ||
        !extract_integer_argument(arguments, "first_line", &first_line) ||
        !extract_integer_argument(arguments, "last_line", &last_line) ||
        first_line < 1L || last_line < first_line ||
        last_line - first_line > 2000L ||
        !openai_path_is_safe(path) || openai_path_is_sensitive(path)) {
        free(path);
        free(new_text);
        return NULL;
    }

    content = openai_read_text_file(path);
    if (content == NULL) {
        free(path);
        free(new_text);
        return NULL;
    }
    actual = strlen(content);
    line_number = 1L;
    start_offset = 0U;
    for (position = 0U;
         position < actual && line_number < first_line;
         ++position) {
        if (content[position] == '\n') {
            ++line_number;
            start_offset = position + 1U;
        }
    }
    if (line_number != first_line || start_offset > actual) {
        free(content);
        free(path);
        free(new_text);
        return NULL;
    }

    end_offset = start_offset;
    while (end_offset < actual && line_number <= last_line) {
        if (content[end_offset++] == '\n') {
            if (line_number == last_line) break;
            ++line_number;
        }
    }
    if (line_number < last_line ||
        (line_number == last_line && end_offset == start_offset)) {
        free(content);
        free(path);
        free(new_text);
        return NULL;
    }

    replacement_len = strlen(new_text);
    result_len = start_offset + replacement_len +
        (actual - end_offset);
    result = (char *)malloc(result_len + 1U);
    if (result != NULL) {
        if (start_offset > 0U) {
            (void)memcpy(result, content, start_offset);
        }
        if (replacement_len > 0U) {
            (void)memcpy(result + start_offset,
                         new_text, replacement_len);
        }
        if (end_offset < actual) {
            (void)memcpy(result + start_offset + replacement_len,
                         content + end_offset, actual - end_offset);
        }
        result[result_len] = '\0';
        *path_out = path;
        path = NULL;
    }

    free(content);
    free(path);
    free(new_text);
    return result;
}

static int m264_guard_candidate(const char *path,
                                const char *candidate)
{
    char *original;
    char reason[256];
    int safe;

    if (path == NULL || candidate == NULL) return 1;
    original = openai_read_text_file(path);
    if (original == NULL) return 1;
    safe = cobol_edit_safe(path, original, candidate,
                           reason, sizeof(reason));
    free(original);
    if (!safe) {
        (void)printf("COBOL structural edit refused: %s\n", reason);
    }
    return safe;
}

openai_replace_result execute_replace_text_tool(
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    if (!m264_guard_text(arguments)) return OPENAI_REPLACE_ERROR;
    return m263_rep_text(state, arguments, display_path);
}

openai_replace_result execute_replace_lines_tool(
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    char *path;
    char *candidate;
    int safe;

    path = NULL;
    candidate = m264_lines_candidate(arguments, &path);
    safe = m264_guard_candidate(path, candidate);
    free(candidate);
    free(path);
    if (!safe) return OPENAI_REPLACE_ERROR;
    return m263_rep_lines(state, arguments, display_path);
}
