#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_JSON_PARSE.H"
#include "LLM_SSE.H"

#define LLM_SSE_LINE_INITIAL 512U
#define LLM_SSE_LINE_MAX 4194304U
#define M273_SSE_RAW "OVMS_AGENT_STREAM_RAW.TMP"

static void sse_remove_all(const char *path)
{
    if (path == NULL || *path == '\0') {
        return;
    }

    while (remove(path) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static char *sse_read_line(FILE *input)
{
    char *line;
    size_t capacity;
    size_t length;
    int ch;

    if (input == NULL) {
        return NULL;
    }

    capacity = LLM_SSE_LINE_INITIAL;
    line = (char *)malloc(capacity);
    if (line == NULL) {
        return NULL;
    }

    length = 0U;
    ch = EOF;

    while ((ch = fgetc(input)) != EOF) {
        if (ch == '\n') {
            break;
        }

        if (ch == '\r') {
            continue;
        }

        if (length + 1U >= capacity) {
            char *grown;
            size_t next_capacity;

            if (capacity >= LLM_SSE_LINE_MAX) {
                free(line);
                return NULL;
            }

            next_capacity = capacity * 2U;
            if (next_capacity > LLM_SSE_LINE_MAX) {
                next_capacity = LLM_SSE_LINE_MAX;
            }

            grown = (char *)realloc(line, next_capacity);
            if (grown == NULL) {
                free(line);
                return NULL;
            }

            line = grown;
            capacity = next_capacity;
        }

        line[length++] = (char)ch;
    }

    if (ch == EOF && length == 0U) {
        free(line);
        return NULL;
    }

    line[length] = '\0';
    return line;
}

static int sse_append(char **buffer,
                      size_t *length,
                      size_t *capacity,
                      const char *text,
                      int add_newline)
{
    size_t text_length;
    size_t needed;
    char *grown;

    if (buffer == NULL ||
        length == NULL ||
        capacity == NULL ||
        text == NULL) {
        return 0;
    }

    text_length = strlen(text);
    needed = *length + text_length +
        (add_newline ? 1U : 0U) + 1U;

    if (needed > LLM_SSE_LINE_MAX) {
        return 0;
    }

    if (*capacity < needed) {
        size_t next_capacity;

        next_capacity = *capacity != 0U ?
            *capacity : LLM_SSE_LINE_INITIAL;

        while (next_capacity < needed) {
            if (next_capacity >= LLM_SSE_LINE_MAX / 2U) {
                next_capacity = LLM_SSE_LINE_MAX;
                break;
            }
            next_capacity *= 2U;
        }

        grown = (char *)realloc(*buffer, next_capacity);
        if (grown == NULL) {
            return 0;
        }

        *buffer = grown;
        *capacity = next_capacity;
    }

    if (add_newline && *length != 0U) {
        (*buffer)[(*length)++] = '\n';
    }

    if (text_length != 0U) {
        (void)memcpy(*buffer + *length, text, text_length);
        *length += text_length;
    }

    (*buffer)[*length] = '\0';
    return 1;
}

static char *sse_response_object(const char *json)
{
    const char *key;
    const char *colon;
    const char *start;
    const char *position;
    int depth;
    int in_string;
    int escaped;
    size_t length;
    char *result;

    if (json == NULL) {
        return NULL;
    }

    key = strstr(json, "\"response\"");
    if (key == NULL) {
        return NULL;
    }

    colon = strchr(key, ':');
    if (colon == NULL) {
        return NULL;
    }

    start = skip_space(colon + 1);
    if (*start != '{') {
        return NULL;
    }

    depth = 0;
    in_string = 0;
    escaped = 0;
    position = start;

    while (*position != '\0') {
        char ch;

        ch = *position;

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
        } else if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                ++position;
                length = (size_t)(position - start);
                result = (char *)malloc(length + 1U);
                if (result == NULL) {
                    return NULL;
                }
                (void)memcpy(result, start, length);
                result[length] = '\0';
                return result;
            }
        }

        ++position;
    }

    return NULL;
}

static int sse_write_response(const char *path,
                              const char *json)
{
    FILE *file;
    size_t length;
    int success;

    if (path == NULL || *path == '\0' ||
        json == NULL || *json == '\0') {
        return 0;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    length = strlen(json);
    success = fwrite(json, 1U, length, file) == length;
    if (success && fputc('\n', file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int sse_process_event(const char *data,
                             llm_sse_text_cb text_callback,
                             const char *response_path,
                             int *text_emitted,
                             int *final_seen)
{
    const char *value;
    char *type_value;

    if (data == NULL || *data == '\0') {
        return 1;
    }

    value = find_string_value(data, "type");
    if (value == NULL) {
        return 1;
    }

    type_value = json_decode_string(value, NULL);
    if (type_value == NULL) {
        return 0;
    }

    if (strcmp(type_value, "response.output_text.delta") == 0) {
        char *delta;

        value = find_string_value(data, "delta");
        delta = value != NULL ?
            json_decode_string(value, NULL) : NULL;

        if (delta == NULL) {
            free(type_value);
            return 0;
        }

        if (*delta != '\0') {
            if (text_callback != NULL) {
                text_callback(delta);
            }
            if (text_emitted != NULL) {
                *text_emitted = 1;
            }
        }

        free(delta);
    } else if (strcmp(type_value, "response.completed") == 0 ||
               strcmp(type_value, "response.failed") == 0 ||
               strcmp(type_value, "response.incomplete") == 0) {
        char *response;

        response = sse_response_object(data);
        if (response == NULL ||
            !sse_write_response(response_path, response)) {
            free(response);
            free(type_value);
            return 0;
        }

        free(response);
        if (final_seen != NULL) {
            *final_seen = 1;
        }
    } else if (strcmp(type_value, "error") == 0) {
        if (!sse_write_response(response_path, data)) {
            free(type_value);
            return 0;
        }

        if (final_seen != NULL) {
            *final_seen = 1;
        }
    }

    free(type_value);
    return 1;
}

int llm_sse_parse(FILE *input,
                  llm_sse_text_cb text_callback,
                  const char *response_path,
                  int *text_emitted)
{
    char *event_data;
    size_t event_length;
    size_t event_capacity;
    char *plain_data;
    size_t plain_length;
    size_t plain_capacity;
    int saw_sse;
    int final_seen;
    int success;
    char *line;
    FILE *debug_file;

    if (input == NULL ||
        response_path == NULL ||
        *response_path == '\0') {
        return 0;
    }

    if (text_emitted != NULL) {
        *text_emitted = 0;
    }

    /*
     * Raw diagnostic capture is opt-in and contains only the provider
     * response body read from curl stdout.  Authentication headers and
     * the request body are never written here.  Remove stale versions
     * on every parse so a later diagnostic can never be mistaken for a
     * current stream.
     */
    sse_remove_all(M273_SSE_RAW);
    debug_file = NULL;
    if (getenv("OVMS_AGENT_STREAM_DEBUG") != NULL) {
        debug_file = fopen(M273_SSE_RAW, "w");
    }

    event_data = NULL;
    event_length = 0U;
    event_capacity = 0U;
    plain_data = NULL;
    plain_length = 0U;
    plain_capacity = 0U;
    saw_sse = 0;
    final_seen = 0;
    success = 1;

    while ((line = sse_read_line(input)) != NULL) {
        if (debug_file != NULL) {
            (void)fputs(line, debug_file);
            (void)fputc('\n', debug_file);
            (void)fflush(debug_file);
        }

        if (line[0] == '\0') {
            if (event_length != 0U) {
                if (!sse_process_event(event_data,
                                       text_callback,
                                       response_path,
                                       text_emitted,
                                       &final_seen)) {
                    success = 0;
                    free(line);
                    break;
                }
                event_length = 0U;
                if (event_data != NULL) {
                    event_data[0] = '\0';
                }
            }
        } else if (strncmp(line, "event:", 6U) == 0) {
            saw_sse = 1;
        } else if (strncmp(line, "data:", 5U) == 0) {
            const char *data;

            saw_sse = 1;
            data = line + 5;
            if (*data == ' ') {
                ++data;
            }

            if (!sse_append(&event_data,
                            &event_length,
                            &event_capacity,
                            data,
                            event_length != 0U)) {
                success = 0;
                free(line);
                break;
            }
        } else if (!saw_sse) {
            if (!sse_append(&plain_data,
                            &plain_length,
                            &plain_capacity,
                            line,
                            plain_length != 0U)) {
                success = 0;
                free(line);
                break;
            }
        }

        free(line);
    }

    if (success && event_length != 0U) {
        if (!sse_process_event(event_data,
                               text_callback,
                               response_path,
                               text_emitted,
                               &final_seen)) {
            success = 0;
        }
    }

    if (success && !saw_sse && plain_length != 0U) {
        const char *plain_start;

        plain_start = skip_space(plain_data);
        if (*plain_start == '{' &&
            sse_write_response(response_path, plain_start)) {
            final_seen = 1;
        } else {
            success = 0;
        }
    }

    if (ferror(input)) {
        success = 0;
    }

    if (debug_file != NULL) {
        (void)fclose(debug_file);
    }

    free(event_data);
    free(plain_data);
    return success && final_seen;
}
