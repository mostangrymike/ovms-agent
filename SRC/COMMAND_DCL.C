#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"
#include "command_internal.h"
#include "llm_internal.h"

#define DCL_COMMAND_MAX 768U
#define DCL_OUTPUT_MAX 32768U
#define DCL_PATH_MAX 256U

static void dcl_remove_versions(const char *path)
{
    if (path == NULL) {
        return;
    }

    while (remove(path) == 0) {
    }
}

static int dcl_starts_word(const char *text, const char *word)
{
    size_t length;

    if (text == NULL || word == NULL) {
        return 0;
    }

    length = strlen(word);
    return strncmp(text, word, length) == 0 &&
           (text[length] == '\0' ||
            text[length] == ' ' ||
            text[length] == '\t' ||
            text[length] == '/');
}

static int dcl_command_valid(const char *command)
{
    char upper[DCL_COMMAND_MAX];
    const unsigned char *cursor;
    size_t length;

    static const char *reserved[] = {
        "EXIT", "LOGOUT", "STOP", "GOTO", "CALL",
        "RETURN", "SUBROUTINE", "SPAWN", "SUBMIT", "PIPE"
    };
    unsigned int index;

    if (command == NULL || *command == '\0') {
        return 0;
    }

    length = strlen(command);
    if (length >= sizeof(upper)) {
        return 0;
    }

    cursor = (const unsigned char *)command;
    while (*cursor != (unsigned char)'\0') {
        if (*cursor < 32U || *cursor == 127U ||
            *cursor == (unsigned char)'@') {
            return 0;
        }
        ++cursor;
    }

    for (index = 0U; index < length; ++index) {
        upper[index] = (char)toupper((unsigned char)command[index]);
    }
    upper[length] = '\0';

    for (index = 0U;
         index < (unsigned int)(sizeof(reserved) / sizeof(reserved[0]));
         ++index) {
        if (dcl_starts_word(upper, reserved[index])) {
            return 0;
        }
    }

    return 1;
}

static int dcl_suffix_ci(const char *text, const char *suffix)
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

static int dcl_procedure_path_valid(const char *path)
{
    const unsigned char *cursor;
    size_t length;

    if (path == NULL || *path == '\0' ||
        strstr(path, "..") != NULL || strchr(path, ':') != NULL ||
        path[0] == '/' || path[0] == '\\' ||
        !dcl_suffix_ci(path, ".COM")) {
        return 0;
    }

    length = strlen(path);
    if (length >= DCL_COMMAND_MAX) {
        return 0;
    }

    cursor = (const unsigned char *)path;
    while (*cursor != (unsigned char)'\0') {
        if (*cursor < 32U || *cursor == 127U ||
            *cursor == (unsigned char)' ' ||
            *cursor == (unsigned char)'\t' ||
            *cursor == (unsigned char)'@' ||
            *cursor == (unsigned char)'|' ||
            *cursor == (unsigned char)'&' ||
            *cursor == (unsigned char)'\'' ||
            *cursor == (unsigned char)'"' ||
            *cursor == (unsigned char)'!' ||
            *cursor == (unsigned char)';') {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static int dcl_make_paths(char *procedure,
                          size_t procedure_size,
                          char *output,
                          size_t output_size)
{
    unsigned long pid;
    int written;

    pid = (unsigned long)getpid();

    written = snprintf(
        procedure, procedure_size,
        "SYS$SCRATCH:OVMS_DCL_%08lX.COM",
        pid);
    if (written < 0 || (size_t)written >= procedure_size) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "SYS$SCRATCH:OVMS_DCL_%08lX.OUT",
        pid);
    return written >= 0 && (size_t)written < output_size;
}

static int dcl_read_output(const char *path,
                           char *output,
                           size_t output_size)
{
    FILE *file;
    char line[1024];
    size_t used;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    used = 0U;
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;

        length = strlen(line);
        if (used + length + 1U >= output_size) {
            static const char truncated[] = "[output truncated]\n";
            size_t remain;

            remain = output_size - used;
            if (remain > 1U) {
                (void)strncpy(output + used, truncated, remain - 1U);
                output[output_size - 1U] = '\0';
            }
            (void)fclose(file);
            return 1;
        }

        (void)memcpy(output + used, line, length);
        used += length;
        output[used] = '\0';
    }

    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int dcl_exec_wrapped(agent_state *state,
                            const char *command,
                            int procedure_mode,
                            char *output,
                            size_t output_size,
                            unsigned long *status_out)
{
    char procedure_path[DCL_PATH_MAX];
    char output_path[DCL_PATH_MAX];
    char invoke[DCL_PATH_MAX + 2U];
    char display[DCL_COMMAND_MAX + 2U];
    char result_note[1024];
    FILE *procedure;
    int status;
    int written;

    if (output != NULL && output_size != 0U) {
        output[0] = '\0';
    }
    if (status_out != NULL) {
        *status_out = 0UL;
    }

    if (state == NULL || output == NULL || output_size == 0U ||
        status_out == NULL ||
        (procedure_mode ? !dcl_procedure_path_valid(command) :
                          !dcl_command_valid(command))) {
        return 0;
    }

    if (strcmp(llm_approval_name(), "full") != 0) {
        (void)snprintf(output, output_size,
                       "DCL command refused: full approval policy required.\n");
        llm_tx_model_result("DCL", "refused", output);
        return 0;
    }

    if (!state->write_enabled) {
        (void)snprintf(output, output_size,
                       "DCL command refused: write gate is disabled.\n");
        llm_tx_model_result("DCL", "write-gate", output);
        return 0;
    }

    if (!state->dcl_enabled) {
        (void)snprintf(output, output_size,
                       "DCL command refused: DCL gate is disabled.\n");
        llm_tx_model_result("DCL", "dcl-gate", output);
        return 0;
    }

    if (!dcl_make_paths(procedure_path, sizeof(procedure_path),
                        output_path, sizeof(output_path))) {
        return 0;
    }

    dcl_remove_versions(procedure_path);
    dcl_remove_versions(output_path);

    procedure = fopen(procedure_path, "w");
    if (procedure == NULL) {
        return 0;
    }

    if (procedure_mode) {
        written = fprintf(
            procedure,
            "$ DEFINE SYS$OUTPUT %s\n"
            "$ DEFINE SYS$ERROR SYS$OUTPUT\n"
            "$ @%s\n"
            "$ OVMS_DCL_STATUS = $STATUS\n"
            "$ EXIT 'OVMS_DCL_STATUS'\n",
            output_path,
            command);
        (void)snprintf(display, sizeof(display), "@%s", command);
    } else {
        written = fprintf(
            procedure,
            "$ DEFINE SYS$OUTPUT %s\n"
            "$ DEFINE SYS$ERROR SYS$OUTPUT\n"
            "$ %s\n"
            "$ OVMS_DCL_STATUS = $STATUS\n"
            "$ EXIT 'OVMS_DCL_STATUS'\n",
            output_path,
            command);
        (void)snprintf(display, sizeof(display), "%s", command);
    }

    if (written < 0) {
        (void)fclose(procedure);
        dcl_remove_versions(procedure_path);
        return 0;
    }

    if (fclose(procedure) != 0) {
        dcl_remove_versions(procedure_path);
        return 0;
    }

    llm_tx_model_call("DCL", display);

    written = snprintf(invoke, sizeof(invoke), "@%s", procedure_path);
    if (written < 0 || (size_t)written >= sizeof(invoke)) {
        dcl_remove_versions(procedure_path);
        return 0;
    }

    status = system(invoke);
    *status_out = (unsigned long)(unsigned int)status;

    if (!dcl_read_output(output_path, output, output_size)) {
        dcl_remove_versions(procedure_path);
        dcl_remove_versions(output_path);
        return 0;
    }

    (void)snprintf(
        result_note, sizeof(result_note),
        "status=%%X%08lX success=%s %s",
        *status_out,
        ((*status_out & 1UL) != 0UL) ? "yes" : "no",
        output);

    llm_tx_model_result(
        "DCL",
        ((*status_out & 1UL) != 0UL) ? "success" : "failed",
        result_note);
    llm_log_event("DCL", display, status);

    dcl_remove_versions(procedure_path);
    dcl_remove_versions(output_path);
    return 1;
}

int command_dcl_exec(agent_state *state,
                     const char *command,
                     char *output,
                     size_t output_size,
                     unsigned long *status_out)
{
    return dcl_exec_wrapped(state, command, 0,
                            output, output_size, status_out);
}

int command_dcl_exec_procedure(agent_state *state,
                               const char *procedure_path,
                               char *output,
                               size_t output_size,
                               unsigned long *status_out)
{
    return dcl_exec_wrapped(state, procedure_path, 1,
                            output, output_size, status_out);
}

void command_dcl(agent_state *state, const char *arguments)
{
    char output[DCL_OUTPUT_MAX];
    unsigned long status;

    if (!command_dcl_exec(state, arguments,
                          output, sizeof(output), &status)) {
        if (output[0] != '\0') {
            (void)fputs(output, stdout);
        } else {
            (void)puts("Usage: DCL <single DCL command>");
        }
        return;
    }

    if (output[0] != '\0') {
        (void)fputs(output, stdout);
        if (strchr(output, '\n') == NULL) {
            (void)putchar('\n');
        }
    }

    (void)printf(
        "OpenVMS completion status: %%X%08lX (%s)\n",
        status,
        (status & 1UL) != 0UL ? "success" : "failure");
}
