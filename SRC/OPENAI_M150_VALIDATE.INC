#include "llm_internal.h"

static int openai_m150_write_plan(
    const char *path,
    const char *text)
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

static int openai_m150_check_plan(
    const char *path,
    const char *text,
    int expected)
{
    int result;

    if (!openai_m150_write_plan(path, text)) {
        return 0;
    }

    result =
        openai_exec_validate_ops_file(path) == expected;

    while (remove(path) == 0) {
    }

    return result;
}

int openai_m150_create_valid(void)
{
    return openai_m150_check_plan(
        "OVMS_AGENT_TEST_M150_VALID.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M150_CREATED.TMP\n"
        "BEGIN_NEW_TEXT\n"
        "first line\n"
        "second line\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        1);
}

int openai_m150_create_old_reject(void)
{
    return openai_m150_check_plan(
        "OVMS_AGENT_TEST_M150_OLD.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M150_CREATED.TMP\n"
        "BEGIN_OLD_TEXT\n"
        "must not be accepted\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "new text\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        0);
}

int openai_m150_create_missing_new(void)
{
    return openai_m150_check_plan(
        "OVMS_AGENT_TEST_M150_MISSING.TMP",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M150_CREATED.TMP\n"
        "END_OPERATION\n",
        0);
}
