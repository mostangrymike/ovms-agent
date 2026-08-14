#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

int openai_m252_delete_exec(const char *path, int commit_change);

int command_line_complete(
    const char *input,
    size_t input_size,
    int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 0;
}

int command_read_stream(
    FILE *stream,
    char *input,
    size_t input_size)
{
    (void)stream;
    (void)input;
    (void)input_size;
    return 0;
}

static void m252_exec_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_exec_seed(
    const char *path,
    const char *text,
    char *exact_spec)
{
    FILE *file;

    m252_exec_cleanup(path);

    file = fopen(path, "w", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF || fclose(file) != 0) {
        return 0;
    }

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgetname(file, exact_spec) == 0) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int m252_exec_plan(
    const char *plan_path,
    const char *target_path)
{
    FILE *file;
    int result;

    m252_exec_cleanup(plan_path);
    file = fopen(plan_path, "w");
    if (file == NULL) {
        return 0;
    }

    result =
        fprintf(file,
                "operation_count=1\n"
                "BEGIN_OPERATION\n"
                "type=delete_file\n"
                "path=%s\n"
                "END_OPERATION\n",
                target_path) > 0;

    if (fclose(file) != 0) {
        result = 0;
    }

    return result;
}

static int m252_exec_read(
    const char *path,
    const char *expected_spec,
    const char *expected_text)
{
    FILE *file;
    char actual_spec[EDIT_TXN_PATH_SIZE];
    char line[128];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgetname(file, actual_spec) == 0 ||
        strcmp(actual_spec, expected_spec) != 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, expected_text) != 0 ||
        fgets(line, sizeof(line), file) != NULL) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int m252_exec_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m252_exec_rollback(void)
{
    const char *base = "M252_EXEC_ROLL.DAT";
    const char *versioned = "M252_EXEC_ROLL.DAT;1";
    const char *plan = "M252_EXEC_ROLL.PLAN";
    const char *text = "M252 EXEC ROLLBACK ORIGINAL\n";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    int result;

    if (!m252_exec_seed(base, text, exact_spec) ||
        !m252_exec_plan(plan, versioned)) {
        m252_exec_cleanup(plan);
        m252_exec_cleanup(base);
        return 0;
    }

    result =
        openai_m252_delete_exec(plan, 0) &&
        m252_exec_read(versioned, exact_spec, text);

    m252_exec_cleanup(plan);
    m252_exec_cleanup(base);
    return result;
}

static int m252_exec_commit(void)
{
    const char *base = "M252_EXEC_COMMIT.DAT";
    const char *versioned = "M252_EXEC_COMMIT.DAT;1";
    const char *plan = "M252_EXEC_COMMIT.PLAN";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    int result;

    if (!m252_exec_seed(
            base,
            "M252 EXEC COMMIT ORIGINAL\n",
            exact_spec) ||
        !m252_exec_plan(plan, versioned)) {
        m252_exec_cleanup(plan);
        m252_exec_cleanup(base);
        return 0;
    }

    result =
        openai_m252_delete_exec(plan, 1) &&
        m252_exec_absent(versioned) &&
        m252_exec_absent(base);

    m252_exec_cleanup(plan);
    m252_exec_cleanup(base);
    return result;
}

int main(void)
{
    if (!m252_exec_rollback()) {
        (void)puts("M252 saved-plan delete rollback failed.");
        return EXIT_FAILURE;
    }

    if (!m252_exec_commit()) {
        (void)puts("M252 saved-plan delete commit failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 saved-plan delete execution tests passed.");
    return EXIT_SUCCESS;
}
