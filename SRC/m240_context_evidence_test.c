#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"
#include "LLM_CONTEXT.H"

int command_line_complete(const char *a,size_t b,int c)
{(void)a;(void)b;(void)c;return 0;}
int command_read_stream(FILE *a,char *b,size_t c)
{(void)a;(void)b;(void)c;return 0;}

int main(void)
{
    char output[8192];
    char result[512];
    unsigned int index;

    while (remove("M240_TX.TMP") == 0) {
    }

    llm_test_tx_path("M240_TX.TMP");

    for (index = 0U; index < 36U; ++index) {
        (void)snprintf(
            result, sizeof(result),
            "TOOL RESULT tool: read_file status: ok code: 1 "
            "effect: read truncated: no output: evidence-%u",
            index);
        llm_tx_model_result("read_file", "ok", result);
    }

    llm_tx_model_result(
        "run_build", "failure",
        "TOOL RESULT tool: run_build status: failure code: 2 "
        "effect: execute truncated: no output: M240-LAST-BUILD-FAILURE");

    if (!llm_context_evidence_text(
            "--------", output, sizeof(output)) ||
        strstr(output, "LATEST NORMALIZED RESULT") == NULL ||
        strstr(output, "RECENT NORMALIZED RESULTS") == NULL ||
        strstr(output, "run_build") == NULL ||
        strstr(output, "M240-LAST-BUILD-FAILURE") == NULL) {
        (void)puts("M240 failed: prioritized evidence replay.");
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Priority result replay: available") == NULL) {
        (void)puts("M240 failed: parity status.");
        return EXIT_FAILURE;
    }

    while (remove("M240_TX.TMP") == 0) {
    }

    (void)puts("Prioritized continuation evidence regression passed.");
    return EXIT_SUCCESS;
}
