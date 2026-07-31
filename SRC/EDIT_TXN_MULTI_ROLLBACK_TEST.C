#include <stdio.h>
#include <stdlib.h>

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

int main(void)
{
    edit_txn transaction;

    if (!write_seed("TXN_MULTI_A.TXT", "original A\n") ||
        !write_seed("TXN_MULTI_B.TXT", "original B\n")) {
        (void)puts("Unable to create transaction fixtures.");
        return EXIT_FAILURE;
    }

    edit_txn_init(&transaction);

    if (!edit_txn_add(
            &transaction,
            "TXN_MULTI_A.TXT",
            "changed A\n") ||
        !edit_txn_add(
            &transaction,
            "TXN_MULTI_B.TXT",
            "changed B\n") ||
        !edit_txn_write(&transaction)) {
        edit_txn_dispose(&transaction);
        (void)puts("Unable to write multi-file transaction.");
        return EXIT_FAILURE;
    }

    if (!edit_txn_rollback(&transaction)) {
        edit_txn_dispose(&transaction);
        (void)puts("Multi-file rollback failed.");
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);
    (void)puts("Multi-file rollback test passed.");
    return EXIT_SUCCESS;
}
