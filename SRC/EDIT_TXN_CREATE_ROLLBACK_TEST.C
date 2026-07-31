#include <stdio.h>
#include <stdlib.h>
#include "edit_txn.h"

int main(void)
{
    edit_txn transaction;
    FILE *file;

    (void)remove("TXN_CREATE_ROLLBACK.OPT");
    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_CREATE_ROLLBACK.OPT",
                      "[.BUILD]TEMP.OBJ\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_rollback(&transaction)) {
        edit_txn_dispose(&transaction);
        (void)puts("Create rollback test failed.");
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);
    file = fopen("TXN_CREATE_ROLLBACK.OPT", "r");
    if (file != NULL) {
        (void)fclose(file);
        (void)puts("Created file still exists after rollback.");
        return EXIT_FAILURE;
    }

    (void)puts("Create rollback test passed.");
    return EXIT_SUCCESS;
}
