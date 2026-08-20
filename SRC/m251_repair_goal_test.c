#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_plan_sensitive.inc"

#define LLM_GOAL_GUARD_TEXT_ONLY 1
#include "LLM_GOAL_GUARD.INC"

/*
 * Link-only stubs for unrelated OPENAI_RETRY.C entry points.
 * The regression executes only the prompt-builder functions below.
 */
int openai_last_workflow = 0;
int openai_last_rollback = 0;
int openai_last_build_known = 0;
int openai_last_build_status = 0;
unsigned long openai_approved_hash = 0UL;
int openai_plan_approved = 0;

const char *openai_workflow_name()
{
    return "test";
}

void openai_log_event()
{
}

char *openai_duplicate_text()
{
    return NULL;
}

char *execute_run_build_tool()
{
    return NULL;
}

void openai_agent_mode()
{
}

int openai_plan_save()
{
    return 0;
}

void openai_agent_plan()
{
}

int openai_plan_is_current()
{
    return 0;
}

char *openai_read_text_file()
{
    return NULL;
}

int openai_plan_approve_file()
{
    return 0;
}

void openai_plan_approve()
{
}

void openai_plan_execute()
{
}

void openai_log_repair_attempt()
{
}

void openai_plan_approval_clear()
{
}

void project_git_diff()
{
}

int openai_plan_clear_files()
{
    return 1;
}

int openai_plan_is_noop_text(const char *text);

char *openai_build_goal_prompt(const char *goal,
                               const char *build_output);
char *openai_build_repair_prompt(const char *goal,
                                 const char *build_output);

int main(void)
{
    char *prompt;
    char reason[256];
    static const char list_block[] =
        "IF WS-CMD = 'LIST'\n"
        "    PERFORM CMD-LIST\n"
        "END-IF\n";

    prompt = openai_build_goal_prompt(
        "Repair the reported runtime behavior",
        "Building test project...\nBuild completed successfully.\n"
    );

    if (prompt == NULL ||
        strstr(prompt, "baseline project build currently succeeds") == NULL ||
        strstr(prompt, "Repair the reported runtime behavior") == NULL ||
        strstr(prompt, "Build completed successfully.") == NULL ||
        strstr(prompt, "do not assume this is a compiler or linker failure") ==
            NULL) {
        free(prompt);
        (void)puts("M251.8 failed: passing-baseline repair prompt is invalid.");
        return EXIT_FAILURE;
    }

    free(prompt);

    prompt = openai_build_repair_prompt(
        "Repair the compiler failure",
        "%CC-E-UNDECLARED, test identifier is undefined.\n"
    );

    if (prompt == NULL ||
        strstr(prompt, "failed build") == NULL ||
        strstr(prompt, "Repair the compiler failure") == NULL ||
        strstr(prompt, "%CC-E-UNDECLARED") == NULL) {
        free(prompt);
        (void)puts("M251.8 failed: failed-build repair prompt regressed.");
        return EXIT_FAILURE;
    }

    free(prompt);

    if (plan_has_sensitive_text("OPENAI_API_KEY") ||
        plan_has_sensitive_text(
            "The code reads OPENAI_API_KEY from the environment.") ||
        plan_has_sensitive_text("Authorization: Bearer") ||
        plan_has_sensitive_text(
            "const char *h = \"Authorization: Bearer \";") ||
        plan_has_sensitive_text("config/server.key") ||
        plan_has_sensitive_text("certificate.pem") ||
        !plan_has_sensitive_text(
            "OPENAI_API_KEY=sk-test-secret-value") ||
        !plan_has_sensitive_text(
            "OPENAI_API_KEY: \"sk-test-secret-value\"") ||
        !plan_has_sensitive_text(
            "Authorization: Bearer sk-test-secret-value")) {
        (void)puts(
            "M251.9 failed: credential-value screening is invalid."
        );
        return EXIT_FAILURE;
    }

    if (!openai_plan_is_noop_text("operation_count=0\n") ||
        !openai_plan_is_noop_text(
            "header\noperation_count=0\r\nfooter\n") ||
        openai_plan_is_noop_text("operation_count=10\n") ||
        openai_plan_is_noop_text("prefix operation_count=0\n")) {
        (void)puts("M251.9 failed: no-op repair detection is invalid.");
        return EXIT_FAILURE;
    }

    if (goal_guard_text_ok(
            "Complete the bounded LIST implementation.",
            list_block,
            "CONTINUE\n",
            reason,
            sizeof(reason)) ||
        strstr(reason, "LIST") == NULL) {
        (void)puts(
            "M251.10 failed: goal-regressing LIST removal was not rejected."
        );
        return EXIT_FAILURE;
    }

    if (!goal_guard_text_ok(
            "Complete the bounded LIST implementation.",
            list_block,
            list_block,
            reason,
            sizeof(reason))) {
        (void)puts(
            "M251.10 failed: goal-preserving LIST repair was rejected."
        );
        return EXIT_FAILURE;
    }

    if (!goal_guard_text_ok(
            "Remove LIST support from this command parser.",
            list_block,
            "CONTINUE\n",
            reason,
            sizeof(reason))) {
        (void)puts(
            "M251.10 failed: explicitly requested LIST removal was rejected."
        );
        return EXIT_FAILURE;
    }

    if (goal_guard_text_ok(
            "Do not remove LIST support from this command parser.",
            list_block,
            "CONTINUE\n",
            reason,
            sizeof(reason))) {
        (void)puts(
            "M251.10 failed: negated LIST removal was not protected."
        );
        return EXIT_FAILURE;
    }

    if (!goal_guard_text_ok(
            "User goal:\nPreserve LIST behavior.\n\n"
            "Current failed build result:\n%COBOL-E-BUILD failed\n",
            "COBOL diagnostic cleanup text\n",
            "diagnostic cleanup text\n",
            reason,
            sizeof(reason))) {
        (void)puts(
            "M251.10 failed: build diagnostics leaked into protected terms."
        );
        return EXIT_FAILURE;
    }

    (void)puts("M251.8 user-directed repair regression passed.");
    (void)puts("M251.9 credential screening regression passed.");
    (void)puts("M251.9 no-op repair regression passed.");
    (void)puts("M251.10 repair goal-preservation regression passed.");
    return EXIT_SUCCESS;
}
