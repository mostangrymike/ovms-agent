#include <stdarg.h>

#include "llm_internal.h"
#include "ANSI_TERM.H"
#include "cobol_guard.h"

static int m273_tools_diff_kind = -1;

static int m273_tools_puts(const char *text)
{
    if (text != NULL && strcmp(text, "Current:") == 0) {
        ansi_term_puts(text);
        m273_tools_diff_kind = ANSI_DIFF_DELETE;
        return 0;
    }

    if (text != NULL && strcmp(text, "\nProposed:") == 0) {
        ansi_term_puts(text);
        m273_tools_diff_kind = ANSI_DIFF_ADD;
        return 0;
    }

    m273_tools_diff_kind = -1;
    ansi_term_puts(text);
    return 0;
}

static int m273_tools_printf(const char *format, ...)
{
    va_list arguments;

    m273_tools_diff_kind = -1;
    va_start(arguments, format);
    ansi_term_vprintf(format, arguments);
    va_end(arguments);
    return 0;
}

static int m273_tools_fputs(const char *text, FILE *stream)
{
    if (stream != stdout) {
        return fputs(text, stream);
    }

    if (m273_tools_diff_kind >= 0) {
        ansi_term_diff(m273_tools_diff_kind, text);
    } else {
        ansi_term_write(text);
    }

    return 0;
}

static size_t m273_tools_fwrite(const void *buffer,
                                size_t size,
                                size_t count,
                                FILE *stream)
{
    size_t length;

    if (stream != stdout) {
        return fwrite(buffer, size, count, stream);
    }

    if (buffer == NULL || size == 0U || count == 0U) {
        return count;
    }

    if (size > ((size_t)-1) / count) {
        return 0U;
    }

    length = size * count;
    if (m273_tools_diff_kind >= 0) {
        ansi_term_diff_n(
            m273_tools_diff_kind,
            (const char *)buffer,
            length
        );
    } else {
        ansi_term_write_n((const char *)buffer, length);
    }

    return count;
}

static int m273_tools_putchar(int character)
{
    char text[1];

    text[0] = (char)character;
    if (m273_tools_diff_kind >= 0) {
        ansi_term_diff_n(m273_tools_diff_kind, text, 1U);
    } else {
        ansi_term_write_n(text, 1U);
    }
    return character;
}

static int m273_tools_fflush(FILE *stream)
{
    if (stream == stdout) {
        ansi_term_flush();
        return 0;
    }

    return fflush(stream);
}

#define puts m273_tools_puts
#define printf m273_tools_printf
#define fputs m273_tools_fputs
#define fwrite m273_tools_fwrite
#define putchar m273_tools_putchar
#define fflush m273_tools_fflush
#define execute_replace_text_tool m263_rep_text
#define execute_replace_lines_tool m263_rep_lines
#include "LLM_TOOLS_CORE.C"
#undef execute_replace_text_tool
#undef execute_replace_lines_tool
#undef fflush
#undef putchar
#undef fwrite
#undef fputs
#undef printf
#undef puts

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
        *old_text == '\0' || !llm_path_is_safe(path) ||
        llm_path_is_sensitive(path)) {
        free(path);
        free(old_text);
        free(new_text);
        return 1;
    }

    original = llm_read_text_file(path);
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
        !llm_path_is_safe(path) || llm_path_is_sensitive(path)) {
        free(path);
        free(new_text);
        return NULL;
    }

    content = llm_read_text_file(path);
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
    original = llm_read_text_file(path);
    if (original == NULL) return 1;
    safe = cobol_edit_safe(path, original, candidate,
                           reason, sizeof(reason));
    free(original);
    if (!safe) {
        (void)printf("COBOL structural edit refused: %s\n", reason);
    }
    return safe;
}

llm_replace_result execute_replace_text_tool(
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    if (!m264_guard_text(arguments)) return LLM_REPLACE_ERROR;
    return m263_rep_text(state, arguments, display_path);
}

llm_replace_result execute_replace_lines_tool(
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
    if (!safe) return LLM_REPLACE_ERROR;
    return m263_rep_lines(state, arguments, display_path);
}
