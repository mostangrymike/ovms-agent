#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "llm_internal.h"
#include "M289_NATIVE_BUILD.H"

static unsigned int m316_test_calls;

int llm_path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }
    return strstr(path, "..") == NULL && strchr(path, ':') == NULL &&
           path[0] != '/' && path[0] != '\\';
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
    ++m316_test_calls;

    if (strncmp(command, "CC ", 3U) == 0) {
        *status_out = 0x10B91262UL;
        text = "%CC-E-BADEXPR, Invalid expression.\n"
               "at line number 3 in file SYS$SYSDEVICE:[MIKE.OVMS_AGENT]M316_BAD.C;1\n";
    } else if (strncmp(command, "CXX ", 4U) == 0) {
        *status_out = 0x15F61262UL;
        text = "%CXX-E-ERROR, SYS$SYSDEVICE:[MIKE.OVMS_AGENT]M316_BAD.CXX;1:3:17: error: expected expression\n"
               "%CXX-E-ENDDIAG, 1 error generated.\n";
    } else if (strncmp(command, "FORTRAN ", 8U) == 0) {
        *status_out = 0x1035A00AUL;
        text = "%F90-E-ERROR, Syntax error\n"
               "at line number 3 in file SYS$SYSDEVICE:[MIKE.OVMS_AGENT]M316_BAD.F90;1\n";
    } else if (strncmp(command, "LINK ", 5U) == 0) {
        *status_out = 0x10000001UL;
        text = "LINK output\n";
    } else {
        *status_out = 2UL;
        text = "unexpected command\n";
    }

    (void)strncpy(output, text, output_size - 1U);
    output[output_size - 1U] = '\0';
    return 1;
}

int command_dcl_exec_procedure(agent_state *state,
                               const char *procedure_path,
                               char *output,
                               size_t output_size,
                               unsigned long *status_out)
{
    (void)state;
    (void)procedure_path;
    if (output == NULL || output_size == 0U || status_out == NULL) {
        return 0;
    }
    ++m316_test_calls;
    *status_out = 0x00038090UL;
    (void)strncpy(output,
                  "%DCL-W-IVVERB, unrecognized command verb - check validity and spelling\n",
                  output_size - 1U);
    output[output_size - 1U] = '\0';
    return 1;
}

static int m316_has(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static int test_c_failure(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    m316_test_calls = 0U;
    status = 0UL;
    result = m289_build_source(state, "M316_BAD.C", &status);
    ok = result != NULL && m316_test_calls == 1U &&
         status == 0x10B91262UL &&
         m316_has(result, "Language: C") &&
         m316_has(result, "Compile status: %X10B91262 (failure)") &&
         m316_has(result, "Link: not run because compile failed.") &&
         m316_has(result, "language=C phase=compile severity=E facility=CC id=BADEXPR") &&
         m316_has(result, "line=3 column=0 message=Invalid expression.");
    if (!ok) {
        (void)printf("M316 failed: C normalization.\n%s\n",
                     result != NULL ? result : "<null>");
    }
    free(result);
    return ok;
}

static int test_cxx_embedded(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    m316_test_calls = 0U;
    status = 0UL;
    result = m289_build_source(state, "M316_BAD.CXX", &status);
    ok = result != NULL && m316_test_calls == 1U &&
         status == 0x15F61262UL &&
         m316_has(result, "Compile status: %X15F61262 (failure)") &&
         m316_has(result, "language=CXX phase=compile severity=E facility=CXX id=ERROR") &&
         m316_has(result, "file=SYS$SYSDEVICE:[MIKE.OVMS_AGENT]M316_BAD.CXX;1 line=3 column=17 message=expected expression");
    if (!ok) {
        (void)printf("M316 failed: CXX embedded location.\n%s\n",
                     result != NULL ? result : "<null>");
    }
    free(result);
    return ok;
}

static int test_posix_failure(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    m316_test_calls = 0U;
    status = 0UL;
    result = m289_build_source(state, "M316_BAD.F90", &status);
    ok = result != NULL && m316_test_calls == 1U &&
         status == 0x1035A00AUL &&
         m316_has(result, "Compile status: %X1035A00A (failure)") &&
         m316_has(result, "Link: not run because compile failed.") &&
         m316_has(result, "language=FORTRAN phase=compile severity=E facility=F90 id=ERROR") &&
         m316_has(result, "line=3 column=0 message=Syntax error");
    if (!ok) {
        (void)printf("M316 failed: POSIX status classification.\n%s\n",
                     result != NULL ? result : "<null>");
    }
    free(result);
    return ok;
}

static int test_dcl_failure(agent_state *state)
{
    char *result;
    unsigned long status;
    int ok;

    m316_test_calls = 0U;
    status = 0UL;
    result = m289_build_source(state, "M316_BAD.COM", &status);
    ok = result != NULL && m316_test_calls == 1U &&
         status == 0x00038090UL &&
         m316_has(result, "Procedure status: %X00038090 (failure)") &&
         m316_has(result, "language=DCL phase=procedure severity=W facility=DCL id=IVVERB") &&
         m316_has(result, "message=unrecognized command verb - check validity and spelling");
    if (!ok) {
        (void)printf("M316 failed: DCL normalization.\n%s\n",
                     result != NULL ? result : "<null>");
    }
    free(result);
    return ok;
}

int main(void)
{
    agent_state state;

    (void)memset(&state, 0, sizeof(state));
    if (!m289_command_allowed("CC M316_BAD.C", M289_BUILD_COMPILE, "C") ||
        !m289_command_allowed("LINK M316_BAD.OBJ", M289_BUILD_LINK, "C") ||
        m289_command_allowed("CC M316_BAD.C|DELETE *.*;*",
                             M289_BUILD_COMPILE, "C") ||
        !test_c_failure(&state) ||
        !test_cxx_embedded(&state) ||
        !test_posix_failure(&state) ||
        !test_dcl_failure(&state)) {
        return 2;
    }

    (void)puts("M316 compiler diagnostic normalization regression passed.");
    return 0;
}
