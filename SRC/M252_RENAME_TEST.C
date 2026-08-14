#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

static void m252_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_seed(
    const char *path,
    const char *text,
    char *exact_spec)
{
    FILE *file;

    m252_cleanup(path);

    file = fopen(path, "w", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
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

static int m252_read(
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

static int m252_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m252_reject_test(void)
{
    edit_txn transaction;
    int result;

    edit_txn_init(&transaction);
    result =
        !edit_txn_add_rename(
            &transaction,
            "M252_RENAME_SRC.DAT",
            "M252_RENAME_DST.DAT;1") &&
        !edit_txn_add_rename(
            &transaction,
            "M252_RENAME_SRC.DAT;1",
            "M252_RENAME_DST.DAT") &&
        !edit_txn_add_rename(
            &transaction,
            "M252_RENAME_SRC.DAT;0",
            "M252_RENAME_DST.DAT;1") &&
        !edit_txn_add_rename(
            &transaction,
            "M252_RENAME_SRC.DAT;1",
            "M252_RENAME_DST.DAT;*") &&
        !edit_txn_add_rename(
            &transaction,
            "M252_RENAME_SRC.DAT;1",
            "[.OTHER]M252_RENAME_DST.DAT;1") &&
        !edit_txn_add_rename(
            &transaction,
            "SYS$DISK:M252_RENAME_SRC.DAT;1",
            "M252_RENAME_DST.DAT;1");
    edit_txn_dispose(&transaction);

    return result;
}

static int m252_existing_dst(void)
{
    const char *src_base = "M252_RENAME_EXIST_SRC.DAT";
    const char *dst_base = "M252_RENAME_EXIST_DST.DAT";
    char src_spec[EDIT_TXN_PATH_SIZE];
    char dst_spec[EDIT_TXN_PATH_SIZE];
    edit_txn transaction;
    int result;

    if (!m252_seed(src_base, "SOURCE\n", src_spec) ||
        !m252_seed(dst_base, "DESTINATION\n", dst_spec)) {
        m252_cleanup(src_base);
        m252_cleanup(dst_base);
        return 0;
    }

    edit_txn_init(&transaction);
    result = !edit_txn_add_rename(
        &transaction,
        "M252_RENAME_EXIST_SRC.DAT;1",
        "M252_RENAME_EXIST_DST.DAT;1");
    edit_txn_dispose(&transaction);

    m252_cleanup(src_base);
    m252_cleanup(dst_base);
    return result;
}

static int m252_rollback_test(void)
{
    const char *src_base = "M252_RENAME_ROLL_SRC.DAT";
    const char *src = "M252_RENAME_ROLL_SRC.DAT;1";
    const char *dst = "M252_RENAME_ROLL_DST.DAT;1";
    const char *text = "M252 RENAME ROLLBACK\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    edit_txn transaction;
    int result;

    m252_cleanup("M252_RENAME_ROLL_DST.DAT");

    if (!m252_seed(src_base, text, original_spec)) {
        m252_cleanup(src_base);
        return 0;
    }

    edit_txn_init(&transaction);
    result =
        edit_txn_add_rename(&transaction, src, dst) &&
        edit_txn_write(&transaction) &&
        m252_absent(src) &&
        m252_read(dst, text) &&
        edit_txn_rollback(&transaction) &&
        m252_absent(dst) &&
        m252_read(src, text);

    edit_txn_dispose(&transaction);
    m252_cleanup(src_base);
    m252_cleanup("M252_RENAME_ROLL_DST.DAT");
    return result;
}

static int m252_commit_test(void)
{
    const char *src_base = "M252_RENAME_COMMIT_SRC.DAT";
    const char *src = "M252_RENAME_COMMIT_SRC.DAT;1";
    const char *dst_base = "M252_RENAME_COMMIT_DST.DAT";
    const char *dst = "M252_RENAME_COMMIT_DST.DAT;1";
    const char *text = "M252 RENAME COMMIT\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    edit_txn transaction;
    int result;

    m252_cleanup(dst_base);

    if (!m252_seed(src_base, text, original_spec)) {
        m252_cleanup(src_base);
        return 0;
    }

    edit_txn_init(&transaction);
    result =
        edit_txn_add_rename(&transaction, src, dst) &&
        edit_txn_write(&transaction) &&
        m252_absent(src) &&
        m252_read(dst, text) &&
        edit_txn_commit(&transaction) &&
        m252_absent(src) &&
        m252_read(dst, text);

    edit_txn_dispose(&transaction);
    m252_cleanup(src_base);
    m252_cleanup(dst_base);
    return result;
}

int main(void)
{
    if (!m252_reject_test()) {
        (void)puts("M252 rename path rejection failed.");
        return EXIT_FAILURE;
    }

    if (!m252_existing_dst()) {
        (void)puts("M252 existing rename destination was accepted.");
        return EXIT_FAILURE;
    }

    if (!m252_rollback_test()) {
        (void)puts("M252 exact-version rename rollback failed.");
        return EXIT_FAILURE;
    }

    if (!m252_commit_test()) {
        (void)puts("M252 exact-version rename commit failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 exact-version rename tests passed.");
    return EXIT_SUCCESS;
}
