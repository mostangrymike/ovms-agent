#include <stdio.h>
#include <string.h>
#include "EDIT_TXN.H"

int rms_replace_text_file(const char *path, const char *text)
{
    (void)path;
    (void)text;
    return 0;
}

#include "EDIT_TXN.C"

static int expect_equal(const char *left, const char *right)
{
    if (!edit_txn_path_equal(left, right)) {
        (void)printf("Expected equivalent transaction paths: %s <> %s\n",
                     left, right);
        return 0;
    }
    return 1;
}

static int expect_distinct(const char *left, const char *right)
{
    if (edit_txn_path_equal(left, right)) {
        (void)printf("Expected distinct transaction paths: %s <> %s\n",
                     left, right);
        return 0;
    }
    return 1;
}

static int test_add_alias_rejected(void)
{
    edit_txn transaction;
    int ok;

    edit_txn_init(&transaction);
    ok = edit_txn_add(
        &transaction,
        "M311_MISSING_DIR/ALIAS.DAT",
        "A\n");
    if (!ok) {
        (void)puts("Unable to stage first M311 alias fixture target.");
        edit_txn_dispose(&transaction);
        return 0;
    }

    if (edit_txn_add(
            &transaction,
            "M311_MISSING_DIR/./ALIAS.DAT",
            "B\n")) {
        (void)puts("Redundant-dot alias was incorrectly staged twice.");
        edit_txn_dispose(&transaction);
        return 0;
    }

    if (transaction.file_count != 1U) {
        (void)puts("Alias rejection changed transaction file count.");
        edit_txn_dispose(&transaction);
        return 0;
    }

    edit_txn_dispose(&transaction);
    return 1;
}

static int test_distinct_adds(void)
{
    edit_txn transaction;
    int ok;

    edit_txn_init(&transaction);
    ok =
        edit_txn_add(
            &transaction,
            "M311_MISSING_DIR/A.DAT",
            "A\n") &&
        edit_txn_add(
            &transaction,
            "M311_MISSING_DIR/B.DAT",
            "B\n");

    if (!ok || transaction.file_count != 2U) {
        (void)puts("Distinct transaction targets did not stage independently.");
        edit_txn_dispose(&transaction);
        return 0;
    }

    edit_txn_dispose(&transaction);
    return 1;
}

int main(void)
{
    if (!expect_equal("M311_ALIAS.DAT", "./M311_ALIAS.DAT") ||
        !expect_equal("M311_ALIAS.DAT", "././M311_ALIAS.DAT") ||
        !expect_equal(
            "SUBDIR/M311_ALIAS.DAT",
            "SUBDIR/./M311_ALIAS.DAT") ||
        !expect_equal("M311_ALIAS.DAT", "m311_alias.dat") ||
        !expect_equal("M311_ALIAS.DAT", "[]M311_ALIAS.DAT") ||
        !expect_distinct(
            "SUBDIR/M311_A.DAT",
            "SUBDIR/M311_B.DAT") ||
        edit_txn_path_valid("../M311_BAD.DAT") ||
        !test_add_alias_rejected() ||
        !test_distinct_adds()) {
        (void)puts("M311 transaction path canonicalization regression failed.");
        return 2;
    }

    (void)puts("M311 transaction path canonicalization regression passed.");
    return 0;
}
