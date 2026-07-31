#include "OPENAI_INTERNAL.H"

static int openai_m149_write_text(
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

static int openai_m149_read_once(
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

static void openai_m149_remove_all(
    const char *path)
{
    while (remove(path) == 0) {
    }
}

int openai_m149_single_line(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_SINGLE.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "AGENT/STATUS\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "AGENT/STATUS\n") == 0;
}

int openai_m149_two_line(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_TWO.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK Explain OpenVMS \\\n"
            "logical names\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(
            input,
            "ASK Explain OpenVMS logical names\n") == 0;
}

int openai_m149_three_line(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_THREE.TMP";
    char input[96];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK Explain \\\n"
            "logical names, \\\n"
            "DCL symbols\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(
            input,
            "ASK Explain logical names, DCL symbols\n") == 0;
}

int openai_m149_marker_removed(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_MARKER.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "CHAT first\\\n"
            "second\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strchr(input, '\\') == NULL &&
        strcmp(input, "CHAT first second\n") == 0;
}

int openai_m149_separator(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_SEPARATOR.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK alpha\\\n"
            "beta\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "ASK alpha beta\n") == 0;
}

int openai_m149_boundary_space(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_SPACE.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK alpha \\\n"
            " beta\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "ASK alpha  beta\n") == 0;
}

int openai_m149_odd_backslash(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_ODD.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK path\\\\\\\n"
            "next\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "ASK path\\\\ next\n") == 0;
}

int openai_m149_even_backslash(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_EVEN.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK path\\\\\n"
            "AGENT/STATUS\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "ASK path\\\\\n") == 0;
}

int openai_m149_pending_eof(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_PENDING.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK incomplete\\")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return status < 0 && input[0] == '\0';
}

int openai_m149_complete_eof(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_COMPLETE_EOF.TMP";
    char input[64];
    int status;

    if (!openai_m149_write_text(
            path,
            "AGENT/STATUS")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return
        status > 0 &&
        strcmp(input, "AGENT/STATUS") == 0;
}

int openai_m149_overflow(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_OVERFLOW.TMP";
    char input[16];
    int status;

    if (!openai_m149_write_text(
            path,
            "ASK 1234567\\\n"
            "890123456789\n")) {
        return 0;
    }

    status = openai_m149_read_once(
        path,
        input,
        sizeof(input));

    openai_m149_remove_all(path);

    return status < 0 && input[0] == '\0';
}

int openai_m149_drain_next(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_DRAIN.TMP";
    FILE *file;
    char input[16];
    int first_status;
    int second_status;
    int result;

    if (!openai_m149_write_text(
            path,
            "ASK 1234567\\\n"
            "890123456789\n"
            "STATUS\n")) {
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
        strcmp(input, "STATUS\n") == 0;

    (void)fclose(file);
    openai_m149_remove_all(path);

    return result;
}

int openai_m149_single_dispatch(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M149_ONCE.TMP";
    FILE *file;
    char input[64];
    int first_status;
    int second_status;
    int result;

    if (!openai_m149_write_text(
            path,
            "ASK first\\\n"
            "second\n"
            "STATUS\n")) {
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

    result =
        first_status > 0 &&
        strcmp(input, "ASK first second\n") == 0;

    second_status = command_read_stream(
        file,
        input,
        sizeof(input));

    result =
        result &&
        second_status > 0 &&
        strcmp(input, "STATUS\n") == 0;

    (void)fclose(file);
    openai_m149_remove_all(path);

    return result;
}
