#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_LANGUAGE.H"

static int expect_lang(const char *prompt,
                       const char *root,
                       const char *wanted,
                       const char *name)
{
    const char *actual;

    actual = llm_lang_detect(prompt, root);
    if (actual == NULL || strcmp(actual, wanted) != 0) {
        (void)printf("M288 failed: %s\n", name);
        return 0;
    }
    return 1;
}

static int expect_none(const char *prompt,
                       const char *root,
                       const char *name)
{
    if (llm_lang_detect(prompt, root) != NULL) {
        (void)printf("M288 failed: %s\n", name);
        return 0;
    }
    return 1;
}

int main(void)
{
    char *merged;

    if (!expect_lang("Modify WC.COB", "SYS$DISK:[TMP]",
                     "COBOL", "COB extension detection") ||
        !expect_lang("Create a Pascal program", "SYS$DISK:[TMP]",
                     "PASCAL", "Pascal prompt detection") ||
        !expect_lang("Fix the program", "SYS$DISK:[MIKE.VMS-FORTRAN-SANDBOX]",
                     "FORTRAN", "sandbox-name detection") ||
        !expect_lang("Compile HELLO.CPP", "SYS$DISK:[TMP]",
                     "CXX", "C++ extension detection") ||
        !expect_lang("Do a basic review of MAIN.C", "SYS$DISK:[TMP]",
                     "C", "extension outranks ordinary adjective") ||
        !expect_lang("Create the program", "SYS$DISK:[MIKE.VMS-BASIC-SANDBOX]",
                     "BASIC", "BASIC sandbox detection") ||
        !expect_none("Do a basic review", "SYS$DISK:[TMP]",
                     "ordinary basic adjective") ||
        !expect_none("Review this JavaScript", "SYS$DISK:[TMP]",
                     "JavaScript is not Java")) {
        return EXIT_FAILURE;
    }

    merged = llm_lang_merge("BASE", "Create a COBOL program");
    if (merged == NULL) {
        (void)puts("M288 failed: unable to merge COBOL pack");
        return EXIT_FAILURE;
    }

    if (strstr(merged, "terminal reference format") == NULL ||
        strcmp(llm_lang_last(), "COBOL") != 0 ||
        llm_lang_last_bytes() == 0U) {
        free(merged);
        (void)puts("M288 failed: COBOL pack was not injected");
        return EXIT_FAILURE;
    }
    free(merged);

    (void)puts("M288 language knowledge regression passed.");
    return EXIT_SUCCESS;
}
