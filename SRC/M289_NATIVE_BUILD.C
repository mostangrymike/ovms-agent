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
    "CXX",
    "PYTHON",
    "PERL",
    "JAVA",
    "DCL",
    "BASIC",
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

static int m289_suffix_ci(const char *text, const char *suffix)
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

int m289_command_allowed(const char *command,
                         int phase,
                         const char *language)
{
    const unsigned char *cursor;
    int verb_ok;
    int language_ok;
    int interpreted;
    int compile_only;
    int procedure;

    if (command == NULL || *command == '\0' ||
        language == NULL || *language == '\0') {
        return 0;
    }

    language_ok = m289_equal_ci(language, "MACRO32") ||
                  m289_equal_ci(language, "FORTRAN") ||
                  m289_equal_ci(language, "COBOL") ||
                  m289_equal_ci(language, "PASCAL") ||
                  m289_equal_ci(language, "CXX") ||
                  m289_equal_ci(language, "PYTHON") ||
                  m289_equal_ci(language, "PERL") ||
                  m289_equal_ci(language, "JAVA") ||
                  m289_equal_ci(language, "DCL") ||
                  m289_equal_ci(language, "BASIC");
    if (!language_ok) {
        return 0;
    }

    interpreted = m289_equal_ci(language, "PYTHON") ||
                  m289_equal_ci(language, "PERL");
    compile_only = m289_equal_ci(language, "JAVA");
    procedure = m289_equal_ci(language, "DCL");
    verb_ok = 0;
    if (phase == M289_BUILD_COMPILE && !interpreted && !procedure) {
        if (m289_equal_ci(language, "MACRO32")) {
            verb_ok = m289_starts_command(command, "MACRO/MIGRATION");
        } else if (m289_equal_ci(language, "FORTRAN")) {
            verb_ok = m289_starts_command(command, "FORTRAN");
        } else if (m289_equal_ci(language, "COBOL")) {
            verb_ok = m289_starts_command(command, "COBOL");
        } else if (m289_equal_ci(language, "PASCAL")) {
            verb_ok = m289_starts_command(command, "PASCAL");
        } else if (m289_equal_ci(language, "CXX")) {
            verb_ok = m289_starts_command(command, "CXX");
        } else if (m289_equal_ci(language, "JAVA")) {
            verb_ok = m289_starts_command(command, "JAVAC");
        } else if (m289_equal_ci(language, "BASIC")) {
            verb_ok = m289_starts_command(command, "BASIC");
        }
    } else if (phase == M289_BUILD_LINK &&
               !interpreted && !compile_only && !procedure) {
        verb_ok = m289_starts_command(command, "LINK");
    } else if (phase == M289_BUILD_RUN && interpreted) {
        if (m289_equal_ci(language, "PYTHON")) {
            verb_ok = m289_starts_command(command, "PYTHON");
        } else if (m289_equal_ci(language, "PERL")) {
            verb_ok = m289_starts_command(command, "PERL");
        }
    } else if (phase == M289_BUILD_RUN && procedure) {
        verb_ok = m289_suffix_ci(command, ".COM") &&
                  strchr(command, ' ') == NULL &&
                  strchr(command, '\t') == NULL;
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
                                char *run_command,
                                size_t run_size,
                                char *error,
                                size_t error_size)
{
    m289_build_profile candidate;
    char load_error[M289_PROFILE_ERROR_MAX];
    char command_error[M289_PROFILE_ERROR_MAX];
    unsigned int index;
    int resolved;

    if (error != NULL && error_size > 0U) {
        error[0] = '\0';
    }
    compile_command[0] = '\0';
    link_command[0] = '\0';
    run_command[0] = '\0';

    for (index = 0U; m289_profile_names[index] != NULL; ++index) {
        load_error[0] = '\0';
        if (!m289_load_named_profile(m289_profile_names[index],
                                     &candidate,
                                     load_error,
                                     sizeof(load_error))) {
            continue;
        }

        command_error[0] = '\0';
        resolved = 0;
        if (m289_equal_ci(candidate.kind, "compiled")) {
            resolved = m289_profile_commands(&candidate, source,
                                              compile_command, compile_size,
                                              link_command, link_size,
                                              command_error,
                                              sizeof(command_error));
            if (resolved) {
                run_command[0] = '\0';
            }
        } else if (m289_equal_ci(candidate.kind, "compile_only")) {
            resolved = m289_profile_compile_command(&candidate, source,
                                                    compile_command,
                                                    compile_size,
                                                    command_error,
                                                    sizeof(command_error));
            if (resolved) {
                link_command[0] = '\0';
                run_command[0] = '\0';
            }
        } else if (m289_equal_ci(candidate.kind, "interpreted") ||
                   m289_equal_ci(candidate.kind, "procedure")) {
            resolved = m289_profile_run_command(&candidate, source,
                                                run_command, run_size,
                                                command_error,
                                                sizeof(command_error));
            if (resolved) {
                compile_command[0] = '\0';
                link_command[0] = '\0';
            }
        }
        if (resolved) {
            *profile = candidate;
            return 1;
        }
    }

    if (error != NULL && error_size > 0U) {
        (void)snprintf(error, error_size,
                       "No native profile accepts source extension.");
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

static int m289_append_header(char *result,
                              const char *language,
                              const char *source)
{
    return m289_append(result, M289_NATIVE_RESULT_MAX,
                       "M289 native source build\n") &&
           m289_append(result, M289_NATIVE_RESULT_MAX, "Language: ") &&
           m289_append(result, M289_NATIVE_RESULT_MAX, language) &&
           m289_append(result, M289_NATIVE_RESULT_MAX, "\n") &&
           m289_append(result, M289_NATIVE_RESULT_MAX, "Source: ") &&
           m289_append(result, M289_NATIVE_RESULT_MAX, source) &&
           m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
}

static char *m289_run_interpreted(agent_state *state,
                                  const m289_build_profile *profile,
                                  const char *source,
                                  const char *run_command,
                                  unsigned long *status_out)
{
    char run_output[M289_STAGE_OUTPUT_MAX];
    char *result;
    unsigned long run_status;
    int executed;

    if (!m289_command_allowed(run_command, M289_BUILD_RUN,
                              profile->language)) {
        return m289_make_error("M289 native source build refused",
                               "resolved command failed execution guard");
    }

    result = (char *)malloc(M289_NATIVE_RESULT_MAX);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';
    if (!m289_append_header(result, profile->language, source) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "Run command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, run_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }

    run_output[0] = '\0';
    run_status = 0UL;
    executed = command_dcl_exec(state, run_command,
                                run_output, sizeof(run_output),
                                &run_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Run execution refused or unavailable.\n");
        if (run_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, run_output);
        }
        *status_out = 0UL;
        return result;
    }

    (void)m289_append_status(result, M289_NATIVE_RESULT_MAX,
                             "Run status: ", run_status);
    if (run_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, "Run output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX, run_output);
        if (run_output[strlen(run_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }

    *status_out = run_status;
    (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                      (run_status & 1UL) != 0UL ?
                          "Result: success\n" :
                          "Result: failure\n");
    return result;
}

static char *m289_run_procedure(agent_state *state,
                                const m289_build_profile *profile,
                                const char *source,
                                const char *procedure_path,
                                unsigned long *status_out)
{
    char procedure_output[M289_STAGE_OUTPUT_MAX];
    char display[M289_PROFILE_CMD_MAX + 2U];
    char *result;
    unsigned long procedure_status;
    int executed;
    int written;

    if (!m289_command_allowed(procedure_path, M289_BUILD_RUN,
                              profile->language)) {
        return m289_make_error("M289 native source build refused",
                               "resolved procedure failed execution guard");
    }

    written = snprintf(display, sizeof(display), "@%s", procedure_path);
    if (written < 0 || (size_t)written >= sizeof(display)) {
        return m289_make_error("M289 native source build refused",
                               "resolved procedure display is too long");
    }

    result = (char *)malloc(M289_NATIVE_RESULT_MAX);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';
    if (!m289_append_header(result, profile->language, source) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX,
                     "Procedure command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, display) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }

    procedure_output[0] = '\0';
    procedure_status = 0UL;
    executed = command_dcl_exec_procedure(state, procedure_path,
                                          procedure_output,
                                          sizeof(procedure_output),
                                          &procedure_status);
    if (!executed) {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Procedure execution refused or unavailable.\n");
        if (procedure_output[0] != '\0') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                              procedure_output);
        }
        *status_out = 0UL;
        return result;
    }

    (void)m289_append_status(result, M289_NATIVE_RESULT_MAX,
                             "Procedure status: ", procedure_status);
    if (procedure_output[0] != '\0') {
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          "Procedure output:\n");
        (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                          procedure_output);
        if (procedure_output[strlen(procedure_output) - 1U] != '\n') {
            (void)m289_append(result, M289_NATIVE_RESULT_MAX, "\n");
        }
    }

    *status_out = procedure_status;
    (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                      (procedure_status & 1UL) != 0UL ?
                          "Result: success\n" :
                          "Result: failure\n");
    return result;
}

static char *m289_compile_only(agent_state *state,
                               const m289_build_profile *profile,
                               const char *source,
                               const char *compile_command,
                               unsigned long *status_out)
{
    char compile_output[M289_STAGE_OUTPUT_MAX];
    char *result;
    unsigned long compile_status;
    int executed;

    if (!m289_command_allowed(compile_command, M289_BUILD_COMPILE,
                              profile->language)) {
        return m289_make_error("M289 native source build refused",
                               "resolved command failed execution guard");
    }

    result = (char *)malloc(M289_NATIVE_RESULT_MAX);
    if (result == NULL) {
        return NULL;
    }
    result[0] = '\0';
    if (!m289_append_header(result, profile->language, source) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "Compile command: ") ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, compile_command) ||
        !m289_append(result, M289_NATIVE_RESULT_MAX, "\n")) {
        free(result);
        return NULL;
    }

    compile_output[0] = '\0';
    compile_status = 0UL;
    executed = command_dcl_exec(state, compile_command,
                                compile_output, sizeof(compile_output),
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

    *status_out = compile_status;
    (void)m289_append(result, M289_NATIVE_RESULT_MAX,
                      (compile_status & 1UL) != 0UL ?
                          "Result: success\n" :
                          "Result: failure\n");
    return result;
}

char *m289_build_source(agent_state *state,
                        const char *source,
                        unsigned long *status_out)
{
    m289_build_profile profile;
    char compile_command[M289_PROFILE_CMD_MAX];
    char link_command[M289_PROFILE_CMD_MAX];
    char run_command[M289_PROFILE_CMD_MAX];
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
                              run_command, sizeof(run_command),
                              error, sizeof(error))) {
        return m289_make_error("Unable to resolve native build profile",
                               error);
    }

    if (m289_equal_ci(profile.kind, "interpreted")) {
        return m289_run_interpreted(state, &profile, source,
                                    run_command, status_out);
    }
    if (m289_equal_ci(profile.kind, "procedure")) {
        return m289_run_procedure(state, &profile, source,
                                  run_command, status_out);
    }
    if (m289_equal_ci(profile.kind, "compile_only")) {
        return m289_compile_only(state, &profile, source,
                                 compile_command, status_out);
    }
    if (!m289_equal_ci(profile.kind, "compiled")) {
        return m289_make_error("M289 native source build refused",
                               "unsupported profile kind");
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

    if (!m289_append_header(result, profile.language, source) ||
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
