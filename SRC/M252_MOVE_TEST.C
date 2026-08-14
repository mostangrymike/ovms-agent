#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

static void m252_move_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m252_move_seed(
    const char *path,
    const char *text,
    char *exact_spec)
{
    FILE *file;

    m252_move_cleanup(path);

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

static int m252_move_read(
    const char *path,
    const char *expected_text,
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
        strcmp(line, expected_text) != 0 ||
        fgets(line, sizeof(line), file) != NULL) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int m252_move_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m252_move_invalid(void)
{
    edit_txn txn;

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "M252_MOVE_A.DAT;1",
            "M252_MOVE_B.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "SRC/M252_MOVE_A.DAT;1",
            "SRC/M252_MOVE_B.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_rename(
            &txn,
            "SRC/M252_MOVE_A.DAT;1",
            "DOC/M252_MOVE_B.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "M252_MOVE_A.DAT",
            "[.TEST]M252_MOVE_A.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "M252_MOVE_A.DAT;1",
            "[.TEST]M252_MOVE_A.DAT;0")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "M252_MOVE_A.DAT;1",
            "[.TEST]M252_MOVE_*.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "../M252_MOVE_A.DAT;1",
            "[.TEST]M252_MOVE_A.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    edit_txn_init(&txn);
    if (edit_txn_add_move(
            &txn,
            "SYS$DISK:M252_MOVE_A.DAT;1",
            "[.TEST]M252_MOVE_A.DAT;1")) {
        edit_txn_dispose(&txn);
        return 0;
    }
    edit_txn_dispose(&txn);

    return 1;
}

static int m252_move_rollback(void)
{
    const char *base = "M252_MOVE_ROLL.DAT";
    const char *source = "M252_MOVE_ROLL.DAT;1";
    const char *target = "[.TEST]M252_MOVE_ROLL.DAT;1";
    const char *text = "M252 MOVE ROLLBACK ORIGINAL\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    char moved_spec[EDIT_TXN_PATH_SIZE];
    char restored_spec[EDIT_TXN_PATH_SIZE];
    edit_txn txn;
    int result;

    m252_move_cleanup(base);
    m252_move_cleanup(target);

    if (!m252_move_seed(base, text, original_spec)) {
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_move(&txn, source, target) &&
        edit_txn_write(&txn) &&
        m252_move_absent(source) &&
        m252_move_read(target, text, moved_spec) &&
        strcmp(moved_spec, original_spec) != 0 &&
        edit_txn_rollback(&txn) &&
        m252_move_read(source, text, restored_spec) &&
        strcmp(restored_spec, original_spec) == 0 &&
        m252_move_absent(target);

    edit_txn_dispose(&txn);
    m252_move_cleanup(base);
    m252_move_cleanup(target);
    return result;
}

static int m252_move_commit(void)
{
    const char *base = "M252_MOVE_COMMIT.DAT";
    const char *source = "M252_MOVE_COMMIT.DAT;1";
    const char *target = "[.TEST]M252_MOVE_COMMIT.DAT;1";
    const char *text = "M252 MOVE COMMIT ORIGINAL\n";
    char original_spec[EDIT_TXN_PATH_SIZE];
    char moved_spec[EDIT_TXN_PATH_SIZE];
    edit_txn txn;
    int result;

    m252_move_cleanup(base);
    m252_move_cleanup(target);

    if (!m252_move_seed(base, text, original_spec)) {
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_move(&txn, source, target) &&
        edit_txn_write(&txn) &&
        edit_txn_commit(&txn) &&
        m252_move_absent(source) &&
        m252_move_read(target, text, moved_spec) &&
        strcmp(moved_spec, original_spec) != 0;

    edit_txn_dispose(&txn);
    m252_move_cleanup(base);
    m252_move_cleanup(target);
    return result;
}

int main(void)
{
    if (!m252_move_invalid()) {
        (void)puts("M252 move path validation failed.");
        return EXIT_FAILURE;
    }

    if (!m252_move_rollback()) {
        (void)puts("M252 exact-version move rollback failed.");
        return EXIT_FAILURE;
    }

    if (!m252_move_commit()) {
        (void)puts("M252 exact-version move commit failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M252 exact-version move tests passed.");
    return EXIT_SUCCESS;
}
