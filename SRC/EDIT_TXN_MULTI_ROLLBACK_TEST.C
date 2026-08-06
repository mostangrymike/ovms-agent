#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

static int write_seed(
    const char *path,
    const char *text)
{
    FILE *file;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int read_equals(
    const char *path,
    const char *expected)
{
    FILE *file;
    char buffer[128];
    size_t used;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    used = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    buffer[used] = '\0';

    if (ferror(file) || fclose(file) != 0) {
        return 0;
    }

    return strcmp(buffer, expected) == 0;
}

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
        /* Remove all visible OpenVMS versions. */
    }
}

int main(void)
{
    edit_txn transaction;
    int result;

    remove_all("TXN_MULTI_A.TXT");
    remove_all("TXN_MULTI_B.TXT");
    remove_all("TXN_MULTI_NEW.TXT");

    if (!write_seed("TXN_MULTI_A.TXT", "original A\n") ||
        !write_seed("TXN_MULTI_B.TXT", "original B\n")) {
        (void)puts("Unable to create transaction fixtures.");
        return EXIT_FAILURE;
    }

    edit_txn_init(&transaction);

    result =
        edit_txn_add(
            &transaction,
            "TXN_MULTI_A.TXT",
            "changed A\n") &&
        edit_txn_add(
            &transaction,
            "TXN_MULTI_B.TXT",
            "changed B\n") &&
        edit_txn_add(
            &transaction,
            "TXN_MULTI_NEW.TXT",
            "created text\n") &&
        !edit_txn_add(
            &transaction,
            "txn_multi_a.txt",
            "duplicate\n") &&
        edit_txn_write(&transaction) &&
        read_equals("TXN_MULTI_A.TXT", "changed A\n") &&
        read_equals("TXN_MULTI_B.TXT", "changed B\n") &&
        read_equals("TXN_MULTI_NEW.TXT", "created text\n") &&
        edit_txn_rollback(&transaction) &&
        read_equals("TXN_MULTI_A.TXT", "original A\n") &&
        read_equals("TXN_MULTI_B.TXT", "original B\n") &&
        fopen("TXN_MULTI_NEW.TXT", "r") == NULL;

    edit_txn_dispose(&transaction);
    remove_all("TXN_MULTI_A.TXT");
    remove_all("TXN_MULTI_B.TXT");
    remove_all("TXN_MULTI_NEW.TXT");

    if (!result) {
        (void)puts("Multi-file rollback test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("Multi-file rollback test passed.");
    return EXIT_SUCCESS;
}
