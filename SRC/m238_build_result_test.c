#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"

int command_line_complete(const char *a,size_t b,int c)
{(void)a;(void)b;(void)c;return 0;}
int command_read_stream(FILE *a,char *b,size_t c)
{(void)a;(void)b;(void)c;return 0;}

int main(void)
{
    char *result;
    char parity[8192];

    result = openai_build_result(
        "Building OVMS Agent Version 2...\n"
        "All regression tests passed.\n"
        "Build completed successfully.\n",
        1
    );

    if (result == NULL ||
        strstr(result, "tool: run_build") == NULL ||
        strstr(result, "status: success") == NULL ||
        strstr(result, "code: 1") == NULL ||
        strstr(result, "effect: execute") == NULL ||
        strstr(result, "Build completed successfully.") == NULL) {
        free(result);
        (void)puts("M238 failed: successful build result.");
        return EXIT_FAILURE;
    }
    free(result);

    result = openai_build_result(
        "%CC-E-UNDECLARED, identifier BROKEN is undefined.\n"
        "%LINK-E-UNDEFSYMS, undefined symbols\n",
        2
    );

    if (result == NULL ||
        strstr(result, "status: failure") == NULL ||
        strstr(result, "code: 2") == NULL ||
        strstr(result, "%CC-E-UNDECLARED") == NULL ||
        strstr(result, "BROKEN is undefined") == NULL ||
        strstr(result, "%LINK-E-UNDEFSYMS") == NULL) {
        free(result);
        (void)puts("M238 failed: failed build evidence.");
        return EXIT_FAILURE;
    }
    free(result);

    if (!openai_result_last_text(parity, sizeof(parity)) ||
        strstr(parity, "tool: run_build") == NULL ||
        strstr(parity, "status: failure") == NULL ||
        strstr(parity, "%CC-E-UNDECLARED") == NULL) {
        (void)puts("M238 failed: build evidence replay.");
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(parity, sizeof(parity)) ||
        strstr(parity, "Normalized build result: available") == NULL ||
        strstr(parity, "Build evidence replay:  available") == NULL) {
        (void)puts("M238 failed: parity.");
        return EXIT_FAILURE;
    }

    (void)puts("Normalized build-result integration regression passed.");
    return EXIT_SUCCESS;
}
