#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_M312_REPORT.INC"

#define M312_FIXTURE "M312_REPORTING_FIXTURE.TXT"

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_fixture(void)
{
    FILE *file;
    unsigned int index;

    remove_all(M312_FIXTURE);
    file = fopen(M312_FIXTURE, "w");
    if (file == NULL) {
        return 0;
    }

    for (index = 0U; index < 500U; ++index) {
        if (fprintf(
                file,
                "%03u SHORT IDENTIFICATION DIVISION filler filler filler\n",
                index) < 0) {
            (void)fclose(file);
            remove_all(M312_FIXTURE);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int run_search(
    const char *pattern,
    char *output,
    size_t output_size,
    unsigned long *total,
    unsigned long *returned,
    int *truncated)
{
    FILE *file;
    int ok;

    file = fopen(M312_FIXTURE, "r");
    if (file == NULL) {
        return 0;
    }

    ok = llm_m312_search_stream(
        file,
        pattern,
        output,
        output_size,
        total,
        returned,
        truncated
    );
    (void)fclose(file);
    return ok;
}

int main(void)
{
    char facts[256];
    char short_output[256];
    char long_output[256];
    char absent_output[256];
    unsigned long short_total;
    unsigned long short_returned;
    unsigned long long_total;
    unsigned long long_returned;
    unsigned long absent_total;
    unsigned long absent_returned;
    int short_truncated;
    int long_truncated;
    int absent_truncated;

    if (!llm_m312_final_facts(facts, sizeof(facts), 32U, 32U) ||
        strstr(facts, "limit_reached=yes") == NULL ||
        strstr(facts, "turns=32") == NULL ||
        strstr(facts, "tool_calls=32") == NULL ||
        strstr(facts, "authoritative") == NULL) {
        (void)puts("M312 final-synthesis host facts regression failed.");
        return EXIT_FAILURE;
    }

    if (!write_fixture()) {
        (void)puts("M312 unable to create search fixture.");
        return EXIT_FAILURE;
    }

    if (!run_search(
            "SHORT",
            short_output,
            sizeof(short_output),
            &short_total,
            &short_returned,
            &short_truncated) ||
        !run_search(
            "IDENTIFICATION DIVISION",
            long_output,
            sizeof(long_output),
            &long_total,
            &long_returned,
            &long_truncated) ||
        !run_search(
            "M312_ABSENT",
            absent_output,
            sizeof(absent_output),
            &absent_total,
            &absent_returned,
            &absent_truncated)) {
        remove_all(M312_FIXTURE);
        (void)puts("M312 search scan regression failed to execute.");
        return EXIT_FAILURE;
    }

    remove_all(M312_FIXTURE);

    if (short_total != 500UL || long_total != 500UL ||
        !short_truncated || !long_truncated ||
        short_returned == 0UL || long_returned == 0UL ||
        short_returned >= short_total || long_returned >= long_total) {
        (void)puts("M312 truncated search totals regression failed.");
        return EXIT_FAILURE;
    }

    if (absent_total != 0UL || absent_returned != 0UL ||
        absent_truncated || absent_output[0] != '\0') {
        (void)puts("M312 absent search regression failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M312 final synthesis host facts regression passed.");
    (void)puts("M312 search total/returned truncation regression passed.");
    return EXIT_SUCCESS;
}
