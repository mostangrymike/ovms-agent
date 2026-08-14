#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

#define FAIL_RECORD_SIZE 32768U

static int write_seed(const char *path, const char *text)
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

static int read_equals(const char *path, const char *expected)
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
    char *too_long;
    int result;
    FILE *probe;

    remove_all("TXN_FAIL_FIRST.TXT");
    remove_all("TXN_FAIL_SECOND.COM");

    if (!write_seed("TXN_FAIL_FIRST.TXT", "original first\n")) {
        (void)puts("Unable to create rollback seed.");
        return EXIT_FAILURE;
    }

    too_long = (char *)malloc(FAIL_RECORD_SIZE + 2U);
    if (too_long == NULL) {
        remove_all("TXN_FAIL_FIRST.TXT");
        return EXIT_FAILURE;
    }

    memset(too_long, 'X', FAIL_RECORD_SIZE);
    too_long[FAIL_RECORD_SIZE] = '\n';
    too_long[FAIL_RECORD_SIZE + 1U] = '\0';

    edit_txn_init(&transaction);

    result =
        edit_txn_add(
            &transaction,
            "TXN_FAIL_FIRST.TXT",
            "changed first\n") &&
        edit_txn_add(
            &transaction,
            "TXN_FAIL_SECOND.COM",
            too_long) &&
        !edit_txn_write(&transaction) &&
        read_equals(
            "TXN_FAIL_FIRST.TXT",
            "original first\n");

    probe = fopen("TXN_FAIL_SECOND.COM", "r");
    if (probe != NULL) {
        (void)fclose(probe);
        result = 0;
    }

    edit_txn_dispose(&transaction);
    free(too_long);

    remove_all("TXN_FAIL_FIRST.TXT");
    remove_all("TXN_FAIL_SECOND.COM");

    if (!result) {
        (void)puts("Automatic multi-file rollback test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("Automatic multi-file rollback test passed.");
    return EXIT_SUCCESS;
}
