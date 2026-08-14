#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

extern int fgetname(FILE *, char *);

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

static int m252_read_exact(
    const char *path,
    const char *expected_spec,
    const char *expected_text)
{
    FILE *file;
    char spec[EDIT_TXN_PATH_SIZE];
    char line[128];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgetname(file, spec) == 0 ||
        strcmp(spec, expected_spec) != 0 ||
        fgets(line, sizeof(line), file) == NULL ||
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

static int m252_rollback_test(void)
{
    const char *base = "M252_DELETE_ROLL.DAT";
    const char *versioned = "M252_DELETE_ROLL.DAT;1";
    const char *text = "M252 ROLLBACK ORIGINAL\n";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    edit_txn transaction;
    int result;

    if (!m252_seed(base, text, exact_spec)) {
        m252_cleanup(base);
        return 0;
    }

    edit_txn_init(&transaction);
    result =
        edit_txn_add_delete(&transaction, versioned) &&
        edit_txn_write(&transaction) &&
        m252_absent(versioned) &&
        edit_txn_rollback(&transaction) &&
        m252_read_exact(versioned, exact_spec, text);

    edit_txn_dispose(&transaction);
    m252_cleanup(base);
    return result;
}

static int m252_commit_test(void)
{
    const char *base = "M252_DELETE_COMMIT.DAT";
    const char *versioned = "M252_DELETE_COMMIT.DAT;1";
    char exact_spec[EDIT_TXN_PATH_SIZE];
    edit_txn transaction;
    int result;

    if (!m252_seed(
            base,
            "M252 COMMIT ORIGINAL\n",
            exact_spec)) {
        m252_cleanup(base);
        return 0;
    }

    edit_txn_init(&transaction);
    result =
        edit_txn_add_delete(&transaction, versioned) &&
        edit_txn_write(&transaction) &&
        m252_absent(versioned) &&
        edit_txn_commit(&transaction) &&
        m252_absent(versioned) &&
        m252_absent(base);

    if (!result && !transaction.committed) {
        (void)edit_txn_rollback(&transaction);
    }

    edit_txn_dispose(&transaction);
    m252_cleanup(base);
    return result;
}

static int m252_reject_test(void)
{
    edit_txn transaction;
    int result;

    edit_txn_init(&transaction);
    result =
        !edit_txn_add_delete(&transaction, "M252_DELETE.DAT") &&
        !edit_txn_add_delete(&transaction, "M252_DELETE.DAT;0") &&
        !edit_txn_add_delete(&transaction, "M252_DELETE.DAT;*") &&
        !edit_txn_add_delete(&transaction, "../M252_DELETE.DAT;1") &&
        !edit_txn_add_delete(&transaction, "SYS$DISK:M252_DELETE.DAT;1");
    edit_txn_dispose(&transaction);

    return result;
}

int main(void)
{
    if (!m252_reject_test()) {
        (void)puts("M252 invalid delete path rejection failed.");
        return EXIT_FAILURE;
    }

    if (!m252_rollback_test()) {
        (void)puts("M252 exact-version delete rollback failed.");
        return EXIT_FAILURE;
    }

    if (!m252_commit_test()) {
        (void)puts("M252 exact-version delete commit failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 exact-version delete tests passed.");
    return EXIT_SUCCESS;
}
