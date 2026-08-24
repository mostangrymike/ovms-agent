#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_JSON_PARSE.H"
#include "LLM_SSE.H"

#define LLM_SSE_INITIAL 512U
#define LLM_SSE_MAX 4194304U
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

static int sse_append_char(char **buffer,
                           size_t *length,
                           size_t *capacity,
                           int ch)
{
    char *grown;
    size_t next_capacity;

    if (buffer == NULL || length == NULL || capacity == NULL) {
        return 0;
    }

    if (*length + 2U > LLM_SSE_MAX) {
        return 0;
    }

    if (*capacity < *length + 2U) {
        next_capacity = *capacity != 0U ?
            *capacity : LLM_SSE_INITIAL;

        while (next_capacity < *length + 2U) {
            if (next_capacity >= LLM_SSE_MAX / 2U) {
                next_capacity = LLM_SSE_MAX;
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

    (*buffer)[(*length)++] = (char)ch;
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

static int sse_marker_step(int ch, int *matched)
{
    static const char marker[] = "data:";

    if (matched == NULL) {
        return 0;
    }

    if (ch == marker[*matched]) {
        ++(*matched);
        if (*matched == 5) {
            *matched = 0;
            return 1;
        }
        return 0;
    }

    *matched = ch == 'd' ? 1 : 0;
    return 0;
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
    int mode;
    int marker_matched;
    int after_data;
    int in_object;
    int object_depth;
    int in_string;
    int escaped;
    int saw_sse;
    int final_seen;
    int success;
    int ch;
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
     * Raw diagnostic capture is opt-in and contains only bytes read
     * from curl stdout. Authentication headers and request data are
     * never written here. The input bytes are preserved exactly so a
     * later diagnostic can expose OpenVMS pipe record boundaries.
     */
    sse_remove_all(M273_SSE_RAW);
    debug_file = NULL;
    if (getenv("OVMS_AGENT_STREAM_DEBUG") != NULL) {
        debug_file = fopen(M273_SSE_RAW, "w", "rfm=stmlf");
    }

    event_data = NULL;
    event_length = 0U;
    event_capacity = 0U;
    plain_data = NULL;
    plain_length = 0U;
    plain_capacity = 0U;
    mode = 0;
    marker_matched = 0;
    after_data = 0;
    in_object = 0;
    object_depth = 0;
    in_string = 0;
    escaped = 0;
    saw_sse = 0;
    final_seen = 0;
    success = 1;

    while ((ch = fgetc(input)) != EOF) {
        if (debug_file != NULL) {
            (void)fputc(ch, debug_file);
            if (ch == '\n' || ch == '\r') {
                (void)fflush(debug_file);
            }
        }

        /*
         * Live VAX evidence shows C RTL pipe record boundaries can be
         * exposed as CR/LF at arbitrary byte positions: inside event
         * names, inside the `data:` marker, and inside the JSON object.
         * JSON carried by SSE is one wire line and encodes user newlines
         * as escapes, so raw CR/LF bytes are framing noise here.
         */
        if (ch == '\n' || ch == '\r') {
            continue;
        }

        if (mode == 0) {
            if (ch == ' ' || ch == '\t') {
                continue;
            }

            if (ch == '{') {
                mode = 1;
            } else {
                mode = 2;
            }
        }

        if (mode == 1) {
            if (!sse_append_char(&plain_data,
                                 &plain_length,
                                 &plain_capacity,
                                 ch)) {
                success = 0;
                break;
            }
            continue;
        }

        if (in_object) {
            if (!sse_append_char(&event_data,
                                 &event_length,
                                 &event_capacity,
                                 ch)) {
                success = 0;
                break;
            }

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
                ++object_depth;
            } else if (ch == '}') {
                --object_depth;
                if (object_depth == 0) {
                    if (!sse_process_event(event_data,
                                           text_callback,
                                           response_path,
                                           text_emitted,
                                           &final_seen)) {
                        success = 0;
                        break;
                    }

                    event_length = 0U;
                    if (event_data != NULL) {
                        event_data[0] = '\0';
                    }
                    in_object = 0;
                    in_string = 0;
                    escaped = 0;
                }
            }
            continue;
        }

        if (after_data) {
            if (ch == ' ' || ch == '\t') {
                continue;
            }

            after_data = 0;
            if (ch == '{') {
                saw_sse = 1;
                in_object = 1;
                object_depth = 1;
                in_string = 0;
                escaped = 0;
                event_length = 0U;
                if (!sse_append_char(&event_data,
                                     &event_length,
                                     &event_capacity,
                                     ch)) {
                    success = 0;
                    break;
                }
                continue;
            }
        }

        if (sse_marker_step(ch, &marker_matched)) {
            after_data = 1;
        }
    }

    if (ferror(input)) {
        success = 0;
    }

    if (in_object || after_data) {
        success = 0;
    }

    if (success && mode == 1 && plain_length != 0U) {
        const char *plain_start;

        plain_start = skip_space(plain_data);
        if (*plain_start == '{' &&
            sse_write_response(response_path, plain_start)) {
            final_seen = 1;
        } else {
            success = 0;
        }
    }

    if (debug_file != NULL) {
        (void)fclose(debug_file);
    }

    free(event_data);
    free(plain_data);
    return success &&
           ((mode == 1 && final_seen) ||
            (saw_sse && final_seen));
}
