#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M210_REPAIR_HISTORY.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * The deterministic M210 observability test never uses interactive input.
 */
int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream;
    (void)input;
    (void)input_size;
    return 0;
}

static void cleanup(void)
{
    openai_test_set_log_path(NULL);
    while (remove(TEST_LOG) == 0) {
    }
}

int main(void)
{
    char record[1024];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    openai_log_repair_attempt(
        1U,
        0x1234ABCDUL,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );

    openai_log_repair_attempt(
        2U,
        0x89ABCDEFUL,
        1,
        OPENAI_ROLLBACK_NONE,
        "committed"
    );

    if (!openai_last_repair_record(
            record,
            sizeof(record))) {
        (void)puts("M210 failed: no persisted repair record.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (strstr(record, "attempt=2") == NULL ||
        strstr(record, "plan=89ABCDEF") == NULL ||
        strstr(record, "build=1") == NULL ||
        strstr(record, "rollback=0") == NULL ||
        strstr(record, "outcome=committed") == NULL) {
        (void)puts("M210 failed: latest repair record is incomplete.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Persistent repair observability test passed.");
    return EXIT_SUCCESS;
}
