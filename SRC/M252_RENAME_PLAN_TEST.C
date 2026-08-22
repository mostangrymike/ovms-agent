#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"

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

static void m252_rp_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_rp_check(
    const char *path,
    const char *text,
    int expected)
{
    FILE *file;
    int actual;
    int result;

    m252_rp_cleanup(path);
    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    result = fputs(text, file) != EOF;
    if (fclose(file) != 0) {
        result = 0;
    }

    if (!result) {
        m252_rp_cleanup(path);
        return 0;
    }

    actual = llm_exec_validate_ops_file(path);
    m252_rp_cleanup(path);
    return actual == expected;
}

static int m252_rp_valid(void)
{
    return m252_rp_check(
        "M252_RP_VALID.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=rename_file\n"
        "path=M252_RENAME_SRC.DAT;1\n"
        "target_path=M252_RENAME_DST.DAT;1\n"
        "END_OPERATION\n",
        1);
}

static int m252_rp_no_target(void)
{
    return m252_rp_check(
        "M252_RP_NOTARGET.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=rename_file\n"
        "path=M252_RENAME_SRC.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_rp_target_first(void)
{
    return m252_rp_check(
        "M252_RP_ORDER.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=rename_file\n"
        "target_path=M252_RENAME_DST.DAT;1\n"
        "path=M252_RENAME_SRC.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_rp_dup_target(void)
{
    return m252_rp_check(
        "M252_RP_DUP.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=rename_file\n"
        "path=M252_RENAME_SRC.DAT;1\n"
        "target_path=M252_RENAME_DST.DAT;1\n"
        "target_path=M252_RENAME_DST2.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_rp_payload(void)
{
    return m252_rp_check(
        "M252_RP_PAYLOAD.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=rename_file\n"
        "path=M252_RENAME_SRC.DAT;1\n"
        "target_path=M252_RENAME_DST.DAT;1\n"
        "new_text=not allowed\n"
        "END_OPERATION\n",
        0);
}

int main(void)
{
    if (!m252_rp_valid()) {
        (void)puts("M252 valid rename_file plan was rejected.");
        return EXIT_FAILURE;
    }

    if (!m252_rp_no_target()) {
        (void)puts("M252 rename_file without target_path was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_rp_target_first()) {
        (void)puts("M252 rename_file accepted target_path before path.");
        return EXIT_FAILURE;
    }

    if (!m252_rp_dup_target()) {
        (void)puts("M252 rename_file accepted duplicate target_path.");
        return EXIT_FAILURE;
    }

    if (!m252_rp_payload()) {
        (void)puts("M252 rename_file accepted trailing payload.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 rename plan parser tests passed.");
    return EXIT_SUCCESS;
}
