#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define DIAGNOSTIC_FILE "[.BUILD]M206_DIAGNOSTIC.TXT"
#define PLAN_VALID "M206_PLAN_VALID.TMP"
#define PLAN_BAD "M206_PLAN_BAD.TMP"
#define PLAN_UNSAFE "M206_PLAN_UNSAFE.TMP"
#define PLAN_DUP "M206_PLAN_DUP.TMP"
#define TARGET_A "M206_TARGET_A.TMP"
#define TARGET_B "M206_TARGET_B.TMP"

/*
 * Link-only stubs for LLM_PLAN.OBJ interactive input helpers.
 * The deterministic M206 regression path never calls these.
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

static void remove_all_versions(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_text(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static char *read_text(const char *path)
{
    FILE *file;
    char *text;
    long length;
    size_t got;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    got = fread(text, 1U, (size_t)length, file);
    text[got] = '\0';
    (void)fclose(file);
    return text;
}

static int prepare_targets(void)
{
    remove_all_versions(TARGET_A);
    remove_all_versions(TARGET_B);

    return write_text(TARGET_A, "ALPHA\n") &&
           write_text(TARGET_B, "BETA\n");
}

static void cleanup(void)
{
    remove_all_versions(PLAN_VALID);
    remove_all_versions(PLAN_BAD);
    remove_all_versions(PLAN_UNSAFE);
    remove_all_versions(PLAN_DUP);
    remove_all_versions(TARGET_A);
    remove_all_versions(TARGET_B);
}

int main(void)
{
    char *diagnostic;
    char *prompt;
    int ok;

    ok = 0;
    diagnostic = NULL;
    prompt = NULL;

    diagnostic = read_text(DIAGNOSTIC_FILE);
    if (diagnostic == NULL ||
        strstr(diagnostic, "MISSING_SYMBOL") == NULL) {
        (void)puts("M206 failed: compiler diagnostic was not captured.");
        goto done;
    }

    prompt = llm_build_repair_prompt(
        "Repair the controlled M206 compiler failure",
        diagnostic);

    if (prompt == NULL ||
        strstr(prompt,
               "Repair the controlled M206 compiler failure") == NULL ||
        strstr(prompt, "MISSING_SYMBOL") == NULL) {
        (void)puts("M206 failed: repair request lost goal or diagnostic.");
        goto done;
    }

    if (!prepare_targets()) {
        (void)puts("M206 failed: unable to create target fixtures.");
        goto done;
    }

    if (!write_text(
            PLAN_VALID,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=M206_TARGET_A.TMP\n"
            "old_text=ALPHA\n"
            "new_text=ALPHA_FIXED\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=M206_TARGET_B.TMP\n"
            "old_text=BETA\n"
            "new_text=BETA_FIXED\n"
            "END_OPERATION\n") ||
        !llm_exec_validate_ops_file(PLAN_VALID) ||
        !llm_exec_dry_validate(
            PLAN_VALID,
            TARGET_A,
            "ALPHA\n")) {
        (void)puts("M206 failed: valid multi-file plan was rejected.");
        goto done;
    }

    if (!write_text(
            PLAN_BAD,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=M206_TARGET_A.TMP\n"
            "old_text=ALPHA\n"
            "new_text=ALPHA_FIXED\n"
            "END_OPERATION\n") ||
        llm_exec_validate_ops_file(PLAN_BAD)) {
        (void)puts("M206 failed: malformed plan was accepted.");
        goto done;
    }

    if (!write_text(
            PLAN_UNSAFE,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=../M206_ESCAPE.TMP\n"
            "old_text=ALPHA\n"
            "new_text=ESCAPE\n"
            "END_OPERATION\n") ||
        llm_exec_dry_validate(
            PLAN_UNSAFE,
            TARGET_A,
            "ALPHA\n")) {
        (void)puts("M206 failed: unsafe plan reached executable staging.");
        goto done;
    }

    if (!write_text(
            PLAN_DUP,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=M206_TARGET_A.TMP\n"
            "old_text=ALPHA\n"
            "new_text=ALPHA_FIXED\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=M206_TARGET_A.TMP\n"
            "old_text=GAMMA\n"
            "new_text=ALPHA_AGAIN\n"
            "END_OPERATION\n") ||
        llm_exec_dry_validate(
            PLAN_DUP,
            TARGET_A,
            "ALPHA\n")) {
        (void)puts("M206 failed: invalid chained edit precondition was accepted.");
        goto done;
    }

    (void)puts("Failed-build diagnostics and repair-plan test passed.");
    ok = 1;

done:
    free(prompt);
    free(diagnostic);
    cleanup();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
