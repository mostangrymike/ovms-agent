#include <stdio.h>
#include <stdlib.h>

#include "edit_txn.h"

int main(void)
{
    edit_txn transaction;
    const char *first;
    const char *second;

    first =
        "[.BUILD]ONE.OBJ\n"
        "[.BUILD]TWO.OBJ\n";

    second =
        "$ WRITE SYS$OUTPUT \"transaction test\"\n"
        "$ EXIT 1\n";

    edit_txn_init(&transaction);

    if (!edit_txn_add(
            &transaction,
            "TXN_TEST.OPT",
            first) ||
        !edit_txn_add(
            &transaction,
            "TXN_TEST.COM",
            second)) {
        (void)puts(
            "Unable to stage transaction."
        );
        edit_txn_dispose(&transaction);
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction)) {
        (void)puts(
            "Transaction write failed."
        );
        edit_txn_dispose(&transaction);
        return EXIT_FAILURE;
    }

    if (!edit_txn_commit(&transaction)) {
        (void)puts(
            "Transaction commit failed."
        );
        edit_txn_dispose(&transaction);
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);
    (void)puts(
        "Edit transaction test passed."
    );
    return EXIT_SUCCESS;
}
