#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_JSON_PARSE.H"
#include "LLM_RESPONSE.H"

int save_response_id(const char *json)
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

int display_clean_response(void)
{
    char *json;
    const char *text_value;
    char *decoded;

    json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);

    if (json == NULL) {
        return 0;
    }

    (void)save_response_id(json);

    decoded = extract_output_text_from_json(json);

    if (decoded != NULL) {
        (void)puts("");
        (void)puts(decoded);
        free(decoded);
        free(json);
        return 1;
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

void display_raw_response(void)
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

int display_api_error_from_json(const char *json)
{
    const char *message_value;
    char *decoded;

    message_value = find_string_value(json, "message");

    if (message_value == NULL) {
        return 0;
    }

    decoded = json_decode_string(message_value, NULL);

    if (decoded == NULL) {
        return 0;
    }

    (void)printf("OpenAI API error: %s\n", decoded);
    free(decoded);
    return 1;
}

int display_output_text_from_json(const char *json)
{
    char *decoded;

    decoded = extract_output_text_from_json(json);

    if (decoded == NULL) {
        return 0;
    }

    (void)puts("");
    (void)puts(decoded);
    free(decoded);
    return 1;
}
