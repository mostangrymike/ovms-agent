#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "llm_internal.h"
#include "M289_BUILD_PROFILE.H"
#include "M289_NATIVE_BUILD.H"
#include "OVMS_STATUS.H"

#define M316_PHASE_MAX 2U
#define M316_CAPTURE_MAX 12288U
#define M316_LINE_MAX 2048U
#define M316_FIELD_MAX 512U

static unsigned long m316_raw_status[M316_PHASE_MAX];
static char m316_raw_output[M316_PHASE_MAX][M316_CAPTURE_MAX];
static unsigned int m316_raw_count;

static int m316_exec_proxy(agent_state *state,
                           const char *command,
                           char *output,
                           size_t output_size,
                           unsigned long *status_out);
static int m316_proc_proxy(agent_state *state,
                           const char *procedure_path,
                           char *output,
                           size_t output_size,
                           unsigned long *status_out);

#define command_dcl_exec m316_exec_proxy
#define command_dcl_exec_procedure m316_proc_proxy
#define m289_build_source m316_build_legacy
#define m289_command_allowed m316_guard_legacy
#include "M289_NATIVE_BUILD.C"
#undef m289_command_allowed
#undef m289_build_source
#undef command_dcl_exec_procedure
#undef command_dcl_exec

typedef struct m316_diag {
    char facility[32];
    char severity[16];
    char ident[64];
    char message[M316_FIELD_MAX];
    char file[M316_FIELD_MAX];
    char raw[M316_LINE_MAX];
    unsigned int line;
    unsigned int column;
    int valid;
} m316_diag;

static void m316_capture_reset(void)
{
    unsigned int index;

    m316_raw_count = 0U;
    for (index = 0U; index < M316_PHASE_MAX; ++index) {
        m316_raw_status[index] = 0UL;
        m316_raw_output[index][0] = '\0';
    }
}

static void m316_capture_store(const char *output, unsigned long status)
{
    unsigned int index;

    if (m316_raw_count >= M316_PHASE_MAX) {
        return;
    }
    index = m316_raw_count++;
    m316_raw_status[index] = status;
    if (output != NULL) {
        (void)strncpy(m316_raw_output[index], output, M316_CAPTURE_MAX - 1U);
        m316_raw_output[index][M316_CAPTURE_MAX - 1U] = '\0';
    }
}

static int m316_exec_proxy(agent_state *state,
                           const char *command,
                           char *output,
                           size_t output_size,
                           unsigned long *status_out)
{
    unsigned long raw_status;
    int executed;

    raw_status = 0UL;
    executed = command_dcl_exec(state, command, output, output_size,
                                &raw_status);
    if (executed) {
        m316_capture_store(output, raw_status);
        *status_out = ovms_status_success(raw_status) ? 1UL : 2UL;
    } else if (status_out != NULL) {
        *status_out = raw_status;
    }
    return executed;
}

static int m316_proc_proxy(agent_state *state,
                           const char *procedure_path,
                           char *output,
                           size_t output_size,
                           unsigned long *status_out)
{
    unsigned long raw_status;
    int executed;

    raw_status = 0UL;
    executed = command_dcl_exec_procedure(state, procedure_path,
                                          output, output_size,
                                          &raw_status);
    if (executed) {
        m316_capture_store(output, raw_status);
        *status_out = ovms_status_success(raw_status) ? 1UL : 2UL;
    } else if (status_out != NULL) {
        *status_out = raw_status;
    }
    return executed;
}

static int m316_suffix_ci(const char *text, const char *suffix)
{
    size_t text_length;
    size_t suffix_length;
    size_t index;

    if (text == NULL || suffix == NULL) {
        return 0;
    }
    text_length = strlen(text);
    suffix_length = strlen(suffix);
    if (suffix_length == 0U || suffix_length > text_length) {
        return 0;
    }
    text += text_length - suffix_length;
    for (index = 0U; index < suffix_length; ++index) {
        if (toupper((unsigned char)text[index]) !=
            toupper((unsigned char)suffix[index])) {
            return 0;
        }
    }
    return 1;
}

static int m316_starts(const char *command, const char *prefix)
{
    size_t length;

    if (command == NULL || prefix == NULL) {
        return 0;
    }
    length = strlen(prefix);
    return strncmp(command, prefix, length) == 0 &&
           (command[length] == '\0' || command[length] == ' ' ||
            command[length] == '\t');
}

static int m316_safe_chars(const char *command)
{
    const unsigned char *cursor;

    if (command == NULL || *command == '\0') {
        return 0;
    }
    cursor = (const unsigned char *)command;
    while (*cursor != (unsigned char)'\0') {
        if (*cursor < 32U || *cursor == 127U ||
            *cursor == (unsigned char)'@' ||
            *cursor == (unsigned char)'|' ||
            *cursor == (unsigned char)'&' ||
            *cursor == (unsigned char)'\'' ||
            *cursor == (unsigned char)'"' ||
            *cursor == (unsigned char)'!' ||
            *cursor == (unsigned char)'<' ||
            *cursor == (unsigned char)'>' ||
            *cursor == (unsigned char)';' ||
            *cursor == (unsigned char)':') {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

int m289_command_allowed(const char *command,
                         int phase,
                         const char *language)
{
    if (language != NULL && m289_equal_ci(language, "C")) {
        if (!m316_safe_chars(command)) {
            return 0;
        }
        if (phase == M289_BUILD_COMPILE) {
            return m316_starts(command, "CC");
        }
        if (phase == M289_BUILD_LINK) {
            return m316_starts(command, "LINK");
        }
        return 0;
    }
    return m316_guard_legacy(command, phase, language);
}

static int m316_copy_range(char *destination,
                           size_t destination_size,
                           const char *start,
                           const char *end)
{
    size_t length;

    if (destination == NULL || destination_size == 0U ||
        start == NULL || end == NULL || end < start) {
        return 0;
    }
    length = (size_t)(end - start);
    if (length == 0U || length >= destination_size) {
        return 0;
    }
    (void)memcpy(destination, start, length);
    destination[length] = '\0';
    return 1;
}

static char *m316_trim(char *text)
{
    char *end;

    if (text == NULL) {
        return NULL;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return text;
}

static int m316_digits(const char *start, const char *end,
                       unsigned int *value_out)
{
    unsigned long value;
    const char *cursor;

    if (start == NULL || end == NULL || end <= start || value_out == NULL) {
        return 0;
    }
    value = 0UL;
    for (cursor = start; cursor < end; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
        value = value * 10UL + (unsigned long)(*cursor - '0');
        if (value > 2147483647UL) {
            return 0;
        }
    }
    *value_out = (unsigned int)value;
    return 1;
}

static const char *m316_prev_colon(const char *start, const char *before)
{
    const char *cursor;

    if (start == NULL || before == NULL || before <= start) {
        return NULL;
    }
    cursor = before;
    while (cursor > start) {
        --cursor;
        if (*cursor == ':') {
            return cursor;
        }
    }
    return NULL;
}

static int m316_embedded_location(char *message, m316_diag *diag)
{
    static const char *markers[] = {
        ": fatal error: ", ": error: ", ": warning: ", ": note: ", NULL
    };
    const char *marker;
    const char *column_colon;
    const char *line_colon;
    const char *message_start;
    size_t message_length;
    unsigned int index;
    unsigned int line_value;
    unsigned int column_value;

    if (message == NULL || diag == NULL) {
        return 0;
    }
    marker = NULL;
    for (index = 0U; markers[index] != NULL; ++index) {
        marker = strstr(message, markers[index]);
        if (marker != NULL) {
            break;
        }
    }
    if (marker == NULL) {
        return 0;
    }
    column_colon = m316_prev_colon(message, marker);
    if (column_colon == NULL ||
        !m316_digits(column_colon + 1, marker, &column_value)) {
        return 0;
    }
    line_colon = m316_prev_colon(message, column_colon);
    if (line_colon == NULL ||
        !m316_digits(line_colon + 1, column_colon, &line_value)) {
        return 0;
    }
    if (!m316_copy_range(diag->file, sizeof(diag->file),
                         message, line_colon)) {
        return 0;
    }
    diag->line = line_value;
    diag->column = column_value;
    message_start = marker + strlen(markers[index]);
    message_length = strlen(message_start);
    if (message_length >= sizeof(diag->message)) {
        message_length = sizeof(diag->message) - 1U;
    }
    (void)memmove(diag->message, message_start, message_length);
    diag->message[message_length] = '\0';
    return 1;
}

static void m316_plain_init(m316_diag *diag,
                            const char *facility,
                            const char *severity,
                            const char *ident,
                            const char *raw)
{
    if (diag == NULL) {
        return;
    }
    (void)memset(diag, 0, sizeof(*diag));
    (void)strncpy(diag->facility, facility, sizeof(diag->facility) - 1U);
    (void)strncpy(diag->severity, severity, sizeof(diag->severity) - 1U);
    (void)strncpy(diag->ident, ident, sizeof(diag->ident) - 1U);
    if (raw != NULL) {
        (void)strncpy(diag->raw, raw, sizeof(diag->raw) - 1U);
    }
    diag->valid = 1;
}

static int m316_parse_java(const char *line, m316_diag *diag)
{
    const char *marker;
    const char *line_colon;
    unsigned int line_value;

    if (line == NULL || diag == NULL) {
        return 0;
    }
    marker = strstr(line, ": error: ");
    if (marker == NULL) {
        return 0;
    }
    line_colon = m316_prev_colon(line, marker);
    if (line_colon == NULL ||
        !m316_digits(line_colon + 1, marker, &line_value)) {
        return 0;
    }
    m316_plain_init(diag, "JAVAC", "E", "ERROR", line);
    if (!m316_copy_range(diag->file, sizeof(diag->file), line, line_colon)) {
        return 0;
    }
    diag->line = line_value;
    (void)strncpy(diag->message, marker + strlen(": error: "),
                  sizeof(diag->message) - 1U);
    diag->message[sizeof(diag->message) - 1U] = '\0';
    return 1;
}

static int m316_parse_perl(const char *line, m316_diag *diag)
{
    const char *prefix;
    const char *line_marker;
    const char *comma;
    unsigned int line_value;

    if (line == NULL || diag == NULL) {
        return 0;
    }
    prefix = "syntax error at ";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return 0;
    }
    line_marker = strstr(line + strlen(prefix), " line ");
    if (line_marker == NULL) {
        return 0;
    }
    comma = strchr(line_marker + strlen(" line "), ',');
    if (comma == NULL ||
        !m316_digits(line_marker + strlen(" line "), comma, &line_value)) {
        return 0;
    }
    m316_plain_init(diag, "PERL", "E", "SYNTAX", line);
    if (!m316_copy_range(diag->file, sizeof(diag->file),
                         line + strlen(prefix), line_marker)) {
        return 0;
    }
    diag->line = line_value;
    (void)strncpy(diag->message, line, sizeof(diag->message) - 1U);
    diag->message[sizeof(diag->message) - 1U] = '\0';
    return 1;
}

static int m316_parse_py_file(const char *line, m316_diag *diag)
{
    const char *prefix;
    const char *quote;
    const char *line_marker;
    const char *end;
    unsigned int line_value;

    if (line == NULL || diag == NULL) {
        return 0;
    }
    prefix = "File \"";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return 0;
    }
    quote = strchr(line + strlen(prefix), '"');
    if (quote == NULL) {
        return 0;
    }
    line_marker = strstr(quote, ", line ");
    if (line_marker == NULL) {
        return 0;
    }
    end = line_marker + strlen(", line ");
    if (!m316_digits(end, line + strlen(line), &line_value)) {
        return 0;
    }
    m316_plain_init(diag, "PYTHON", "E", "SYNTAX", line);
    if (!m316_copy_range(diag->file, sizeof(diag->file),
                         line + strlen(prefix), quote)) {
        return 0;
    }
    diag->line = line_value;
    return 1;
}

static int m316_parse_facility(const char *line, m316_diag *diag)
{
    const char *first;
    const char *second;
    const char *comma;
    char message[M316_FIELD_MAX];
    char *trimmed;

    if (line == NULL || diag == NULL || line[0] != '%') {
        return 0;
    }
    first = strchr(line + 1, '-');
    if (first == NULL) {
        return 0;
    }
    second = strchr(first + 1, '-');
    comma = second != NULL ? strchr(second + 1, ',') : NULL;
    if (second == NULL || comma == NULL ||
        !m316_copy_range(diag->facility, sizeof(diag->facility),
                         line + 1, first) ||
        !m316_copy_range(diag->severity, sizeof(diag->severity),
                         first + 1, second) ||
        !m316_copy_range(diag->ident, sizeof(diag->ident),
                         second + 1, comma)) {
        return 0;
    }
    (void)strncpy(message, comma + 1, sizeof(message) - 1U);
    message[sizeof(message) - 1U] = '\0';
    trimmed = m316_trim(message);
    (void)strncpy(diag->message, trimmed, sizeof(diag->message) - 1U);
    diag->message[sizeof(diag->message) - 1U] = '\0';
    (void)strncpy(diag->raw, line, sizeof(diag->raw) - 1U);
    diag->raw[sizeof(diag->raw) - 1U] = '\0';
    diag->file[0] = '\0';
    diag->line = 0U;
    diag->column = 0U;
    diag->valid = 1;
    (void)m316_embedded_location(diag->message, diag);
    return 1;
}

static int m316_parse_location(const char *line, m316_diag *diag)
{
    const char *prefix;
    const char *file_marker;
    const char *file_start;
    const char *end;
    unsigned int line_value;

    if (line == NULL || diag == NULL || !diag->valid) {
        return 0;
    }
    prefix = strstr(line, "at line number ");
    if (prefix == NULL) {
        return 0;
    }
    prefix += strlen("at line number ");
    file_marker = strstr(prefix, " in file ");
    if (file_marker == NULL ||
        !m316_digits(prefix, file_marker, &line_value)) {
        return 0;
    }
    file_start = file_marker + strlen(" in file ");
    while (*file_start == ' ' || *file_start == '\t') {
        ++file_start;
    }
    end = file_start + strlen(file_start);
    while (end > file_start &&
           (end[-1] == '\r' || end[-1] == '\n' ||
            end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    if (!m316_copy_range(diag->file, sizeof(diag->file),
                         file_start, end)) {
        return 0;
    }
    diag->line = line_value;
    return 1;
}

static int m316_append_diag(char *result,
                            const char *language,
                            const char *phase,
                            const char *source,
                            const m316_diag *diag)
{
    char line[1536];
    const char *file;
    int written;

    if (result == NULL || language == NULL || phase == NULL ||
        source == NULL || diag == NULL || !diag->valid) {
        return 0;
    }
    file = diag->file[0] != '\0' ? diag->file : source;
    written = snprintf(line, sizeof(line),
                       "Normalized diagnostic: language=%s phase=%s severity=%s facility=%s id=%s file=%s line=%u column=%u message=%s\n",
                       language, phase, diag->severity, diag->facility,
                       diag->ident, file, diag->line, diag->column,
                       diag->message);
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return 0;
    }
    return m289_append(result, M289_NATIVE_RESULT_MAX, line);
}

static void m316_normalize_output(char *result,
                                  const char *language,
                                  const char *phase,
                                  const char *source,
                                  const char *output)
{
    const char *cursor;
    const char *next;
    char line[M316_LINE_MAX];
    size_t length;
    char *text;
    m316_diag pending;

    if (result == NULL || language == NULL || phase == NULL ||
        source == NULL || output == NULL || *output == '\0') {
        return;
    }
    (void)memset(&pending, 0, sizeof(pending));
    cursor = output;
    while (*cursor != '\0') {
        next = strchr(cursor, '\n');
        length = next != NULL ? (size_t)(next - cursor) : strlen(cursor);
        if (length >= sizeof(line)) {
            length = sizeof(line) - 1U;
        }
        (void)memcpy(line, cursor, length);
        line[length] = '\0';
        text = m316_trim(line);

        if (m289_equal_ci(language, "JAVA")) {
            m316_diag plain;
            if (m316_parse_java(text, &plain)) {
                if (pending.valid) {
                    (void)m316_append_diag(result, language, phase,
                                           source, &pending);
                    (void)memset(&pending, 0, sizeof(pending));
                }
                (void)m316_append_diag(result, language, phase, source, &plain);
            }
        } else if (m289_equal_ci(language, "PERL")) {
            m316_diag plain;
            if (m316_parse_perl(text, &plain)) {
                if (pending.valid) {
                    (void)m316_append_diag(result, language, phase,
                                           source, &pending);
                    (void)memset(&pending, 0, sizeof(pending));
                }
                (void)m316_append_diag(result, language, phase, source, &plain);
            }
        } else if (m289_equal_ci(language, "PYTHON")) {
            m316_diag plain;
            if (m316_parse_py_file(text, &plain)) {
                if (pending.valid) {
                    (void)m316_append_diag(result, language, phase,
                                           source, &pending);
                }
                pending = plain;
            } else if (pending.valid &&
                       strncmp(text, "SyntaxError: ", 13U) == 0) {
                (void)strncpy(pending.message, text + 13,
                              sizeof(pending.message) - 1U);
                pending.message[sizeof(pending.message) - 1U] = '\0';
                (void)m316_append_diag(result, language, phase,
                                       source, &pending);
                (void)memset(&pending, 0, sizeof(pending));
            }
        }

        if (*text == '%') {
            m316_diag current;
            (void)memset(&current, 0, sizeof(current));
            if (m316_parse_facility(text, &current)) {
                if (pending.valid && strcmp(pending.raw, current.raw) == 0) {
                    /* Duplicate SYS$OUTPUT/SYS$ERROR copy: keep one pending. */
                } else {
                    if (pending.valid) {
                        (void)m316_append_diag(result, language, phase,
                                               source, &pending);
                    }
                    pending = current;
                    if (pending.line != 0U || pending.column != 0U) {
                        (void)m316_append_diag(result, language, phase,
                                               source, &pending);
                        (void)memset(&pending, 0, sizeof(pending));
                    }
                }
            }
        } else if (pending.valid && m316_parse_location(text, &pending)) {
            (void)m316_append_diag(result, language, phase, source, &pending);
            (void)memset(&pending, 0, sizeof(pending));
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }
    if (pending.valid) {
        (void)m316_append_diag(result, language, phase, source, &pending);
    }
}

static void m316_patch_status(char *result,
                              const char *label,
                              unsigned long raw_status)
{
    char *position;
    char hex[9];
    int written;

    if (result == NULL || label == NULL) {
        return;
    }
    position = strstr(result, label);
    if (position == NULL) {
        return;
    }
    position = strstr(position + strlen(label), "%X");
    if (position == NULL || strlen(position) < 10U) {
        return;
    }
    written = snprintf(hex, sizeof(hex), "%08lX", raw_status);
    if (written == 8) {
        (void)memcpy(position + 2, hex, 8U);
    }
}

static void m316_patch_legacy(char *result,
                              const char *language,
                              const char *source,
                              unsigned long *status_out)
{
    if (result == NULL || status_out == NULL || m316_raw_count == 0U) {
        return;
    }
    if (strstr(result, "Compile status: ") != NULL) {
        m316_patch_status(result, "Compile status: ", m316_raw_status[0]);
        m316_normalize_output(result, language, "compile", source,
                              m316_raw_output[0]);
        if (m316_raw_count > 1U) {
            m316_patch_status(result, "Link status: ", m316_raw_status[1]);
            m316_normalize_output(result, language, "link", source,
                                  m316_raw_output[1]);
        }
    } else if (strstr(result, "Run status: ") != NULL) {
        m316_patch_status(result, "Run status: ", m316_raw_status[0]);
        m316_normalize_output(result, language, "run", source,
                              m316_raw_output[0]);
    } else if (strstr(result, "Procedure status: ") != NULL) {
        m316_patch_status(result, "Procedure status: ", m316_raw_status[0]);
        m316_normalize_output(result, language, "procedure", source,
                              m316_raw_output[0]);
    }
    *status_out = m316_raw_status[m316_raw_count - 1U];
}

static int m316_append_raw_status(char *result,
                                  const char *label,
                                  unsigned long status)
{
    char line[128];
    int written;

    written = snprintf(line, sizeof(line), "%s%%X%08lX (%s)\n",
                       label, status,
                       ovms_status_success(status) ? "success" : "failure");
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return 0;
    }
    return m289_append(result, M289_NATIVE_RESULT_MAX, line);
}

static char *m316_build_c(agent_state *state,
                          const char *source,
                          unsigned long *status_out)
{
    m289_build_profile profile;
    char compile_command[M289_PROFILE_CMD_MAX];
    char link_command[M289_PROFILE_CMD_MAX];
    char error[M289_PROFILE_ERROR_MAX];
    char compile_output[M316_CAPTURE_MAX];
    char link_output[M316_CAPTURE_MAX];
    char *result;
    unsigned long compile_status;
    unsigned long link_status;
    int executed;

    if (!m289_load_named_profile("C", &profile, error, sizeof(error)) ||
        !m289_profile_commands(&profile, source,
                               compile_command, sizeof(compile_command),
                               link_command, sizeof(link_command),
                               error, sizeof(error))) {
        return m289_make_error("Unable to resolve native build profile", error);
    }
    if (!m289_command_allowed(compile_command, M289_BUILD_COMPILE, "C") ||
        !m289_command_allowed(link_command, M289_BUILD_LINK, "C")) {
        return m289_make_error("M316 native C build refused",
                               "resolved command failed execution guard");
    }
    result = (char *)malloc(M289_NATIVE_RESULT_MAX);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';
    if (!m289_append_header(result, "C", source) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "Compile command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, compile_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }
    compile_output[0] = '\0';
    compile_status = 0UL;
    executed = command_dcl_exec(state, compile_command,
                                compile_output, sizeof(compile_output),
                                &compile_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Compile execution refused or unavailable.\n");
        if (compile_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, compile_output);
        }
        *status_out = compile_status;
        return result;
    }
    (void)m316_append_raw_status(result, "Compile status: ", compile_status);
    if (compile_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, "Compile output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, compile_output);
        if (compile_output[strlen(compile_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }
    m316_normalize_output(result, "C", "compile", source, compile_output);
    if (!ovms_status_success(compile_status)) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Link: not run because compile failed.\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Result: failure\n");
        *status_out = compile_status;
        return result;
    }
    if (!m289_append(result, M289_NATIVE_RESULT_MAX, "Link command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, link_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }
    link_output[0] = '\0';
    link_status = 0UL;
    executed = command_dcl_exec(state, link_command,
                                link_output, sizeof(link_output),
                                &link_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Link execution refused or unavailable.\n");
        if (link_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, link_output);
        }
        *status_out = link_status;
        return result;
    }
    (void)m316_append_raw_status(result, "Link status: ", link_status);
    if (link_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, "Link output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, link_output);
        if (link_output[strlen(link_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }
    m316_normalize_output(result, "C", "link", source, link_output);
    *status_out = link_status;
    (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                      ovms_status_success(link_status) ?
                          "Result: success\n" : "Result: failure\n");
    return result;
}

char *m289_build_source(agent_state *state,
                        const char *source,
                        unsigned long *status_out)
{
    char *result;
    char language[32];
    char *marker;
    char *end;
    size_t length;

    if (source != NULL && m316_suffix_ci(source, ".C")) {
        if (state == NULL || status_out == NULL || !llm_path_is_safe(source)) {
            return m289_make_error("M316 native C build refused",
                                   "invalid or unsafe source path");
        }
        return m316_build_c(state, source, status_out);
    }
    m316_capture_reset();
    result = m316_build_legacy(state, source, status_out);
    if (result == NULL || source == NULL || status_out == NULL) {
        return result;
    }
    language[0] = '\0';
    marker = strstr(result, "Language: ");
    if (marker != NULL) {
        marker += strlen("Language: ");
        end = strchr(marker, '\n');
        if (end != NULL) {
            length = (size_t)(end - marker);
            if (length > 0U && length < sizeof(language)) {
                (void)memcpy(language, marker, length);
                language[length] = '\0';
            }
        }
    }
    if (language[0] == '\0') {
        (void)strcpy(language, "UNKNOWN");
    }
    m316_patch_legacy(result, language, source, status_out);
    return result;
}
