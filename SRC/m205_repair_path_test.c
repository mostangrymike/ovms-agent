#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"

#define M205_EXISTING "[.TEST]M205_REPAIR_EXISTING.TXT"
#define M205_CREATED  "[.TEST]M205_REPAIR_CREATED.TXT"

static void remove_all_versions(const char *path)
{
    while (remove(path) == 0) {
        /* Remove every visible OpenVMS version. */
    }
}

static int write_text(const char *path, const char *text)
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

static int text_equals(const char *path, const char *expected)
{
    FILE *file;
    char buffer[256];
    size_t used;
    int extra;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    used = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    extra = fgetc(file);
    (void)fclose(file);

    buffer[used] = '\0';

    return extra == EOF && strcmp(buffer, expected) == 0;
}

static int file_missing(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int run_success_case(void)
{
    edit_txn transaction;
    int ok;

    remove_all_versions(M205_EXISTING);
    remove_all_versions(M205_CREATED);

    if (!write_text(M205_EXISTING, "broken\n")) {
        return 0;
    }

    edit_txn_init(&transaction);

    ok =
        edit_txn_add(&transaction, M205_EXISTING, "repaired\n") &&
        edit_txn_add(&transaction, M205_CREATED, "generated\n") &&
        edit_txn_write(&transaction);

    /*
     * Simulated controlled rebuild success: the executor commits the same
     * transaction only after receiving an odd OpenVMS success status.
     */
    if (ok) {
        ok = edit_txn_commit(&transaction);
    }

    edit_txn_dispose(&transaction);

    ok =
        ok &&
        text_equals(M205_EXISTING, "repaired\n") &&
        text_equals(M205_CREATED, "generated\n");

    remove_all_versions(M205_EXISTING);
    remove_all_versions(M205_CREATED);
    return ok;
}

static int run_failure_case(void)
{
    edit_txn transaction;
    int ok;
    int rollback_ok;

    remove_all_versions(M205_EXISTING);
    remove_all_versions(M205_CREATED);

    if (!write_text(M205_EXISTING, "original\n")) {
        return 0;
    }

    edit_txn_init(&transaction);

    ok =
        edit_txn_add(&transaction, M205_EXISTING, "still broken\n") &&
        edit_txn_add(&transaction, M205_CREATED, "temporary\n") &&
        edit_txn_write(&transaction);

    /*
     * Simulated controlled rebuild failure: the executor rolls back the
     * complete transaction and must restore replacements and creations.
     */
    rollback_ok = ok ? edit_txn_rollback(&transaction) : 0;
    edit_txn_dispose(&transaction);

    ok =
        ok &&
        rollback_ok &&
        text_equals(M205_EXISTING, "original\n") &&
        file_missing(M205_CREATED);

    remove_all_versions(M205_EXISTING);
    remove_all_versions(M205_CREATED);
    return ok;
}

int main(void)
{
    if (!run_success_case()) {
        (void)puts("M205 repair commit fixture failed.");
        return EXIT_FAILURE;
    }

    if (!run_failure_case()) {
        (void)puts("M205 repair rollback fixture failed.");
        return EXIT_FAILURE;
    }

    (void)puts("Controlled repair transaction test passed.");
    return EXIT_SUCCESS;
}
