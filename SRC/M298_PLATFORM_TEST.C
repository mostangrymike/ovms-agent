#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_KNOWLEDGE.H"
#include "LLM_LANGUAGE.H"

static int expect_mask(const char *prompt,
                       unsigned int wanted,
                       const char *name)
{
    unsigned int actual;

    actual = llm_knowledge_detect(prompt);
    if (actual != wanted) {
        (void)printf("M298 failed: %s (got %u wanted %u)\n",
                     name, actual, wanted);
        return 0;
    }
    return 1;
}

int main(void)
{
    char *merged;

    if (!expect_mask("Use GITAUTH fetch origin main and verify FETCH_HEAD",
                     LLM_KNOWLEDGE_GIT,
                     "Git detection") ||
        !expect_mask("Inspect the RMS variable length record format",
                     LLM_KNOWLEDGE_RMS,
                     "RMS detection") ||
        !expect_mask("Fix this DCL command procedure and check $STATUS",
                     LLM_KNOWLEDGE_DCL,
                     "DCL detection") ||
        !expect_mask("Use Git to inspect an RMS record format problem",
                     LLM_KNOWLEDGE_GIT | LLM_KNOWLEDGE_RMS,
                     "combined Git RMS detection") ||
        !expect_mask("Review MAIN.C and simplify the parser",
                     0U,
                     "unrelated source edit") ||
        !expect_mask("Add a branch statement to the COBOL program",
                     0U,
                     "ordinary branch false positive")) {
        return EXIT_FAILURE;
    }

    merged = llm_knowledge_merge("BASE",
                                 "Use GITAUTH fetch origin main");
    if (merged == NULL) {
        (void)puts("M298 failed: unable to merge Git pack");
        return EXIT_FAILURE;
    }
    if (strstr(merged, "FETCH_HEAD") == NULL ||
        strcmp(llm_knowledge_last(), "OPENVMS_GIT") != 0 ||
        llm_knowledge_last_bytes() == 0U) {
        free(merged);
        (void)puts("M298 failed: Git pack was not injected");
        return EXIT_FAILURE;
    }
    free(merged);

    merged = llm_knowledge_merge("BASE",
                                 "Review MAIN.C and simplify the parser");
    if (merged == NULL || strcmp(merged, "BASE") != 0 ||
        llm_knowledge_last()[0] != '\0' ||
        llm_knowledge_last_bytes() != 0U) {
        if (merged != NULL) free(merged);
        (void)puts("M298 failed: unrelated prompt changed instructions");
        return EXIT_FAILURE;
    }
    free(merged);

    merged = llm_lang_merge("BASE",
                            "Use GITAUTH while modifying WC.COB");
    if (merged == NULL ||
        strstr(merged, "terminal reference format") == NULL ||
        strstr(merged, "FETCH_HEAD") == NULL ||
        strcmp(llm_lang_last(), "COBOL") != 0 ||
        strcmp(llm_knowledge_last(), "OPENVMS_GIT") != 0) {
        if (merged != NULL) free(merged);
        (void)puts("M298 failed: language and platform packs did not compose");
        return EXIT_FAILURE;
    }
    free(merged);

    (void)puts("M298 platform knowledge regression passed.");
    return EXIT_SUCCESS;
}
