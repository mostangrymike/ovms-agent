#include "llm_internal.h"

#define LLM_INSTR_FILE "OVMS_AGENT_INSTRUCTIONS.TXT"
#define OPENAI_INSTR_MAX 4096U
#define LLM_INSTR_PATH 256U
#define OPENAI_INSTR_PROMPT 12288U

static char llm_instr_data[OPENAI_INSTR_MAX];
static char llm_instr_file[LLM_INSTR_PATH];
static char llm_instr_test_path[LLM_INSTR_PATH];
static size_t llm_instr_bytes = 0U;
static int llm_instr_state = 0;
static int llm_instr_truncated = 0;

static int llm_instr_make_path(const agent_state *state,
                                  char *output,
                                  size_t output_size)
{
    const char *root;
    size_t length;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (llm_instr_test_path[0] != '\0') {
        (void)strncpy(
            output, llm_instr_test_path, output_size - 1U
        );
        output[output_size - 1U] = '\0';
        return 1;
    }

    root = state != NULL ? state->project_root : NULL;

    if (root == NULL || *root == '\0' ||
        strcmp(root, ".") == 0) {
        (void)strncpy(
            output, LLM_INSTR_FILE, output_size - 1U
        );
        output[output_size - 1U] = '\0';
        return 1;
    }

    length = strlen(root);

    if (root[length - 1U] == ']' ||
        root[length - 1U] == '>' ||
        root[length - 1U] == ':') {
        written = snprintf(
            output, output_size, "%s%s",
            root, LLM_INSTR_FILE
        );
    } else if (root[length - 1U] == '/') {
        written = snprintf(
            output, output_size, "%s%s",
            root, LLM_INSTR_FILE
        );
    } else {
        written = snprintf(
            output, output_size, "%s/%s",
            root, LLM_INSTR_FILE
        );
    }

    return written >= 0 && (size_t)written < output_size;
}

static int llm_instr_load(const agent_state *state)
{
    FILE *file;
    char path[LLM_INSTR_PATH];
    size_t used;
    int ch;

    llm_instr_data[0] = '\0';
    llm_instr_bytes = 0U;
    llm_instr_truncated = 0;

    if (!llm_instr_make_path(
            state, path, sizeof(path))) {
        llm_instr_state = -1;
        llm_instr_file[0] = '\0';
        return 0;
    }

    (void)strncpy(
        llm_instr_file, path,
        sizeof(llm_instr_file) - 1U
    );
    llm_instr_file[
        sizeof(llm_instr_file) - 1U
    ] = '\0';

    file = fopen(path, "r");

    if (file == NULL) {
        llm_instr_state = 1;
        return 1;
    }

    used = 0U;

    while ((ch = fgetc(file)) != EOF) {
        if (used + 1U < sizeof(llm_instr_data)) {
            if (ch == '\r') {
                continue;
            }
            llm_instr_data[used++] = (char)ch;
        } else {
            llm_instr_truncated = 1;
        }
    }

    if (ferror(file)) {
        (void)fclose(file);
        llm_instr_state = -1;
        llm_instr_data[0] = '\0';
        llm_instr_bytes = 0U;
        return 0;
    }

    (void)fclose(file);

    while (used > 0U &&
           (llm_instr_data[used - 1U] == '\n' ||
            llm_instr_data[used - 1U] == ' ' ||
            llm_instr_data[used - 1U] == '\t')) {
        --used;
    }

    llm_instr_data[used] = '\0';
    llm_instr_bytes = used;
    llm_instr_state = 2;

    return 1;
}

static int llm_instr_ensure(const agent_state *state)
{
    if (llm_instr_state == 0) {
        return llm_instr_load(state);
    }

    return llm_instr_state >= 1;
}

int openai_instr_reload(const agent_state *state)
{
    llm_instr_state = 0;
    return llm_instr_load(state);
}

int openai_instr_compose(const agent_state *state,
                         const char *goal,
                         char *output,
                         size_t output_size)
{
    int written;

    if (goal == NULL || *goal == '\0' ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (!llm_instr_ensure(state)) {
        return 0;
    }

    if (llm_instr_state != 2 ||
        llm_instr_bytes == 0U) {
        written = snprintf(
            output, output_size, "%s", goal
        );
        return written >= 0 &&
               (size_t)written < output_size;
    }

    written = snprintf(
        output, output_size,
        "PROJECT INSTRUCTIONS\n"
        "--------------------\n"
        "%s\n\n"
        "CURRENT REQUEST\n"
        "---------------\n"
        "%s",
        llm_instr_data,
        goal
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_instr_status_text(const agent_state *state,
                             char *output,
                             size_t output_size)
{
    const char *status;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (!llm_instr_ensure(state)) {
        status = "error";
    } else if (llm_instr_state == 2) {
        status = "loaded";
    } else {
        status = "not found";
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent project instructions\n"
        "-------------------------------\n"
        "Filespec:  %s\n"
        "Status:    %s\n"
        "Bytes:     %lu\n"
        "Truncated: %s\n"
        "Limit:     %u bytes\n",
        llm_instr_file[0] != '\0' ?
            llm_instr_file : "(unresolved)",
        status,
        (unsigned long)llm_instr_bytes,
        llm_instr_truncated ? "yes" : "no",
        (unsigned int)(OPENAI_INSTR_MAX - 1U)
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_instr_show_text(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (!llm_instr_ensure(state)) {
        return 0;
    }

    if (llm_instr_state != 2 ||
        llm_instr_bytes == 0U) {
        written = snprintf(
            output, output_size,
            "No project instructions are loaded.\n"
            "Expected filespec: %s\n",
            llm_instr_file[0] != '\0' ?
                llm_instr_file : LLM_INSTR_FILE
        );

        return written >= 0 &&
               (size_t)written < output_size;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent project instructions\n"
        "-------------------------------\n"
        "%s\n",
        llm_instr_data
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

void openai_show_instr_status(const agent_state *state)
{
    char output[1024];

    if (!openai_instr_status_text(
            state, output, sizeof(output))) {
        (void)puts("Unable to show project instruction status.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_instr(const agent_state *state)
{
    char output[OPENAI_INSTR_MAX + 256U];

    if (!openai_instr_show_text(
            state, output, sizeof(output))) {
        (void)puts("Unable to show project instructions.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_instr_reload_cmd(const agent_state *state)
{
    if (!openai_instr_reload(state)) {
        (void)puts("Project instruction reload failed.");
        return;
    }

    openai_show_instr_status(state);
}

void openai_test_instr_path(const char *path)
{
    llm_instr_state = 0;
    llm_instr_data[0] = '\0';
    llm_instr_file[0] = '\0';
    llm_instr_bytes = 0U;
    llm_instr_truncated = 0;

    if (path == NULL || *path == '\0') {
        llm_instr_test_path[0] = '\0';
        return;
    }

    (void)strncpy(
        llm_instr_test_path,
        path,
        sizeof(llm_instr_test_path) - 1U
    );
    llm_instr_test_path[
        sizeof(llm_instr_test_path) - 1U
    ] = '\0';
}
