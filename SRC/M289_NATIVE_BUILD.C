#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "llm_internal.h"
#include "M289_BUILD_PROFILE.H"
#include "M289_NATIVE_BUILD.H"

#define M289_STAGE_OUTPUT_MAX 12288U
#define M289_STATUS_TEXT_MAX 128U

static int m289_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)toupper((unsigned char)*left++);
        b = (unsigned char)toupper((unsigned char)*right++);
        if (a != b) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int m289_starts_command(const char *command, const char *prefix)
{
    size_t length;

    if (command == NULL || prefix == NULL) {
        return 0;
    }

    length = strlen(prefix);
    return strncmp(command, prefix, length) == 0 &&
           (command[length] == '\0' ||
            command[length] == ' ' ||
            command[length] == '\t');
}

int m289_command_allowed(const char *command, int phase)
{
    const unsigned char *cursor;

    if (command == NULL || *command == '\0') {
        return 0;
    }

    if (phase == M289_BUILD_COMPILE) {
        if (!m289_starts_command(command, "MACRO/MIGRATION")) {
            return 0;
        }
    } else if (phase == M289_BUILD_LINK) {
        if (!m289_starts_command(command, "LINK")) {
            return 0;
        }
    } else {
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

static int m289_append(char *output,
                       size_t output_size,
                       const char *text)
{
    size_t used;
    size_t length;

    if (output == NULL || output_size == 0U || text == NULL) {
        return 0;
    }

    used = strlen(output);
    length = strlen(text);
    if (used + length >= output_size) {
        return 0;
    }

    (void)memcpy(output + used, text, length + 1U);
    return 1;
}

static int m289_append_status(char *output,
                              size_t output_size,
                              const char *label,
                              unsigned long status)
{
    char line[M289_STATUS_TEXT_MAX];
    int written;

    written = snprintf(line, sizeof(line),
                       "%s%%X%08lX (%s)\n",
                       label,
                       status,
                       (status & 1UL) != 0UL ? "success" : "failure");
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return 0;
    }

    return m289_append(output, output_size, line);
}

static int m289_load_macro_profile(m289_build_profile *profile,
                                   char *error,
                                   size_t error_size)
{
    if (m289_profile_load("[.LANGUAGE]MACRO32.BUILD",
                          profile, error, error_size)) {
        return 1;
    }

    return m289_profile_load("LANGUAGE/MACRO32.BUILD",
                             profile, error, error_size);
}

static char *m289_make_error(const char *message,
                             const char *detail)
{
    char *result;
    size_t size;

    if (message == NULL) {
        message = "M289 native source build failed.";
    }
    if (detail == NULL) {
        detail = "";
    }

    size = strlen(message) + strlen(detail) + 4U;
    result = (char *)malloc(size);
    if (result == NULL) {
        return NULL;
    }

    (void)strcpy(result, message);
    if (*detail != '\0') {
        (void)strcat(result, ": ");
        (void)strcat(result, detail);
    }
    (void)strcat(result, "\n");
    return result;
}

char *m289_build_source(agent_state *state,
                        const char *source,
                        unsigned long *status_out)
{
    m289_build_profile profile;
    char compile_command[M289_PROFILE_CMD_MAX];
    char link_command[M289_PROFILE_CMD_MAX];
    char compile_output[M289_STAGE_OUTPUT_MAX];
    char link_output[M289_STAGE_OUTPUT_MAX];
    char error[M289_PROFILE_ERROR_MAX];
    char *result;
    unsigned long compile_status;
    unsigned long link_status;
    int executed;

    if (status_out != NULL) {
        *status_out = 0UL;
    }

    if (state == NULL || source == NULL || *source == '\0' ||
        status_out == NULL) {
        return m289_make_error("M289 native source build refused",
                               "invalid arguments");
    }

    if (!llm_path_is_safe(source)) {
        return m289_make_error("M289 native source build refused",
                               "unsafe project-relative source path");
    }

    if (!m289_load_macro_profile(&profile, error, sizeof(error))) {
        return m289_make_error("Unable to load MACRO32 build profile",
                               error);
    }

    if (!m289_equal_ci(profile.language, "MACRO32") ||
        !m289_equal_ci(profile.kind, "compiled")) {
        return m289_make_error("M289 native source build refused",
                               "profile is not compiled MACRO32");
    }

    if (!m289_profile_commands(&profile, source,
                               compile_command, sizeof(compile_command),
                               link_command, sizeof(link_command),
                               error, sizeof(error))) {
        return m289_make_error("Unable to resolve MACRO32 build commands",
                               error);
    }

    if (!m289_command_allowed(compile_command, M289_BUILD_COMPILE) ||
        !m289_command_allowed(link_command, M289_BUILD_LINK)) {
        return m289_make_error("M289 native source build refused",
                               "resolved command failed execution guard");
    }

    result = (char *)malloc(M289_NATIVE_RESULT_MAX);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';

    if (!m289_append(result, M289_NATIVE_RESULT_MAX,
                     "M289 native source build\n") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX,
                     "Language: MACRO32\n") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX,
                     "Source: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, source) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX,
                     "Compile command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, compile_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }

    compile_output[0] = '\0';
    compile_status = 0UL;
    executed = command_dcl_exec(state,
                                compile_command,
                                compile_output,
                                sizeof(compile_output),
                                &compile_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Compile execution refused or unavailable.\n");
        if (compile_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                              compile_output);
        }
        *status_out = 0UL;
        return result;
    }

    (void)m289_append_status(result, M289_NATIVE_RESULT_MAX,
                             "Compile status: ", compile_status);
    if (compile_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Compile output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          compile_output);
        if (compile_output[strlen(compile_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }

    if ((compile_status & 1UL) == 0UL) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Link: not run because compile failed.\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Result: failure\n");
        *status_out = compile_status;
        return result;
    }

    if (!m289_append(result, M289_NATIVE_RESULT_MAX,
                     "Link command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, link_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }

    link_output[0] = '\0';
    link_status = 0UL;
    executed = command_dcl_exec(state,
                                link_command,
                                link_output,
                                sizeof(link_output),
                                &link_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Link execution refused or unavailable.\n");
        if (link_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                              link_output);
        }
        *status_out = 0UL;
        return result;
    }

    (void)m289_append_status(result, M289_NATIVE_RESULT_MAX,
                             "Link status: ", link_status);
    if (link_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Link output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          link_output);
        if (link_output[strlen(link_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }

    *status_out = link_status;
    (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                      (link_status & 1UL) != 0UL ?
                          "Result: success\n" :
                          "Result: failure\n");
    return result;
}
