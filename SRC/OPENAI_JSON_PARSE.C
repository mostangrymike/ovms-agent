#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_json_parse.h"

char *read_entire_file(const char *path, size_t *length_out)
{
    FILE *file;
    long length;
    char *data;
    size_t actual;

    if (openai_path_is_sensitive(path)) {
        (void)printf("Access denied for sensitive path: %s\n", path);
        return NULL;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);

    if (length < 0L) {
        (void)fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    data = malloc((size_t)length + 1U);

    if (data == NULL) {
        (void)fclose(file);
        return NULL;
    }

    actual = fread(data, 1U, (size_t)length, file);

    if (ferror(file)) {
        free(data);
        (void)fclose(file);
        return NULL;
    }

    data[actual] = '\0';
    (void)fclose(file);

    if (length_out != NULL) {
        *length_out = actual;
    }

    return data;
}

const char *skip_space(const char *position)
{
    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    return position;
}

int hex_value(int ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}

int append_utf8(char **destination,
                       size_t *remaining,
                       unsigned int codepoint)
{
    if (codepoint <= 0x7FU) {
        if (*remaining < 2U) {
            return 0;
        }

        *(*destination)++ = (char)codepoint;
        --(*remaining);
        return 1;
    }

    if (codepoint <= 0x7FFU) {
        if (*remaining < 3U) {
            return 0;
        }

        *(*destination)++ =
            (char)(0xC0U | (codepoint >> 6));
        *(*destination)++ =
            (char)(0x80U | (codepoint & 0x3FU));
        *remaining -= 2U;
        return 1;
    }

    if (codepoint <= 0xFFFFU) {
        if (*remaining < 4U) {
            return 0;
        }

        *(*destination)++ =
            (char)(0xE0U | (codepoint >> 12));
        *(*destination)++ =
            (char)(0x80U | ((codepoint >> 6) & 0x3FU));
        *(*destination)++ =
            (char)(0x80U | (codepoint & 0x3FU));
        *remaining -= 3U;
        return 1;
    }

    return 0;
}

char *json_decode_string(const char *start,
                                const char **after)
{
    const char *source;
    char *result;
    char *destination;
    size_t capacity;
    size_t remaining;

    if (start == NULL || *start != '"') {
        return NULL;
    }

    capacity = strlen(start) + 1U;
    result = malloc(capacity);

    if (result == NULL) {
        return NULL;
    }

    source = start + 1;
    destination = result;
    remaining = capacity;

    while (*source != '\0' && *source != '"') {
        unsigned char ch;

        ch = (unsigned char)*source++;

        if (ch != (unsigned char)'\\') {
            if (remaining < 2U) {
                free(result);
                return NULL;
            }

            *destination++ = (char)ch;
            --remaining;
            continue;
        }

        ch = (unsigned char)*source++;

        switch (ch) {
        case '"':
            *destination++ = '"';
            --remaining;
            break;

        case '\\':
            *destination++ = '\\';
            --remaining;
            break;

        case '/':
            *destination++ = '/';
            --remaining;
            break;

        case 'b':
            *destination++ = '\b';
            --remaining;
            break;

        case 'f':
            *destination++ = '\f';
            --remaining;
            break;

        case 'n':
            *destination++ = '\n';
            --remaining;
            break;

        case 'r':
            *destination++ = '\r';
            --remaining;
            break;

        case 't':
            *destination++ = '\t';
            --remaining;
            break;

        case 'u':
        {
            int h0;
            int h1;
            int h2;
            int h3;
            unsigned int codepoint;

            h0 = hex_value((unsigned char)source[0]);
            h1 = hex_value((unsigned char)source[1]);
            h2 = hex_value((unsigned char)source[2]);
            h3 = hex_value((unsigned char)source[3]);

            if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) {
                free(result);
                return NULL;
            }

            codepoint =
                ((unsigned int)h0 << 12) |
                ((unsigned int)h1 << 8) |
                ((unsigned int)h2 << 4) |
                (unsigned int)h3;

            source += 4;

            if (!append_utf8(&destination,
                             &remaining,
                             codepoint)) {
                free(result);
                return NULL;
            }

            break;
        }

        default:
            free(result);
            return NULL;
        }
    }

    if (*source != '"') {
        free(result);
        return NULL;
    }

    *destination = '\0';

    if (after != NULL) {
        *after = source + 1;
    }

    return result;
}

int json_key_matches(const char *position,
                            const char *key,
                            const char **value_start)
{
    char *decoded_key;
    const char *after_key;
    const char *cursor;

    if (*position != '"') {
        return 0;
    }

    decoded_key = json_decode_string(position, &after_key);

    if (decoded_key == NULL) {
        return 0;
    }

    if (strcmp(decoded_key, key) != 0) {
        free(decoded_key);
        return 0;
    }

    free(decoded_key);
    cursor = skip_space(after_key);

    if (*cursor != ':') {
        return 0;
    }

    cursor = skip_space(cursor + 1);

    if (value_start != NULL) {
        *value_start = cursor;
    }

    return 1;
}

const char *find_string_value(const char *start,
                                     const char *key)
{
    const char *position;

    for (position = start;
         *position != '\0';
         ++position) {
        const char *value;

        if (*position == '"' &&
            json_key_matches(position, key, &value) &&
            *value == '"') {
            return value;
        }

        if (*position == '\\' && position[1] != '\0') {
            ++position;
        }
    }

    return NULL;
}

const char *find_output_text_object(const char *json)
{
    const char *position;

    for (position = json;
         *position != '\0';
         ++position) {
        const char *value;

        if (*position == '"' &&
            json_key_matches(position, "type", &value) &&
            *value == '"') {
            char *type_value;

            type_value = json_decode_string(value, NULL);

            if (type_value != NULL) {
                int match;

                match = strcmp(type_value, "output_text") == 0;
                free(type_value);

                if (match) {
                    return position;
                }
            }
        }

        if (*position == '\\' && position[1] != '\0') {
            ++position;
        }
    }

    return NULL;
}

char *extract_top_level_id(const char *json)
{
    const char *value;

    value = find_string_value(json, "id");

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

const char *find_function_call_object(const char *json)
{
    const char *position;

    for (position = json;
         position != NULL && *position != '\0';
         ++position) {
        const char *value;

        if (*position == '"' &&
            json_key_matches(position, "type", &value) &&
            *value == '"') {
            char *decoded;
            int match;

            decoded = json_decode_string(value, NULL);

            if (decoded == NULL) {
                continue;
            }

            match = strcmp(decoded, "function_call") == 0;
            free(decoded);

            if (match) {
                return position;
            }
        }
    }

    return NULL;
}

int extract_function_call(const char *json,
                                 char **name,
                                 char **call_id,
                                 char **arguments)
{
    const char *object;
    const char *value;

    *name = NULL;
    *call_id = NULL;
    *arguments = NULL;

    object = find_function_call_object(json);

    if (object == NULL) {
        return 0;
    }

    value = find_string_value(object, "name");

    if (value != NULL) {
        *name = json_decode_string(value, NULL);
    }

    value = find_string_value(object, "call_id");

    if (value != NULL) {
        *call_id = json_decode_string(value, NULL);
    }

    value = find_string_value(object, "arguments");

    if (value != NULL) {
        *arguments = json_decode_string(value, NULL);
    }

    if (*name == NULL ||
        *call_id == NULL ||
        *arguments == NULL) {
        free(*name);
        free(*call_id);
        free(*arguments);
        *name = NULL;
        *call_id = NULL;
        *arguments = NULL;
        return 0;
    }

    return 1;
}

char *extract_path_argument(const char *arguments)
{
    const char *value;

    value = find_string_value(arguments, "path");

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

char *extract_string_argument(const char *arguments,
                                     const char *name)
{
    const char *value;

    value = find_string_value(arguments, name);

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

int extract_integer_argument(const char *arguments,
                                    const char *name,
                                    long *value_out)
{
    const char *position;
    const char *value_start;
    char *end_pointer;
    long parsed_value;

    if (arguments == NULL ||
        name == NULL ||
        value_out == NULL) {
        return 0;
    }

    for (position = arguments;
         *position != '\0';
         ++position) {
        if (*position == '"' &&
            json_key_matches(position, name, &value_start)) {
            value_start = skip_space(value_start);
            parsed_value = strtol(
                value_start,
                &end_pointer,
                10
            );

            if (end_pointer == value_start) {
                return 0;
            }

            *value_out = parsed_value;
            return 1;
        }
    }

    return 0;
}

char *extract_output_items_json(const char *json)
{
    const char *key;
    const char *position;
    const char *array_start;
    const char *array_end;
    int depth;
    int in_string;
    int escaped;
    size_t length;
    char *result;

    if (json == NULL) {
        return NULL;
    }

    key = strstr(json, "\"output\"");
    if (key == NULL) {
        return NULL;
    }

    position = strchr(key, ':');
    if (position == NULL) {
        return NULL;
    }

    position = skip_space(position + 1);
    if (*position != '[') {
        return NULL;
    }

    array_start = position + 1;
    position = array_start;
    array_end = NULL;
    depth = 1;
    in_string = 0;
    escaped = 0;

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
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                array_end = position;
                break;
            }
        }

        ++position;
    }

    if (array_end == NULL) {
        return NULL;
    }

    length = (size_t)(array_end - array_start);
    result = malloc(length + 1U);
    if (result == NULL) {
        return NULL;
    }

    (void)memcpy(result, array_start, length);
    result[length] = '\0';
    return result;
}

char *extract_output_text_from_json(const char *json)
{
    const char *search_start;

    if (json == NULL) {
        return NULL;
    }

    search_start = json;

    while (*search_start != '\0') {
        const char *object_start;
        const char *text_value;
        char *decoded;

        object_start = find_output_text_object(search_start);

        if (object_start == NULL) {
            return NULL;
        }

        text_value = find_string_value(object_start, "text");

        if (text_value != NULL) {
            decoded = json_decode_string(text_value, NULL);

            if (decoded != NULL) {
                if (*decoded != '\0') {
                    return decoded;
                }

                free(decoded);
            }
        }

        search_start = object_start + 1;
    }

    return NULL;
}
