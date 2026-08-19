#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"

/* OPENAI_PLAN.OBJ references the interactive command-input helpers that are
 * normally supplied by MAIN.C.  This standalone parser regression does not
 * exercise interactive input and cannot link MAIN.OBJ because it defines its
 * own main(), so provide inert test-local definitions. */
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

static int m252_write_plan(const char *path, const char *text)
{
    FILE *file;
    int result;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    result = fputs(text, file) != EOF;
    if (fclose(file) != 0) {
        result = 0;
    }

    return result;
}

static void m252_remove_plan(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_check_plan(const char *path,
                           const char *text,
                           int expected)
{
    int actual;

    m252_remove_plan(path);

    if (!m252_write_plan(path, text)) {
        return 0;
    }

    actual = openai_exec_validate_ops_file(path);
    m252_remove_plan(path);

    return actual == expected;
}

static int m252_valid_delete(void)
{
    return m252_check_plan(
        "M252_PLAN_VALID.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=delete_file\n"
        "path=M252_DELETE_TARGET.DAT;1\n"
        "END_OPERATION\n",
        1);
}

static int m252_delete_old_reject(void)
{
    return m252_check_plan(
        "M252_PLAN_OLD.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=delete_file\n"
        "path=M252_DELETE_TARGET.DAT;1\n"
        "BEGIN_OLD_TEXT\n"
        "must not be accepted\n"
        "END_OLD_TEXT\n"
        "END_OPERATION\n",
        0);
}

static int m252_delete_new_reject(void)
{
    return m252_check_plan(
        "M252_PLAN_NEW.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=delete_file\n"
        "path=M252_DELETE_TARGET.DAT;1\n"
        "BEGIN_NEW_TEXT\n"
        "must not be accepted\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        0);
}

static int m252_delete_field_reject(void)
{
    return m252_check_plan(
        "M252_PLAN_FIELD.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=delete_file\n"
        "path=M252_DELETE_TARGET.DAT;1\n"
        "new_text=must not be accepted\n"
        "END_OPERATION\n",
        0);
}

static int m252_missing_path_reject(void)
{
    return m252_check_plan(
        "M252_PLAN_NOPATH.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=delete_file\n"
        "END_OPERATION\n",
        0);
}

int main(void)
{
    if (!m252_valid_delete()) {
        (void)puts("M252 valid delete_file plan was rejected.");
        return EXIT_FAILURE;
    }

    if (!m252_delete_old_reject()) {
        (void)puts("M252 delete_file OLD payload was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_delete_new_reject()) {
        (void)puts("M252 delete_file NEW payload was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_delete_field_reject()) {
        (void)puts("M252 delete_file legacy payload was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_missing_path_reject()) {
        (void)puts("M252 delete_file without path was accepted.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 delete plan parser tests passed.");
    return EXIT_SUCCESS;
}
