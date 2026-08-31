#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "llm_internal.h"
#include "M289_NATIVE_BUILD.H"

#define TEST_COMMAND_MAX 512U

static int exec_mode = 0;
static unsigned int exec_calls = 0U;
static char exec_first[TEST_COMMAND_MAX];
static char exec_second[TEST_COMMAND_MAX];

int llm_path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }
    if (strstr(path, "..") != NULL ||
        strchr(path, ':') != NULL ||
        path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    return 1;
}

int command_dcl_exec(agent_state *state,
                     const char *command,
                     char *output,
                     size_t output_size,
                     unsigned long *status_out)
{
    const char *text;

    (void)state;
    if (command == NULL || output == NULL || output_size == 0U ||
        status_out == NULL) {
        return 0;
    }

    ++exec_calls;
    if (exec_calls == 1U) {
        (void)strncpy(exec_first, command, sizeof(exec_first) - 1U);
        exec_first[sizeof(exec_first) - 1U] = '\0';
    } else if (exec_calls == 2U) {
        (void)strncpy(exec_second, command, sizeof(exec_second) - 1U);
        exec_second[sizeof(exec_second) - 1U] = '\0';
    }

    if (exec_mode == 3) {
        (void)snprintf(output, output_size,
                       "DCL command refused: full approval policy required.\n");
        *status_out = 0UL;
        return 0;
    }

    if (exec_calls == 1U) {
        if (exec_mode == 1) {
            *status_out = 2UL;
            text = "MACRO compile failed\n";
        } else {
            *status_out = 0x107D0001UL;
            text = "MACRO compile output\n";
        }
    } else {
        if (exec_mode == 2) {
            *status_out = 2UL;
            text = "LINK failed\n";
        } else {
            *status_out = 0x10000001UL;
            text = "LINK output\n";
        }
    }

    (void)strncpy(output, text, output_size - 1U);
    output[output_size - 1U] = '\0';
    return 1;
}

static void reset_exec(int mode)
{
    exec_mode = mode;
    exec_calls = 0U;
    exec_first[0] = '\0';
    exec_second[0] = '\0';
}

static int test_command_guard(void)
{
    if (!m289_command_allowed("MACRO/MIGRATION WC.MAR",
                              M289_BUILD_COMPILE) ||
        !m289_command_allowed("LINK WC.OBJ", M289_BUILD_LINK) ||
        m289_command_allowed("MACRO WC.MAR", M289_BUILD_COMPILE) ||
        m289_command_allowed("MACRO/MIGRATION WC.MAR|DELETE *.*;*",
                             M289_BUILD_COMPILE) ||
        m289_command_allowed("LINK WC.OBJ @EVIL.COM",
                             M289_BUILD_LINK) ||
        m289_command_allowed("LINK SYS$DISK:[X]WC.OBJ",
                             M289_BUILD_LINK)) {
        (void)puts("M289 native failed: command guard.");
        return 0;
    }
    return 1;
}

static int test_success(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    reset_exec(0);
    status = 0UL;
    result = m289_build_source(state, "WC.MAR", &status);
    ok = result != NULL &&
         exec_calls == 2U &&
         strcmp(exec_first, "MACRO/MIGRATION WC.MAR") == 0 &&
         strcmp(exec_second, "LINK WC.OBJ") == 0 &&
         status == 0x10000001UL &&
         strstr(result, "Compile status: %X107D0001 (success)") != NULL &&
         strstr(result, "Link status: %X10000001 (success)") != NULL &&
         strstr(result, "Result: success") != NULL;
    if (!ok) {
        (void)printf("M289 native failed: success path.\n%s\n",
                     result != NULL ? result : "<null>");
    }
    free(result);
    return ok;
}

static int test_compile_failure(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    reset_exec(1);
    status = 0UL;
    result = m289_build_source(state, "WC.MAR", &status);
    ok = result != NULL &&
         exec_calls == 1U &&
         status == 2UL &&
         strstr(result, "Link: not run because compile failed.") != NULL &&
         strstr(result, "Result: failure") != NULL;
    if (!ok) {
        (void)puts("M289 native failed: compile-failure sequencing.");
    }
    free(result);
    return ok;
}

static int test_link_failure(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    reset_exec(2);
    status = 0UL;
    result = m289_build_source(state, "WC.MAR", &status);
    ok = result != NULL &&
         exec_calls == 2U &&
         status == 2UL &&
         strstr(result, "Link status: %X00000002 (failure)") != NULL &&
         strstr(result, "Result: failure") != NULL;
    if (!ok) {
        (void)puts("M289 native failed: link-failure sequencing.");
    }
    free(result);
    return ok;
}

static int test_refusal(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    reset_exec(3);
    status = 99UL;
    result = m289_build_source(state, "WC.MAR", &status);
    ok = result != NULL &&
         exec_calls == 1U &&
         status == 0UL &&
         strstr(result, "Compile execution refused or unavailable.") != NULL &&
         strstr(result, "full approval policy required") != NULL;
    if (!ok) {
        (void)puts("M289 native failed: DCL refusal propagation.");
    }
    free(result);
    return ok;
}

static int test_invalid_source(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    reset_exec(0);
    status = 0UL;
    result = m289_build_source(state, "../WC.MAR", &status);
    ok = result != NULL && exec_calls == 0U &&
         strstr(result, "unsafe project-relative source path") != NULL;
    free(result);
    if (!ok) {
        (void)puts("M289 native failed: unsafe source path.");
        return 0;
    }

    reset_exec(0);
    result = m289_build_source(state, "WC.C", &status);
    ok = result != NULL && exec_calls == 0U &&
         strstr(result, "Source extension is not allowed") != NULL;
    free(result);
    if (!ok) {
        (void)puts("M289 native failed: non-MACRO32 source rejection.");
        return 0;
    }
    return 1;
}

int main(void)
{
    agent_state state;
    int ok;

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";
    state.write_enabled = 1;
    state.dcl_enabled = 1;

    ok = test_command_guard() &&
         test_success(&state) &&
         test_compile_failure(&state) &&
         test_link_failure(&state) &&
         test_refusal(&state) &&
         test_invalid_source(&state);

    if (!ok) {
        (void)puts("M289 native build executor test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M289 native build executor test passed.");
    return EXIT_SUCCESS;
}
