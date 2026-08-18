#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_plan.h"

#define GAP009_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define GAP009_CHECK_FILE "OVMS_AGENT_PLAN.TXT.CHK"
#define GAP009_EXISTING "EXISTING.TXT"
#define GAP009_NEW "NEW.TXT"

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

static void gap009_remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static char *gap009_read_plan(void)
{
    FILE *file;
    long length;
    char *text;
    size_t got;

    file = fopen(GAP009_PLAN_FILE, "r");
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
    if (fclose(file) != 0 || got != (size_t)length) {
        free(text);
        return NULL;
    }

    text[got] = '\0';
    return text;
}

static int gap009_has(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static int gap009_write_existing(void)
{
    FILE *file;
    int ok;

    file = fopen(GAP009_EXISTING, "w");
    if (file == NULL) {
        return 0;
    }

    ok = fputs("old\n", file) != EOF;
    if (fclose(file) != 0) {
        ok = 0;
    }

    return ok;
}

static void gap009_cleanup(void)
{
    gap009_remove_all(GAP009_PLAN_FILE);
    gap009_remove_all(GAP009_CHECK_FILE);
    gap009_remove_all("OVMS_AGENT_PLAN_SAVE_OLD.TXT");
    gap009_remove_all("OVMS_AGENT_PLAN_SAVE_OLD.CHK");
    gap009_remove_all(GAP009_EXISTING);
    gap009_remove_all(GAP009_NEW);
}

int main(void)
{
    static const char scoped_plan[] =
        "Goal\nTest saved file scope.\n"
        "Files to inspect\nEXISTING.TXT\n"
        "Files to modify\nEXISTING.TXT\n"
        "Files to create\nNEW.TXT\n"
        "Ordered edits\nReplace one line and create one file.\n"
        "Validation\nCheck metadata.\n"
        "Risks\nNone.\n"
        "Authority required\nworkspace\n"
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=EXISTING.TXT\n"
        "BEGIN_OLD_TEXT\nold\nEND_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\nnew\nEND_NEW_TEXT\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=NEW.TXT\n"
        "BEGIN_NEW_TEXT\ncreated\nEND_NEW_TEXT\n"
        "END_OPERATION\n";
    static const char zero_plan[] =
        "Goal\nInspect only.\n"
        "Files to inspect\nNone.\n"
        "Files to modify\nNone.\n"
        "Files to create\nNone.\n"
        "Ordered edits\nNone.\n"
        "Validation\nNone.\n"
        "Risks\nNone.\n"
        "Authority required\nread-only\n";
    static const char unsafe_plan[] =
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=../BAD.TXT\n"
        "BEGIN_NEW_TEXT\nx\nEND_NEW_TEXT\n"
        "END_OPERATION\n";
    char *text;
    FILE *file;
    int ok;

    gap009_cleanup();
    ok = 1;

    if (!gap009_write_existing()) {
        (void)puts("M266: unable to create existing-file fixture.");
        gap009_cleanup();
        return 2;
    }

    if (!openai_plan_save("GAP-009 scoped plan", scoped_plan)) {
        (void)puts("M266: unable to save scoped GAP-009 plan.");
        ok = 0;
    }

    text = gap009_read_plan();
    if (text == NULL ||
        !gap009_has(text, "file_count=2\n") ||
        !gap009_has(text, "file=EXISTING.TXT|size=") ||
        !gap009_has(text, "file=NEW.TXT|missing=1\n")) {
        (void)puts("M266: scoped plan metadata is incorrect.");
        ok = 0;
    }
    free(text);

    if (ok && !openai_plan_is_current(0)) {
        (void)puts("M266: scoped plan is not current immediately after save.");
        ok = 0;
    }

    file = fopen(GAP009_NEW, "w");
    if (file == NULL || fputs("appeared after planning\n", file) == EOF) {
        if (file != NULL) {
            (void)fclose(file);
        }
        (void)puts("M266: unable to create planned-new fixture.");
        ok = 0;
    } else if (fclose(file) != 0) {
        (void)puts("M266: unable to close planned-new fixture.");
        ok = 0;
    }

    if (ok && openai_plan_is_current(0)) {
        (void)puts("M266: plan did not become stale when planned-new file appeared.");
        ok = 0;
    }

    gap009_cleanup();

    if (ok && !openai_plan_save("GAP-009 zero scope", zero_plan)) {
        (void)puts("M266: unable to save zero-file plan.");
        ok = 0;
    }

    text = gap009_read_plan();
    if (ok && (text == NULL || !gap009_has(text, "file_count=0\n"))) {
        (void)puts("M266: zero-file plan did not persist file_count=0.");
        ok = 0;
    }
    free(text);

    if (ok && !openai_plan_is_current(0)) {
        (void)puts("M266: zero-file plan is not current after save.");
        ok = 0;
    }

    gap009_cleanup();

    if (ok && openai_plan_save("GAP-009 unsafe scope", unsafe_plan)) {
        (void)puts("M266: unsafe plan scope was accepted.");
        ok = 0;
    }

    gap009_cleanup();

    if (!ok) {
        return 2;
    }

    (void)puts("M266 GAP-009 saved plan file-scope regression passed.");
    return 1;
}
