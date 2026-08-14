#include <stdio.h>
#include <stdlib.h>

#include "openai_internal.h"

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

static void m252_mp_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_mp_check(
    const char *path,
    const char *text,
    int expected)
{
    FILE *file;
    int actual;
    int result;

    m252_mp_cleanup(path);
    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    result = fputs(text, file) != EOF;
    if (fclose(file) != 0) {
        result = 0;
    }

    if (!result) {
        m252_mp_cleanup(path);
        return 0;
    }

    actual = openai_exec_validate_ops_file(path);
    m252_mp_cleanup(path);
    return actual == expected;
}

static int m252_mp_valid(void)
{
    return m252_mp_check(
        "M252_MP_VALID.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=move_file\n"
        "path=M252_MOVE_SRC.DAT;1\n"
        "target_path=[.TEST]M252_MOVE_DST.DAT;1\n"
        "END_OPERATION\n",
        1);
}

static int m252_mp_no_target(void)
{
    return m252_mp_check(
        "M252_MP_NOTARGET.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=move_file\n"
        "path=M252_MOVE_SRC.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_mp_target_first(void)
{
    return m252_mp_check(
        "M252_MP_ORDER.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=move_file\n"
        "target_path=[.TEST]M252_MOVE_DST.DAT;1\n"
        "path=M252_MOVE_SRC.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_mp_dup_target(void)
{
    return m252_mp_check(
        "M252_MP_DUP.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=move_file\n"
        "path=M252_MOVE_SRC.DAT;1\n"
        "target_path=[.TEST]M252_MOVE_DST.DAT;1\n"
        "target_path=[.TEST]M252_MOVE_DST2.DAT;1\n"
        "END_OPERATION\n",
        0);
}

static int m252_mp_payload(void)
{
    return m252_mp_check(
        "M252_MP_PAYLOAD.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=move_file\n"
        "path=M252_MOVE_SRC.DAT;1\n"
        "target_path=[.TEST]M252_MOVE_DST.DAT;1\n"
        "new_text=not allowed\n"
        "END_OPERATION\n",
        0);
}

int main(void)
{
    if (!m252_mp_valid()) {
        (void)puts("M252 valid move_file plan was rejected.");
        return EXIT_FAILURE;
    }

    if (!m252_mp_no_target()) {
        (void)puts("M252 move_file without target_path was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_mp_target_first()) {
        (void)puts("M252 move_file accepted target_path before path.");
        return EXIT_FAILURE;
    }

    if (!m252_mp_dup_target()) {
        (void)puts("M252 move_file accepted duplicate target_path.");
        return EXIT_FAILURE;
    }

    if (!m252_mp_payload()) {
        (void)puts("M252 move_file accepted trailing payload.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 move plan parser tests passed.");
    return EXIT_SUCCESS;
}
