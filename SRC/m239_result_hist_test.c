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
    char output[16384];

    while (remove("M239_TX.TMP") == 0) {
    }

    llm_test_tx_path("M239_TX.TMP");

    llm_tx_model_result(
        "read_file", "ok",
        "TOOL RESULT tool: read_file status: ok code: 1 "
        "effect: read truncated: no output: alpha"
    );
    llm_tx_loop_event("turn", "complete");
    llm_tx_model_result(
        "run_build", "failure",
        "TOOL RESULT tool: run_build status: failure code: 2 "
        "effect: execute truncated: no output: %CC-E-UNDECLARED"
    );

    if (!llm_session_results_text(
            "--------", output, sizeof(output)) ||
        strstr(output, "read_file") == NULL ||
        strstr(output, "run_build") == NULL ||
        strstr(output, "%CC-E-UNDECLARED") == NULL ||
        strstr(output, "Results: 2") == NULL) {
        (void)puts("M239 failed: result history.");
        return EXIT_FAILURE;
    }

    if (!llm_session_result_last(
            "--------", output, sizeof(output)) ||
        strstr(output, "run_build") == NULL ||
        strstr(output, "%CC-E-UNDECLARED") == NULL) {
        (void)puts("M239 failed: last result.");
        return EXIT_FAILURE;
    }

    if (!llm_session_clear_results("--------")) {
        (void)puts("M239 failed: result clear.");
        return EXIT_FAILURE;
    }

    if (!llm_session_results_text(
            "--------", output, sizeof(output)) ||
        strstr(output, "Results: 0") == NULL) {
        (void)puts("M239 failed: clear verification.");
        return EXIT_FAILURE;
    }

    if (!llm_session_hist_text(
            "--------", output, sizeof(output)) ||
        strstr(output, "kind=loop") == NULL) {
        (void)puts("M239 failed: clear preserved transcript.");
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Session result history: available") == NULL ||
        strstr(output, "Evidence preloading:    available") == NULL) {
        (void)puts("M239 failed: parity.");
        return EXIT_FAILURE;
    }

    while (remove("M239_TX.TMP") == 0) {
    }

    (void)puts("First-class session evidence regression passed.");
    return EXIT_SUCCESS;
}
