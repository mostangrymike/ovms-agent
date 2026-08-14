#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

int openai_m252_rename_exec(const char *path, int commit_change);

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

static void m252_re_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_re_seed(
    const char *path,
    const char *text,
    char *exact_spec)
{
    FILE *file;

    m252_re_cleanup(path);

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

static int m252_re_plan(
    const char *plan_path,
    const char *source_path,
    const char *target_path)
{
    FILE *file;
    int result;

    m252_re_cleanup(plan_path);
    file = fopen(plan_path, "w");
    if (file == NULL) {
        return 0;
    }

    result =
        fprintf(file,
                "operation_count=1\n"
                "BEGIN_OPERATION\n"
                "type=rename_file\n"
                "path=%s\n"
                "target_path=%s\n"
                "END_OPERATION\n",
                source_path,
                target_path) > 0;

    if (fclose(file) != 0) {
        result = 0;
    }

    return result;
}

static int m252_re_read(
    const char *path,
    const char *expected_text)
{
    FILE *file;
    char line[128];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, expected_text) != 0 ||
        fgets(line, sizeof(line), file) != NULL) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int m252_re_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m252_re_rollback(void)
{
    const char *src_base = "M252_REX_ROLL_SRC.DAT";
    const char *src_ver = "M252_REX_ROLL_SRC.DAT;1";
    const char *dst_base = "M252_REX_ROLL_DST.DAT";
    const char *dst_ver = "M252_REX_ROLL_DST.DAT;1";
    const char *plan = "M252_REX_ROLL.PLAN";
    const char *text = "M252 RENAME EXEC ROLLBACK\n";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    int result;

    m252_re_cleanup(dst_base);

    if (!m252_re_seed(src_base, text, exact_spec) ||
        !m252_re_plan(plan, src_ver, dst_ver)) {
        m252_re_cleanup(plan);
        m252_re_cleanup(src_base);
        m252_re_cleanup(dst_base);
        return 0;
    }

    result =
        openai_m252_rename_exec(plan, 0) &&
        m252_re_read(src_ver, text) &&
        m252_re_absent(dst_ver);

    m252_re_cleanup(plan);
    m252_re_cleanup(src_base);
    m252_re_cleanup(dst_base);
    return result;
}

static int m252_re_commit(void)
{
    const char *src_base = "M252_REX_COMMIT_SRC.DAT";
    const char *src_ver = "M252_REX_COMMIT_SRC.DAT;1";
    const char *dst_base = "M252_REX_COMMIT_DST.DAT";
    const char *dst_ver = "M252_REX_COMMIT_DST.DAT;1";
    const char *plan = "M252_REX_COMMIT.PLAN";
    const char *text = "M252 RENAME EXEC COMMIT\n";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    int result;

    m252_re_cleanup(dst_base);

    if (!m252_re_seed(src_base, text, exact_spec) ||
        !m252_re_plan(plan, src_ver, dst_ver)) {
        m252_re_cleanup(plan);
        m252_re_cleanup(src_base);
        m252_re_cleanup(dst_base);
        return 0;
    }

    result =
        openai_m252_rename_exec(plan, 1) &&
        m252_re_absent(src_ver) &&
        m252_re_read(dst_ver, text);

    m252_re_cleanup(plan);
    m252_re_cleanup(src_base);
    m252_re_cleanup(dst_base);
    return result;
}

int main(void)
{
    if (!m252_re_rollback()) {
        (void)puts("M252 saved-plan rename rollback failed.");
        return EXIT_FAILURE;
    }

    if (!m252_re_commit()) {
        (void)puts("M252 saved-plan rename commit failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 saved-plan rename execution tests passed.");
    return EXIT_SUCCESS;
}
