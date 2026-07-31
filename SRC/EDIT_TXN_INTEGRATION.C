/*
 * M61 integration pattern
 */

#include "edit_txn.h"

static int apply_two_file_change(
    const char *first_path,
    const char *first_text,
    const char *second_path,
    const char *second_text)
{
    edit_txn transaction;
    int verified;

    edit_txn_init(&transaction);

    if (!edit_txn_add(
            &transaction,
            first_path,
            first_text) ||
        !edit_txn_add(
            &transaction,
            second_path,
            second_text) ||
        !edit_txn_write(
            &transaction)) {
        edit_txn_dispose(&transaction);
        return 0;
    }

    /*
     * Replace this placeholder with:
     *   reindex
     *   object invalidation
     *   controlled build
     */
    verified = 1;

    if (!verified) {
        (void)edit_txn_rollback(
            &transaction
        );
        edit_txn_dispose(&transaction);
        return 0;
    }

    (void)edit_txn_commit(
        &transaction
    );
    edit_txn_dispose(&transaction);
    return 1;
}
