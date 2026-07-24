#include <ctype.h>
#include <dirent.h>
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


#define OPENAI_AGENT_MAX_TURNS 6
#define OPENAI_AGENT_CACHE_SIZE 8
#define OPENAI_SEARCH_OUTPUT_LIMIT 32768
#define OPENAI_LIST_OUTPUT_LIMIT 32768

typedef struct openai_file_cache_entry {
    char *path;
    char *content;
} openai_file_cache_entry;

static int write_agent_tools(FILE *file)
{
    return fputs(
        "\"tools\":["
        "{"
        "\"type\":\"function\","
        "\"name\":\"list_directory\","
        "\"description\":\"List entries in one project-relative directory. Use dot for the project root.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"read_file\","
        "\"description\":\"Read one project-relative text file. Use Unix-style paths such as SRC/MAIN.C.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"search_file\","
        "\"description\":\"Search one project-relative text file for a literal string and return matching lines with line numbers.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"pattern\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\",\"pattern\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "}"
        "]",
        file
    ) != EOF;
}
static int write_agent_request(const char *model,
                               const char *instructions,
                               const char *user_prompt,
                               const char *previous_id,
                               const char *call_id,
                               const char *tool_output)
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
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
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

    if (success && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"input\":[{\"type\":\"function_call_output\","
                  "\"call_id\":\"", file) == EOF ||
            !json_write_escaped(file, call_id) ||
            fputs("\",\"output\":\"", file) == EOF ||
            !json_write_escaped(file, tool_output) ||
            fputs("\"}],", file) == EOF ||
            !write_agent_tools(file)) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF ||
            !write_agent_tools(file)) {
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
        (void)puts("Unable to write complete agent request.");
        return 0;
    }

    return 1;
}

static int perform_openai_request(void)
{
    int status;

    status = system(
        "curl --silent --show-error "
        "--output " OPENAI_RESPONSE_FILE " "
        "--header @" OPENAI_HEADERS_FILE " "
        "--data-binary @" OPENAI_REQUEST_FILE " "
        "https://api.openai.com/v1/responses"
    );

    return (status & 1) != 0;
}

static char *extract_top_level_id(const char *json)
{
    const char *value;

    value = find_string_value(json, "id");

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

static const char *find_function_call_object(const char *json)
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

static int extract_function_call(const char *json,
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

static char *extract_path_argument(const char *arguments)
{
    const char *value;

    value = find_string_value(arguments, "path");

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

static char *make_tool_error(const char *message,
                             const char *path)
{
    size_t size;
    char *result;

    size = strlen(message) + 1U;

    if (path != NULL) {
        size += strlen(path) + 2U;
    }

    result = malloc(size);

    if (result == NULL) {
        return NULL;
    }

    (void)strcpy(result, message);

    if (path != NULL) {
        (void)strcat(result, ": ");
        (void)strcat(result, path);
    }

    return result;
}

static char *extract_string_argument(const char *arguments,
                                     const char *name)
{
    const char *value;

    value = find_string_value(arguments, name);

    if (value == NULL) {
        return NULL;
    }

    return json_decode_string(value, NULL);
}

static void openai_cache_init(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        cache[index].path = NULL;
        cache[index].content = NULL;
    }
}

static void openai_cache_free(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        free(cache[index].path);
        free(cache[index].content);
    }
}

static const char *openai_cache_lookup(
    const openai_file_cache_entry *cache,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        if (cache[index].path != NULL &&
            strcmp(cache[index].path, path) == 0) {
            return cache[index].content;
        }
    }

    return NULL;
}

static int openai_cache_store(openai_file_cache_entry *cache,
                              const char *path,
                              const char *content)
{
    unsigned int index;
    char *path_copy;
    char *content_copy;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        if (cache[index].path == NULL) {
            break;
        }
    }

    if (index == OPENAI_AGENT_CACHE_SIZE) {
        return 0;
    }

    path_copy = malloc(strlen(path) + 1U);
    content_copy = malloc(strlen(content) + 1U);

    if (path_copy == NULL || content_copy == NULL) {
        free(path_copy);
        free(content_copy);
        return 0;
    }

    (void)strcpy(path_copy, path);
    (void)strcpy(content_copy, content);
    cache[index].path = path_copy;
    cache[index].content = content_copy;
    return 1;
}

static char *openai_duplicate_text(const char *text)
{
    char *copy;

    copy = malloc(strlen(text) + 1U);

    if (copy != NULL) {
        (void)strcpy(copy, text);
    }

    return copy;
}

static char *execute_read_file_tool(
    const char *arguments,
    openai_file_cache_entry *cache,
    int *cache_hit,
    char **display_path)
{
    char *path;
    char *output;
    const char *cached;

    *cache_hit = 0;
    *display_path = NULL;
    path = extract_path_argument(arguments);

    if (path == NULL) {
        return make_tool_error(
            "read_file arguments did not contain a valid path",
            NULL
        );
    }

    *display_path = openai_duplicate_text(path);

    if (!openai_path_is_safe(path)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            path
        );
        free(path);
        return output;
    }

    cached = openai_cache_lookup(cache, path);

    if (cached != NULL) {
        *cache_hit = 1;
        output = openai_duplicate_text(cached);
        free(path);
        return output;
    }

    output = openai_read_text_file(path);

    if (output == NULL) {
        output = make_tool_error("Unable to read file", path);
    } else {
        (void)openai_cache_store(cache, path, output);
    }

    free(path);
    return output;
}

static int openai_join_path(const char *parent,
                            const char *child,
                            char *output,
                            size_t output_size)
{
    int written;

    if (parent == NULL || child == NULL ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (*parent == '\0' || strcmp(parent, ".") == 0) {
        written = snprintf(output, output_size, "%s", child);
    } else {
        written = snprintf(output, output_size, "%s/%s", parent, child);
    }

    return written >= 0 && (size_t)written < output_size;
}

static char *execute_list_directory_tool(const char *arguments,
                                         char **display_path)
{
    char *path;
    const char *target;
    DIR *directory;
    struct dirent *entry;
    char *output;
    size_t used;
    unsigned long count;

    *display_path = NULL;
    path = extract_string_argument(arguments, "path");

    if (path == NULL) {
        return make_tool_error(
            "list_directory arguments did not contain a valid path",
            NULL
        );
    }

    target = *path == '\0' ? "." : path;
    *display_path = openai_duplicate_text(target);

    if (strcmp(target, ".") != 0 &&
        !openai_path_is_safe(target)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            target
        );
        free(path);
        return output;
    }

    directory = opendir(target);

    if (directory == NULL) {
        output = make_tool_error("Unable to open directory", target);
        free(path);
        return output;
    }

    output = malloc(OPENAI_LIST_OUTPUT_LIMIT);

    if (output == NULL) {
        (void)closedir(directory);
        free(path);
        return make_tool_error(
            "Insufficient memory for directory listing",
            target
        );
    }

    output[0] = '\0';
    used = 0U;
    count = 0UL;

    while ((entry = readdir(directory)) != NULL) {
        char child_path[1024];
        DIR *child_directory;
        int is_directory;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, ".git", 4U) == 0) {
            continue;
        }

        if (!openai_join_path(target,
                              entry->d_name,
                              child_path,
                              sizeof(child_path))) {
            continue;
        }

        child_directory = opendir(child_path);
        is_directory = child_directory != NULL;

        if (child_directory != NULL) {
            (void)closedir(child_directory);
        }

        written = snprintf(output + used,
                           OPENAI_LIST_OUTPUT_LIMIT - used,
                           "%s%s\n",
                           entry->d_name,
                           is_directory ? "/" : "");

        if (written < 0 ||
            (size_t)written >= OPENAI_LIST_OUTPUT_LIMIT - used) {
            (void)snprintf(output + used,
                           OPENAI_LIST_OUTPUT_LIMIT - used,
                           "[directory listing truncated]\n");
            break;
        }

        used += (size_t)written;
        ++count;
    }

    (void)closedir(directory);
    free(path);

    if (count == 0UL && output[0] == '\0') {
        (void)strcpy(output, "[empty directory]");
    }

    return output;
}

static char *execute_search_file_tool(const char *arguments,
                                      char **display_path,
                                      char **display_pattern)
{
    char *path;
    char *pattern;
    FILE *file;
    char line[2048];
    unsigned long line_number;
    unsigned long matches;
    size_t used;
    char *output;

    *display_path = NULL;
    *display_pattern = NULL;

    path = extract_string_argument(arguments, "path");
    pattern = extract_string_argument(arguments, "pattern");

    if (path == NULL || pattern == NULL || *pattern == '\0') {
        free(path);
        free(pattern);
        return make_tool_error(
            "search_file requires valid path and pattern arguments",
            NULL
        );
    }

    *display_path = openai_duplicate_text(path);
    *display_pattern = openai_duplicate_text(pattern);

    if (!openai_path_is_safe(path)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            path
        );
        free(path);
        free(pattern);
        return output;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        output = make_tool_error("Unable to search file", path);
        free(path);
        free(pattern);
        return output;
    }

    output = malloc(OPENAI_SEARCH_OUTPUT_LIMIT);

    if (output == NULL) {
        (void)fclose(file);
        free(path);
        free(pattern);
        return make_tool_error("Insufficient memory for search", path);
    }

    output[0] = '\0';
    used = 0U;
    line_number = 1UL;
    matches = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, pattern) != NULL) {
            int written;

            written = snprintf(output + used,
                               OPENAI_SEARCH_OUTPUT_LIMIT - used,
                               "%lu: %s",
                               line_number,
                               line);

            if (written < 0 ||
                (size_t)written >=
                    OPENAI_SEARCH_OUTPUT_LIMIT - used) {
                (void)snprintf(
                    output + used,
                    OPENAI_SEARCH_OUTPUT_LIMIT - used,
                    "\n[search output truncated]\n"
                );
                break;
            }

            used += (size_t)written;

            if (used > 0U &&
                output[used - 1U] != '\n' &&
                used + 1U < OPENAI_SEARCH_OUTPUT_LIMIT) {
                output[used++] = '\n';
                output[used] = '\0';
            }

            ++matches;
        }

        ++line_number;
    }

    (void)fclose(file);

    if (matches == 0UL) {
        (void)strcpy(output, "No matching lines.");
    }

    free(path);
    free(pattern);
    return output;
}

static int display_output_text_from_json(const char *json)
{
    const char *object_start;
    const char *text_value;
    char *decoded;

    object_start = find_output_text_object(json);

    if (object_start == NULL) {
        return 0;
    }

    text_value = find_string_value(object_start, "text");

    if (text_value == NULL) {
        return 0;
    }

    decoded = json_decode_string(text_value, NULL);

    if (decoded == NULL) {
        return 0;
    }

    (void)puts("");
    (void)puts(decoded);
    free(decoded);
    return 1;
}

void openai_agent(agent_state *state, const char *goal)
{
    static const char instructions[] =
        "You are a careful OpenVMS C code analyst operating inside a "
        "project sandbox. You have three read-only tools: list_directory, "
        "search_file, and read_file. Use list_directory to discover project "
        "structure, search_file to locate relevant lines, and read_file for "
        "full context. Avoid requesting the same file repeatedly. Never claim "
        "to have inspected content unless a tool returned it. Do not request "
        "paths outside the project. Provide a concise, concrete answer after "
        "inspecting the necessary files.";
    const char *api_key;
    const char *model;
    char *previous_id;
    char *tool_output;
    char *call_id;
    unsigned int turn;
    openai_file_cache_entry cache[OPENAI_AGENT_CACHE_SIZE];

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT goal");
        return;
    }

    api_key = getenv("OPENAI_API_KEY");
    model = getenv("OVMS_AGENT_MODEL");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("OPENAI_API_KEY is not defined.");
        return;
    }

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        return;
    }

    if (!write_headers(api_key)) {
        return;
    }

    previous_id = NULL;
    tool_output = NULL;
    call_id = NULL;
    openai_cache_init(cache);

    (void)puts("Starting read-only agent...");

    for (turn = 0U; turn < OPENAI_AGENT_MAX_TURNS; ++turn) {
        char *json;
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;

        if (!write_agent_request(model,
                                 instructions,
                                 goal,
                                 previous_id,
                                 call_id,
                                 tool_output)) {
            break;
        }

        free(call_id);
        call_id = NULL;
        free(tool_output);
        tool_output = NULL;

        if (!perform_openai_request()) {
            (void)puts("OpenAI request failed.");
            break;
        }

        json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);

        if (json == NULL) {
            (void)puts("Unable to read OpenAI response.");
            break;
        }

        response_id = extract_top_level_id(json);

        if (response_id == NULL) {
            (void)puts("Response did not contain an id.");
            free(json);
            break;
        }

        free(previous_id);
        previous_id = response_id;

        if (display_output_text_from_json(json)) {
            free(json);
            remove_temporary_files();
            free(previous_id);
            openai_cache_free(cache);
            return;
        }

        name = NULL;
        new_call_id = NULL;
        arguments = NULL;

        if (!extract_function_call(json,
                                   &name,
                                   &new_call_id,
                                   &arguments)) {
            (void)puts(
                "Response contained neither output text nor a function call."
            );
            free(json);
            break;
        }

        free(json);

        if (strcmp(name, "list_directory") == 0) {
            char *display_path;

            display_path = NULL;
            tool_output = execute_list_directory_tool(
                arguments,
                &display_path
            );

            (void)printf("Tool executed: list_directory %s\n",
                         display_path != NULL ?
                             display_path : "");
            free(display_path);
        } else if (strcmp(name, "read_file") == 0) {
            int cache_hit;
            char *display_path;

            display_path = NULL;
            tool_output = execute_read_file_tool(
                arguments,
                cache,
                &cache_hit,
                &display_path
            );

            (void)printf("Tool executed: read_file %s%s\n",
                         display_path != NULL ? display_path : "",
                         cache_hit ? " [cache]" : "");
            free(display_path);
        } else if (strcmp(name, "search_file") == 0) {
            char *display_path;
            char *display_pattern;

            display_path = NULL;
            display_pattern = NULL;
            tool_output = execute_search_file_tool(
                arguments,
                &display_path,
                &display_pattern
            );

            (void)printf("Tool executed: search_file %s \"%s\"\n",
                         display_path != NULL ? display_path : "",
                         display_pattern != NULL ?
                             display_pattern : "");
            free(display_path);
            free(display_pattern);
        } else {
            tool_output = make_tool_error(
                "Unsupported tool requested",
                name
            );
            (void)printf("Unsupported tool requested: %s\n", name);
        }

        free(name);
        free(arguments);

        if (tool_output == NULL) {
            free(new_call_id);
            (void)puts("Unable to produce tool output.");
            break;
        }

        call_id = new_call_id;
    }

    (void)puts(
        "Agent stopped before producing a final answer "
        "(tool-turn limit or error)."
    );

    remove_temporary_files();
    free(previous_id);
    free(call_id);
    free(tool_output);
    openai_cache_free(cache);
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
