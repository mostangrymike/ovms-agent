#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai.h"

#define OPENAI_REQUEST_FILE  "OVMS_AGENT_REQUEST.JSON"
#define OPENAI_RESPONSE_FILE "OVMS_AGENT_RESPONSE.JSON"
#define OPENAI_HEADERS_FILE  "OVMS_AGENT_HEADERS.TXT"
#define OPENAI_RESPONSE_ID_SIZE 128

static char previous_response_id[OPENAI_RESPONSE_ID_SIZE];

static int json_write_escaped(FILE *file, const char *text)
{
    const unsigned char *position;

    if (file == NULL || text == NULL) {
        return 0;
    }

    for (position = (const unsigned char *)text;
         *position != (unsigned char)'\0';
         ++position) {
        switch (*position) {
        case '"':
            if (fputs("\\\"", file) == EOF) {
                return 0;
            }
            break;

        case '\\':
            if (fputs("\\\\", file) == EOF) {
                return 0;
            }
            break;

        case '\b':
            if (fputs("\\b", file) == EOF) {
                return 0;
            }
            break;

        case '\f':
            if (fputs("\\f", file) == EOF) {
                return 0;
            }
            break;

        case '\n':
            if (fputs("\\n", file) == EOF) {
                return 0;
            }
            break;

        case '\r':
            if (fputs("\\r", file) == EOF) {
                return 0;
            }
            break;

        case '\t':
            if (fputs("\\t", file) == EOF) {
                return 0;
            }
            break;

        default:
            if (*position < 32U) {
                if (fprintf(file,
                            "\\u%04x",
                            (unsigned int)*position) < 0) {
                    return 0;
                }
            } else if (fputc((int)*position, file) == EOF) {
                return 0;
            }
            break;
        }
    }

    return 1;
}

static int write_request(const char *model,
                         const char *prompt,
                         const char *previous_id)
{
    FILE *file;
    int success;

    file = fopen(OPENAI_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     OPENAI_REQUEST_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, prompt) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success && fputs("}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write complete API request.");
        return 0;
    }

    return 1;
}

static int write_headers(const char *api_key)
{
    FILE *file;
    int success;

    file = fopen(OPENAI_HEADERS_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     OPENAI_HEADERS_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("Content-Type: application/json\n", file) == EOF ||
        fputs("Authorization: Bearer ", file) == EOF ||
        fputs(api_key, file) == EOF ||
        fputc('\n', file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write API headers.");
        return 0;
    }

    return 1;
}

static void remove_temporary_files(void)
{
    (void)remove(OPENAI_REQUEST_FILE);
    (void)remove(OPENAI_HEADERS_FILE);
}

static char *read_entire_file(const char *path, size_t *length_out)
{
    FILE *file;
    long length;
    char *data;
    size_t actual;

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

static const char *skip_space(const char *position)
{
    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    return position;
}

static int hex_value(int ch)
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

static int append_utf8(char **destination,
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

static char *json_decode_string(const char *start,
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

static int json_key_matches(const char *position,
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

static const char *find_string_value(const char *start,
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

static const char *find_output_text_object(const char *json)
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

static int save_response_id(const char *json)
{
    const char *id_value;
    char *decoded;

    id_value = find_string_value(json, "id");

    if (id_value == NULL) {
        return 0;
    }

    decoded = json_decode_string(id_value, NULL);

    if (decoded == NULL) {
        return 0;
    }

    if (strlen(decoded) >= sizeof(previous_response_id)) {
        free(decoded);
        return 0;
    }

    (void)strcpy(previous_response_id, decoded);
    free(decoded);
    return 1;
}

static int display_clean_response(void)
{
    char *json;
    const char *object_start;
    const char *text_value;
    char *decoded;

    json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);

    if (json == NULL) {
        return 0;
    }

    (void)save_response_id(json);

    object_start = find_output_text_object(json);

    if (object_start != NULL) {
        text_value = find_string_value(object_start, "text");

        if (text_value != NULL) {
            decoded = json_decode_string(text_value, NULL);

            if (decoded != NULL) {
                (void)puts("");
                (void)puts(decoded);
                free(decoded);
                free(json);
                return 1;
            }
        }
    }

    text_value = find_string_value(json, "message");

    if (text_value != NULL) {
        decoded = json_decode_string(text_value, NULL);

        if (decoded != NULL) {
            (void)puts("");
            (void)printf("OpenAI API error: %s\n", decoded);
            free(decoded);
            free(json);
            return 1;
        }
    }

    free(json);
    return 0;
}

static void display_raw_response(void)
{
    FILE *file;
    char line[2048];

    file = fopen(OPENAI_RESPONSE_FILE, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n",
                     OPENAI_RESPONSE_FILE,
                     strerror(errno));
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)fputs(line, stdout);
    }

    (void)fclose(file);
}

static void openai_send(agent_state *state,
                        const char *prompt,
                        int continue_conversation)
{
    const char *api_key;
    const char *model;
    const char *previous_id;
    int status;

    (void)state;

    if (prompt == NULL || *prompt == '\0') {
        (void)puts("Prompt cannot be empty.");
        return;
    }

    api_key = getenv("OPENAI_API_KEY");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("OPENAI_API_KEY is not defined.");
        return;
    }

    model = getenv("OVMS_AGENT_MODEL");

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        return;
    }

    previous_id = NULL;

    if (continue_conversation &&
        previous_response_id[0] != '\0') {
        previous_id = previous_response_id;
    }

    if (!write_request(model, prompt, previous_id) ||
        !write_headers(api_key)) {
        remove_temporary_files();
        return;
    }

    (void)puts("Sending request to OpenAI...");

    status = system(
        "curl --silent --show-error "
        "--output " OPENAI_RESPONSE_FILE " "
        "--header @" OPENAI_HEADERS_FILE " "
        "--data-binary @" OPENAI_REQUEST_FILE " "
        "https://api.openai.com/v1/responses"
    );

    remove_temporary_files();

    if ((status & 1) == 0) {
        (void)printf(
            "curl failed with OpenVMS status %d.\n",
            status
        );
        return;
    }

    if (!display_clean_response()) {
        (void)puts("");
        (void)puts(
            "Unable to extract assistant text; raw response follows."
        );
        display_raw_response();
    }

    (void)putchar('\n');
}


static int openai_path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (*path == '/' ||
        strchr(path, ':') != NULL ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    return 1;
}

static char *openai_read_text_file(const char *path)
{
    FILE *file;
    long length;
    size_t actual;
    char *data;

    file = fopen(path, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n",
                     path,
                     strerror(errno));
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)printf("Unable to size %s: %s\n",
                     path,
                     strerror(errno));
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);

    if (length < 0L) {
        (void)printf("Unable to size %s.\n", path);
        (void)fclose(file);
        return NULL;
    }

    if (length > 65536L) {
        (void)printf(
            "File is too large for REVIEW (%ld bytes; limit 65536).\n",
            length
        );
        (void)fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)printf("Unable to rewind %s: %s\n",
                     path,
                     strerror(errno));
        (void)fclose(file);
        return NULL;
    }

    data = malloc((size_t)length + 1U);

    if (data == NULL) {
        (void)puts("Insufficient memory for file review.");
        (void)fclose(file);
        return NULL;
    }

    actual = fread(data, 1U, (size_t)length, file);

    if (ferror(file)) {
        (void)printf("Unable to read %s: %s\n",
                     path,
                     strerror(errno));
        free(data);
        (void)fclose(file);
        return NULL;
    }

    data[actual] = '\0';
    (void)fclose(file);
    return data;
}

void openai_review_file(agent_state *state, const char *path)
{
    static const char introduction[] =
        "Review this OpenVMS C source file. Identify correctness bugs, "
        "OpenVMS portability problems, security issues, and maintainability "
        "problems. Prioritize concrete findings. Do not rewrite the entire "
        "file unless necessary.\n\nFile: ";
    static const char separator[] = "\n\nSource:\n";
    char *source;
    char *prompt;
    size_t prompt_size;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!openai_path_is_safe(path)) {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    source = openai_read_text_file(path);

    if (source == NULL) {
        return;
    }

    prompt_size =
        strlen(introduction) +
        strlen(path) +
        strlen(separator) +
        strlen(source) +
        1U;

    prompt = malloc(prompt_size);

    if (prompt == NULL) {
        (void)puts("Insufficient memory for review prompt.");
        free(source);
        return;
    }

    (void)strcpy(prompt, introduction);
    (void)strcat(prompt, path);
    (void)strcat(prompt, separator);
    (void)strcat(prompt, source);

    openai_send(state, prompt, 0);

    free(prompt);
    free(source);
}

void openai_ask(agent_state *state, const char *prompt)
{
    openai_send(state, prompt, 0);
}

void openai_chat(agent_state *state, const char *prompt)
{
    openai_send(state, prompt, 1);
}

void openai_chat_reset(void)
{
    previous_response_id[0] = '\0';
    (void)puts("OpenAI conversation reset.");
}

int openai_chat_active(void)
{
    return previous_response_id[0] != '\0';
}
