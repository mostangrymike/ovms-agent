#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "command_input_m251.inc"
#include "COMMAND_INPUT_M299.INC"

#define TEST_OK "M251_MULTI_OK.TMP"
#define TEST_OVER "M251_MULTI_OVER.TMP"
#define TEST_EOF "M251_MULTI_EOF.TMP"

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int test_preserve_and_recover(void)
{
    FILE *file;
    char body[OVMS_AGENT_INPUT_SIZE];
    char next[64];
    int status;
    int ok;

    remove_all(TEST_OK);

    file = fopen(TEST_OK, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "first line\n"
            "second line with spaces\n"
            ".ENDING is content\n"
            ".END\n"
            "STATUS\n",
            file) == EOF ||
        fclose(file) != 0) {
        remove_all(TEST_OK);
        return 0;
    }

    file = fopen(TEST_OK, "r");
    if (file == NULL) {
        remove_all(TEST_OK);
        return 0;
    }

    status = command_read_multiline(
        file,
        body,
        sizeof(body));

    next[0] = '\0';
    (void)fgets(next, sizeof(next), file);

    ok =
        status == 1 &&
        strcmp(
            body,
            "first line\n"
            "second line with spaces\n"
            ".ENDING is content\n") == 0 &&
        strcmp(next, "STATUS\n") == 0;

    (void)fclose(file);
    remove_all(TEST_OK);
    return ok;
}

static int test_overflow_recovery(void)
{
    FILE *file;
    char body[128];
    char next[64];
    size_t index;
    int status;
    int ok;

    remove_all(TEST_OVER);

    file = fopen(TEST_OVER, "w");
    if (file == NULL) {
        return 0;
    }

    for (index = 0U; index < 200U; ++index) {
        if (fputc('X', file) == EOF) {
            (void)fclose(file);
            remove_all(TEST_OVER);
            return 0;
        }
    }

    if (fputs("\n.END\nSTATUS\n", file) == EOF ||
        fclose(file) != 0) {
        remove_all(TEST_OVER);
        return 0;
    }

    file = fopen(TEST_OVER, "r");
    if (file == NULL) {
        remove_all(TEST_OVER);
        return 0;
    }

    status = command_read_multiline(
        file,
        body,
        sizeof(body));

    next[0] = '\0';
    (void)fgets(next, sizeof(next), file);

    ok =
        status == -1 &&
        body[0] == '\0' &&
        strcmp(next, "STATUS\n") == 0;

    (void)fclose(file);
    remove_all(TEST_OVER);
    return ok;
}

static int test_unterminated(void)
{
    FILE *file;
    char body[256];
    int status;
    int ok;

    remove_all(TEST_EOF);

    file = fopen(TEST_EOF, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("first\nsecond\n", file) == EOF ||
        fclose(file) != 0) {
        remove_all(TEST_EOF);
        return 0;
    }

    file = fopen(TEST_EOF, "r");
    if (file == NULL) {
        remove_all(TEST_EOF);
        return 0;
    }

    status = command_read_multiline(
        file,
        body,
        sizeof(body));

    ok = status == -2 && body[0] == '\0';

    (void)fclose(file);
    remove_all(TEST_EOF);
    return ok;
}

static int test_empty(void)
{
    FILE *file;
    char body[64];
    int status;
    int ok;

    remove_all(TEST_OK);

    file = fopen(TEST_OK, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(".END\n", file) == EOF ||
        fclose(file) != 0) {
        remove_all(TEST_OK);
        return 0;
    }

    file = fopen(TEST_OK, "r");
    if (file == NULL) {
        remove_all(TEST_OK);
        return 0;
    }

    status = command_read_multiline(
        file,
        body,
        sizeof(body));

    ok = status == 1 && body[0] == '\0';

    (void)fclose(file);
    remove_all(TEST_OK);
    return ok;
}

static int test_trigger(const char *text,
                        int expected,
                        const char *expected_prefix)
{
    char input[OVMS_AGENT_INPUT_SIZE];
    char prefix[OVMS_AGENT_INPUT_SIZE];
    int status;

    if (text == NULL || strlen(text) >= sizeof(input)) {
        return 0;
    }

    (void)strcpy(input, text);
    status = main_multiline_prefix(input, prefix, sizeof(prefix));

    if (status != expected) {
        return 0;
    }

    if (!expected) {
        return prefix[0] == '\0';
    }

    return expected_prefix != NULL &&
           strcmp(prefix, expected_prefix) == 0;
}

static int test_native_triggers(void)
{
    return
        test_trigger("AGENT/WRITE\n", 1, "AGENT/WRITE") &&
        test_trigger("ASK", 1, "ASK") &&
        test_trigger("chat", 1, "chat") &&
        test_trigger(
            "AGENT/IMAGE SYS$DISK:[IMAGES]PANEL.PNG",
            1,
            "AGENT/IMAGE SYS$DISK:[IMAGES]PANEL.PNG") &&
        test_trigger(
            "AGENT/IMAGE \"SYS$DISK:[MY IMAGES]PANEL.PNG\"",
            1,
            "AGENT/IMAGE \"SYS$DISK:[MY IMAGES]PANEL.PNG\"") &&
        test_trigger(
            "AGENT/SESSION/EXEC 00000001",
            1,
            "AGENT/SESSION/EXEC 00000001") &&
        test_trigger(
            "AGENT/EXEC/FORK experiment ::",
            1,
            "AGENT/EXEC/FORK experiment ::") &&
        test_trigger(
            "AGENT/EXEC/RESUME/CREATE",
            1,
            "AGENT/EXEC/RESUME/CREATE") &&
        test_trigger("AGENT/MULTILINE", 1, "AGENT") &&
        test_trigger(
            "agent/create/multiline",
            1,
            "AGENT/CREATE") &&
        test_trigger("AGENT/WRITE one line", 0, NULL) &&
        test_trigger(
            "AGENT/IMAGE image.png describe this",
            0,
            NULL) &&
        test_trigger("AGENT/IMAGE", 0, NULL) &&
        test_trigger(
            "AGENT/EXEC/FORK experiment :: one line",
            0,
            NULL) &&
        test_trigger("AGENT/EXEC/FORK ::", 0, NULL) &&
        test_trigger("DCL", 0, NULL) &&
        test_trigger("AGENT/PATCH", 0, NULL);
}

int main(void)
{
    if (!test_preserve_and_recover()) {
        (void)puts(
            "M251.5b failed: multiline text was not preserved."
        );
        return EXIT_FAILURE;
    }

    if (!test_overflow_recovery()) {
        (void)puts(
            "M251.5b failed: overflow did not drain through .END."
        );
        return EXIT_FAILURE;
    }

    if (!test_unterminated()) {
        (void)puts(
            "M251.5b failed: unterminated multiline input was accepted."
        );
        return EXIT_FAILURE;
    }

    if (!test_empty()) {
        (void)puts(
            "M251.5b failed: empty terminated body handling changed."
        );
        return EXIT_FAILURE;
    }

    if (!test_native_triggers()) {
        (void)puts(
            "M299 failed: native multiline trigger selection changed."
        );
        return EXIT_FAILURE;
    }

    (void)puts("M251.5b true multiline input regression passed.");
    (void)puts("M299 native multiline trigger regression passed.");
    return EXIT_SUCCESS;
}