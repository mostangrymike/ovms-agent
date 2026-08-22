#include "LLM_INTERNAL.H"

static int llm_m148_write_input(
    const char *path,
    const char *prefix,
    size_t repeat_count,
    const char *suffix)
{
    FILE *file;
    size_t index;
    int result;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    result = fputs(prefix, file) != EOF;

    for (index = 0U;
         result && index < repeat_count;
         ++index) {
        result = fputc('X', file) != EOF;
    }

    if (result && suffix != NULL) {
        result = fputs(suffix, file) != EOF;
    }

    if (fclose(file) != 0) {
        result = 0;
    }

    return result;
}

static int llm_m148_read_file(
    const char *path,
    char *input,
    size_t input_size)
{
    FILE *file;
    int status;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    status = command_read_stream(
        file,
        input,
        input_size);

    (void)fclose(file);
    return status;
}

int llm_m148_complete_accept(void)
{
    char input[32];

    strcpy(input, "AGENT/STATUS\n");

    return command_line_complete(
        input,
        sizeof(input),
        0);
}

int llm_m148_exact_safe(void)
{
    char input[8];

    strcpy(input, "1234567");

    return
        !command_line_complete(
            input,
            sizeof(input),
            0) &&
        command_line_complete(
            input,
            sizeof(input),
            1);
}

int llm_m148_prefix_blocked(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_PREFIX.TMP";
    char input[32];
    int status;

    if (!llm_m148_write_input(
            path,
            "AGENT/APPROVE ",
            80U,
            "\n")) {
        return 0;
    }

    status = llm_m148_read_file(
        path,
        input,
        sizeof(input));

    while (remove(path) == 0) {
    }

    return status < 0 && input[0] == '\0';
}

int llm_m148_remainder_drained(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_DRAIN.TMP";
    FILE *file;
    char input[32];
    int first_status;
    int second_status;
    int result;

    if (!llm_m148_write_input(
            path,
            "AGENT/APPROVE ",
            80U,
            "\nAGENT/STATUS\n")) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    first_status = command_read_stream(
        file,
        input,
        sizeof(input));

    second_status = command_read_stream(
        file,
        input,
        sizeof(input));

    result =
        first_status < 0 &&
        second_status > 0 &&
        strcmp(input, "AGENT/STATUS\n") == 0;

    (void)fclose(file);

    while (remove(path) == 0) {
    }

    return result;
}

int llm_m148_approval_unchanged(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_APPROVE.TMP";
    char input[32];
    int approved_before;
    unsigned long hash_before;
    int invalid_before;
    int status;

    llm_plan_approval_clear();

    approved_before = llm_plan_approved;
    hash_before = llm_approved_hash;
    invalid_before = llm_approval_invalidated;

    if (!llm_m148_write_input(
            path,
            "AGENT/APPROVE ",
            80U,
            "\n")) {
        return 0;
    }

    status = llm_m148_read_file(
        path,
        input,
        sizeof(input));

    while (remove(path) == 0) {
    }

    return
        status < 0 &&
        llm_plan_approved == approved_before &&
        llm_approved_hash == hash_before &&
        llm_approval_invalidated == invalid_before;
}

int llm_m148_execute_no_action(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_EXEC.TMP";
    char input[32];
    int workflow_before;
    int rollback_before;
    int status;

    workflow_before = llm_last_workflow;
    rollback_before = llm_last_rollback;

    if (!llm_m148_write_input(
            path,
            "AGENT/EXECUTE ",
            80U,
            "\n")) {
        return 0;
    }

    status = llm_m148_read_file(
        path,
        input,
        sizeof(input));

    while (remove(path) == 0) {
    }

    return
        status < 0 &&
        llm_last_workflow == workflow_before &&
        llm_last_rollback == rollback_before;
}

int llm_m148_eof_accept(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_EOF.TMP";
    char input[32];
    int status;

    if (!llm_m148_write_input(
            path,
            "AGENT/STATUS",
            0U,
            NULL)) {
        return 0;
    }

    status = llm_m148_read_file(
        path,
        input,
        sizeof(input));

    while (remove(path) == 0) {
    }

    return
        status > 0 &&
        strcmp(input, "AGENT/STATUS") == 0;
}

int llm_m148_empty_eof(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M148_EMPTY.TMP";
    char input[32];
    int status;

    if (!llm_m148_write_input(
            path,
            "",
            0U,
            NULL)) {
        return 0;
    }

    status = llm_m148_read_file(
        path,
        input,
        sizeof(input));

    while (remove(path) == 0) {
    }

    return status == 0 && input[0] == '\0';
}
