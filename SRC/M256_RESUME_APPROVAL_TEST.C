#include <stdio.h>
#include <stdlib.h>

#include "openai_internal.h"

#define TEST_DATA "M256_SESSIONS.DAT"
#define TEST_CUR  "M256_SESSION.CUR"

extern int openai_plan_approved;
extern unsigned long openai_approved_hash;
extern int openai_approval_invalidated;

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static void cleanup(void)
{
    openai_test_session_paths(NULL, NULL);
    openai_plan_session_reset();
    remove_all(TEST_DATA);
    remove_all(TEST_CUR);
}

int main(void)
{
    char id[9];

    cleanup();
    openai_test_session_paths(TEST_DATA, TEST_CUR);

    if (!openai_session_new("m256 resume approval", id)) {
        (void)puts("M256 failed: unable to create session fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* Simulate approval that existed before the user resumed the session. */
    openai_plan_approved = 1;
    openai_approved_hash = 0x12345678UL;
    openai_approval_invalidated = 1;

    if (!openai_session_resume(id)) {
        (void)puts("M256 failed: session resume unexpectedly failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_plan_approved != 0 ||
        openai_approved_hash != 0UL ||
        openai_approval_invalidated != 0) {
        (void)puts("M256 failed: resumed session retained stale plan approval.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M256 session resume reapproval evidence passed.");
    return EXIT_SUCCESS;
}
