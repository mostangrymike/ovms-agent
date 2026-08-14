#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

#define M253_FAIL_RECORD_SIZE 32768U

static void m253_fail_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m253_fail_seed(
    const char *path,
    const char *text,
    char *exact_spec)
{
    FILE *file;

    m253_fail_cleanup(path);

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

static int m253_fail_read(
    const char *path,
    const char *expected,
    char *exact_spec)
{
    FILE *file;
    char line[128];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgetname(file, exact_spec) == 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, expected) != 0 ||
        fgets(line, sizeof(line), file) != NULL) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int m253_fail_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static char *m253_fail_text(void)
{
    char *text;

    text = (char *)malloc(M253_FAIL_RECORD_SIZE + 2U);
    if (text == NULL) {
        return NULL;
    }

    (void)memset(text, 'X', M253_FAIL_RECORD_SIZE);
    text[M253_FAIL_RECORD_SIZE] = '\n';
    text[M253_FAIL_RECORD_SIZE + 1U] = '\0';
    return text;
}

static int m253_delete_fail(void)
{
    const char *base = "M253_DFAIL.DAT";
    const char *source = "M253_DFAIL.DAT;1";
    const char *bad = "M253_DFAIL_BAD.OPT";
    const char *text = "M253 DELETE FAILURE ORIGINAL\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    char restored_spec[EDIT_TXN_PATH_SIZE];
    char *too_long;
    edit_txn txn;
    int result;

    m253_fail_cleanup(base);
    m253_fail_cleanup(bad);

    if (!m253_fail_seed(base, text, original_spec)) {
        return 0;
    }

    too_long = m253_fail_text();
    if (too_long == NULL) {
        m253_fail_cleanup(base);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_delete(&txn, source) &&
        edit_txn_add(&txn, bad, too_long) &&
        !edit_txn_write(&txn) &&
        m253_fail_read(source, text, restored_spec) &&
        strcmp(restored_spec, original_spec) == 0 &&
        m253_fail_absent(bad);

    edit_txn_dispose(&txn);
    free(too_long);
    m253_fail_cleanup(base);
    m253_fail_cleanup(bad);
    return result;
}

static int m253_rename_fail(void)
{
    const char *base = "M253_RFAIL_SRC.DAT";
    const char *source = "M253_RFAIL_SRC.DAT;1";
    const char *target = "M253_RFAIL_DST.DAT;1";
    const char *bad = "M253_RFAIL_BAD.OPT";
    const char *text = "M253 RENAME FAILURE ORIGINAL\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    char restored_spec[EDIT_TXN_PATH_SIZE];
    char *too_long;
    edit_txn txn;
    int result;

    m253_fail_cleanup(base);
    m253_fail_cleanup(target);
    m253_fail_cleanup(bad);

    if (!m253_fail_seed(base, text, original_spec)) {
        return 0;
    }

    too_long = m253_fail_text();
    if (too_long == NULL) {
        m253_fail_cleanup(base);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_rename(&txn, source, target) &&
        edit_txn_add(&txn, bad, too_long) &&
        !edit_txn_write(&txn) &&
        m253_fail_read(source, text, restored_spec) &&
        strcmp(restored_spec, original_spec) == 0 &&
        m253_fail_absent(target) &&
        m253_fail_absent(bad);

    edit_txn_dispose(&txn);
    free(too_long);
    m253_fail_cleanup(base);
    m253_fail_cleanup(target);
    m253_fail_cleanup(bad);
    return result;
}

static int m253_move_fail(void)
{
    const char *base = "M253_MFAIL_SRC.DAT";
    const char *source = "M253_MFAIL_SRC.DAT;1";
    const char *target = "[.TEST]M253_MFAIL_DST.DAT;1";
    const char *bad = "M253_MFAIL_BAD.OPT";
    const char *text = "M253 MOVE FAILURE ORIGINAL\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    char restored_spec[EDIT_TXN_PATH_SIZE];
    char *too_long;
    edit_txn txn;
    int result;

    m253_fail_cleanup(base);
    m253_fail_cleanup(target);
    m253_fail_cleanup(bad);

    if (!m253_fail_seed(base, text, original_spec)) {
        return 0;
    }

    too_long = m253_fail_text();
    if (too_long == NULL) {
        m253_fail_cleanup(base);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_move(&txn, source, target) &&
        edit_txn_add(&txn, bad, too_long) &&
        !edit_txn_write(&txn) &&
        m253_fail_read(source, text, restored_spec) &&
        strcmp(restored_spec, original_spec) == 0 &&
        m253_fail_absent(target) &&
        m253_fail_absent(bad);

    edit_txn_dispose(&txn);
    free(too_long);
    m253_fail_cleanup(base);
    m253_fail_cleanup(target);
    m253_fail_cleanup(bad);
    return result;
}

int main(void)
{
    if (!m253_delete_fail()) {
        (void)puts("M253 delete automatic failure rollback evidence failed.");
        return EXIT_FAILURE;
    }

    if (!m253_rename_fail()) {
        (void)puts("M253 rename automatic failure rollback evidence failed.");
        return EXIT_FAILURE;
    }

    if (!m253_move_fail()) {
        (void)puts("M253 move automatic failure rollback evidence failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M253 structural operation failure rollback evidence passed.");
    return EXIT_SUCCESS;
}
