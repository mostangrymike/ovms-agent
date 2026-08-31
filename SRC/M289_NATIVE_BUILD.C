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
#define M289_PROFILE_FILE_MAX 128U

static const char *m289_profile_names[] = {
    "MACRO32",
    "FORTRAN",
    "COBOL",
    "PASCAL",
    NULL
};

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

int m289_command_allowed(const char *command,
                         int phase,
                         const char *language)
{
    const unsigned char *cursor;
    int verb_ok;
    int language_ok;

    if (command == NULL || *command == '\0' ||
        language == NULL || *language == '\0') {
        return 0;
    }

    language_ok = m289_equal_ci(language, "MACRO32") ||
                  m289_equal_ci(language, "FORTRAN") ||
                  m289_equal_ci(language, "COBOL") ||
                  m289_equal_ci(language, "PASCAL");
    if (!language_ok) {
        return 0;
    }

    verb_ok = 0;
    if (phase == M289_BUILD_COMPILE) {
        if (m289_equal_ci(language, "MACRO32")) {
            verb_ok = m289_starts_command(command, "MACRO/MIGRATION");
        } else if (m289_equal_ci(language, "FORTRAN")) {
            verb_ok = m289_starts_command(command, "FORTRAN");
        } else if (m289_equal_ci(language, "COBOL")) {
            verb_ok = m289_starts_command(command, "COBOL");
        } else if (m289_equal_ci(language, "PASCAL")) {
            verb_ok = m289_starts_command(command, "PASCAL");
        }
    } else if (phase == M289_BUILD_LINK) {
        verb_ok = m289_starts_command(command, "LINK");
    }

    if (!verb_ok) {
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

static int m289_profile_name_ok(const char *name,
                                const m289_build_profile *profile,
                                char *error,
                                size_t error_size)
{
    if (name != NULL && profile != NULL &&
        m289_equal_ci(name, profile->language)) {
        return 1;
    }

    if (error != NULL && error_size > 0U) {
        (void)snprintf(error, error_size,
                       "Build profile language does not match profile name.");
    }
    return 0;
}

static int m289_load_named_profile(const char *name,
                                   m289_build_profile *profile,
                                   char *error,
                                   size_t error_size)
{
    char path[M289_PROFILE_FILE_MAX];
    int written;

    if (name == NULL || *name == '\0' || profile == NULL) {
        return 0;
    }

    written = snprintf(path, sizeof(path), "[.LANGUAGE]%s.BUILD", name);
    if (written >= 0 && (size_t)written < sizeof(path) &&
        m289_profile_load(path, profile, error, error_size)) {
        return m289_profile_name_ok(name, profile, error, error_size);
    }

    written = snprintf(path, sizeof(path), "LANGUAGE/%s.BUILD", name);
    if (written < 0 || (size_t)written >= sizeof(path) ||
        !m289_profile_load(path, profile, error, error_size)) {
        return 0;
    }

    return m289_profile_name_ok(name, profile, error, error_size);
}

static int m289_resolve_profile(const char *source,
                                m289_build_profile *profile,
                                char *compile_command,
                                size_t compile_size,
                                char *link_command,
                                size_t link_size,
                                char *error,
                                size_t error_size)
{
    m289_build_profile candidate;
    char load_error[M289_PROFILE_ERROR_MAX];
    char command_error[M289_PROFILE_ERROR_MAX];
    unsigned int index;

    if (error != NULL && error_size > 0U) {
        error[0] = '\0';
    }

    for (index = 0U; m289_profile_names[index] != NULL; ++index) {
        load_error[0] = '\0';
        if (!m289_load_named_profile(m289_profile_names[index],
                                     &candidate,
                                     load_error,
                                     sizeof(load_error))) {
            continue;
        }

        command_error[0] = '\0';
        if (m289_profile_commands(&candidate, source,
                                  compile_command, compile_size,
                                  link_command, link_size,
                                  command_error, sizeof(command_error))) {
            *profile = candidate;
            return 1;
        }
    }

    if (error != NULL && error_size > 0U) {
        (void)snprintf(error, error_size,
                       "No compiled native profile accepts source extension.");
    }
    return 0;
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

    if (!m289_resolve_profile(source, &profile,
                              compile_command, sizeof(compile_command),
                              link_command, sizeof(link_command),
                              error, sizeof(error))) {
        return m289_make_error("Unable to resolve native build profile",
                               error);
    }

    if (!m289_equal_ci(profile.kind, "compiled")) {
        return m289_make_error("M289 native source build refused",
                               "profile is not compiled");
    }

    if (!m289_command_allowed(compile_command, M289_BUILD_COMPILE,
                              profile.language) ||
        !m289_command_allowed(link_command, M289_BUILD_LINK,
                              profile.language)) {
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
                     "Language: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX,
                     profile.language) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n") ||
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
