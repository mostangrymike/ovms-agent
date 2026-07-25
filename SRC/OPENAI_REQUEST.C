#include "openai_internal.h"

int write_request(const char *model,
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

    if (success &&
        fputs(
            ",\"parallel_tool_calls\":false"
            "}\n",
            file
        ) == EOF) {
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

int write_headers(const char *api_key)
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

void remove_temporary_files(void)
{
    (void)remove(OPENAI_REQUEST_FILE);
    (void)remove(OPENAI_HEADERS_FILE);
}

void openai_send(agent_state *state,
                        const char *prompt,
                        int continue_conversation)
{
    const char *api_key;
    const char *model;
    const char *previous_id;
    int status;

    (void)state;

    openai_last_workflow = continue_conversation ?
        OPENAI_WORKFLOW_CHAT : OPENAI_WORKFLOW_ASK;

    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );

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

int write_agent_tools(FILE *file)
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
        "\"name\":\"read_file_range\","
        "\"description\":\"Read an inclusive line range from one project-relative text file. Use this for large files.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"end_line\":{\"type\":\"integer\",\"minimum\":1}"
        "},"
        "\"required\":[\"path\",\"start_line\",\"end_line\"],"
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

int write_agent_tools_with_replace(FILE *file)
{
    return fputs(
        "\"tools\":["
        "{"
        "\"type\":\"function\","
        "\"name\":\"list_directory\","
        "\"description\":\"List entries in one project-relative directory. Use dot for the project root.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"read_file\","
        "\"description\":\"Read one project-relative text file.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"read_file_range\","
        "\"description\":\"Read an inclusive line range from one project-relative text file. Use this for large files.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"end_line\":{\"type\":\"integer\",\"minimum\":1}"
        "},"
        "\"required\":[\"path\",\"start_line\",\"end_line\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"search_file\","
        "\"description\":\"Search one project-relative text file for a literal string.\","
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
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"replace_text\","
        "\"description\":\"Replace one exact, unique text block in a project-relative file. The local user must confirm before the patch is written.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"old_text\":{\"type\":\"string\"},"
        "\"new_text\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\",\"old_text\",\"new_text\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"replace_lines\","
        "\"description\":\"Replace one inclusive line range in a project-relative text file. The local user must confirm before the patch is written.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"first_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"last_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"new_text\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\",\"first_line\",\"last_line\",\"new_text\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "}"
        "]",
        file
    ) != EOF;
}

int write_agent_tools_with_create(FILE *file)
{
    return fputs(
        "\"tools\":["
        "{"
        "\"type\":\"function\","
        "\"name\":\"list_directory\","
        "\"description\":\"List entries in one project-relative directory. Use dot for the project root.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"read_file\","
        "\"description\":\"Read one existing project-relative text file.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"read_file_range\","
        "\"description\":\"Read an inclusive line range from one project-relative text file. Use this for large files.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"end_line\":{\"type\":\"integer\",\"minimum\":1}"
        "},"
        "\"required\":[\"path\",\"start_line\",\"end_line\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"search_file\","
        "\"description\":\"Search one existing project-relative text file for a literal string.\","
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
        "},"
        "{"
        "\"type\":\"function\","
        "\"name\":\"create_file\","
        "\"description\":\"Create one new project-relative text file. The destination must not already exist, content is limited to 65536 bytes, and the local user must confirm before creation.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{"
        "\"path\":{\"type\":\"string\"},"
        "\"content\":{\"type\":\"string\"}"
        "},"
        "\"required\":[\"path\",\"content\"],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "}"
        "]",
        file
    ) != EOF;
}

int write_create_agent_request(
    const char *model,
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
            fputs("\"}],", file) == EOF) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF) {
            success = 0;
        }
    }

    if (success && !write_agent_tools_with_create(file)) {
        success = 0;
    }

    /*
     * The first response may inspect existing project files. Once context
     * exists, require the model to invoke create_file instead of asking for
     * confirmation in ordinary text.
     */
    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(
                ",\"tool_choice\":{"
                "\"type\":\"function\","
                "\"name\":\"create_file\""
                "}",
                file
            ) == EOF) {
            success = 0;
        }
    }

    if (success &&
        fputs(
            ",\"parallel_tool_calls\":false"
            "}\n",
            file
        ) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write complete create-agent request.");
        return 0;
    }

    return 1;
}

int write_build_agent_tools(FILE *file)
{
    return fputs(
        "\"tools\":[{"
        "\"type\":\"function\","
        "\"name\":\"run_build\","
        "\"description\":\"Run the fixed project BUILD.COM command procedure "
        "and return its captured output and OpenVMS completion status. "
        "This tool accepts no command text.\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[],"
        "\"additionalProperties\":false"
        "},"
        "\"strict\":true"
        "}]",
        file
    ) != EOF;
}

int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt)
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
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\",", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"tool_choice\":{" 
              "\"type\":\"function\","
              "\"name\":\"run_build\""
              "},\"store\":true,"
              "\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write initial build-agent request.");
        return 0;
    }

    return 1;
}

int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *context_items,
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
        fputs("\",\"input\":[", file) == EOF ||
        fputs(context_items, file) == EOF ||
        fputs(",{\"type\":\"function_call_output\","
              "\"call_id\":\"", file) == EOF ||
        !json_write_escaped(file, call_id) ||
        fputs("\",\"output\":\"", file) == EOF ||
        !json_write_escaped(file, tool_output) ||
        fputs("\"}],", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"store\":true,"
              "\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write build-agent follow-up request.");
        return 0;
    }

    return 1;
}

int write_agent_request_mode(const char *model,
                                    const char *instructions,
                                    const char *user_prompt,
                                    const char *previous_id,
                                    const char *call_id,
                                    const char *tool_output,
                                    int allow_write)
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
            fputs("\"}],", file) == EOF) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF) {
            success = 0;
        }
    }

    if (success) {
        if (allow_write) {
            success = write_agent_tools_with_replace(file);
        } else {
            success = write_agent_tools(file);
        }
    }

    if (success &&
        allow_write &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"tool_choice\":\"required\"", file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        fputs(
            ",\"parallel_tool_calls\":false"
            "}\n",
            file
        ) == EOF) {
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

int write_agent_request(const char *model,
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

    if (success &&
        fputs(
            ",\"parallel_tool_calls\":false"
            "}\n",
            file
        ) == EOF) {
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

int perform_openai_request(void)
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

