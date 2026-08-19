#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M212_REPAIR_HISTORY.LOG"

/* Link-only stubs required by OPENAI_PLAN.OBJ. */
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

static int ordered(const char *text,
                   const char *first,
                   const char *second)
{
    const char *a;
    const char *b;

    a = strstr(text, first);
    b = strstr(text, second);
    return a != NULL && b != NULL && a < b;
}

int main(void)
{
    char history[8192];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    openai_log_repair_attempt(1U, 0x10101010UL, 1,
                              OPENAI_ROLLBACK_NONE, "committed");
    openai_log_repair_attempt(1U, 0x20202020UL, 2,
                              OPENAI_ROLLBACK_SUCCEEDED, "rolled_back");
    openai_log_repair_attempt(1U, 0x30303030UL, 1,
                              OPENAI_ROLLBACK_NONE, "committed");
    openai_log_repair_attempt(1U, 0x40404040UL, 2,
                              OPENAI_ROLLBACK_SUCCEEDED, "rolled_back");
    openai_log_repair_attempt(1U, 0x50505050UL, 1,
                              OPENAI_ROLLBACK_NONE, "committed");
    openai_log_repair_attempt(1U, 0x60606060UL, 2,
                              OPENAI_ROLLBACK_SUCCEEDED, "rolled_back");
    openai_log_repair_attempt(2U, 0x61616161UL, 1,
                              OPENAI_ROLLBACK_NONE, "committed");
    openai_log_repair_attempt(1U, 0x70707070UL, 1,
                              OPENAI_ROLLBACK_NONE, "committed");

    if (!openai_repair_history_text(history, sizeof(history))) {
        (void)puts("M212 failed: repair history unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (strstr(history, "Recent runs: 5 (maximum 5)") == NULL ||
        strstr(history, "Run 1 (newest)") == NULL ||
        strstr(history, "10101010") != NULL ||
        strstr(history, "20202020") != NULL ||
        strstr(history, "30303030") == NULL ||
        strstr(history, "60606060") == NULL ||
        strstr(history, "61616161") == NULL ||
        strstr(history, "70707070") == NULL) {
        (void)puts("M212 failed: repair history bounds are incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!ordered(history, "70707070", "60606060") ||
        !ordered(history, "60606060", "50505050") ||
        !ordered(history, "50505050", "40404040") ||
        !ordered(history, "40404040", "30303030") ||
        !ordered(history, "60606060", "61616161")) {
        (void)puts("M212 failed: repair history ordering is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Bounded repair history test passed.");
    return EXIT_SUCCESS;
}
