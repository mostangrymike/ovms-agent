#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stat.h>
#include <rms.h>
#include "rms_write.h"
#include "edit_txn.h"

static void cleanup_seed(void)
{
    for (;;) {
        if (remove("TXN_ROLLBACK_SEED.DAT") != 0) {
            break;
        }
    }
}
static int test_existing_version(void)
{
    edit_txn transaction;
    FILE *file = NULL;
    char original_spec[EDIT_TXN_PATH_SIZE];
    char current_spec[EDIT_TXN_PATH_SIZE];
    char line[64];

    cleanup_seed();

    file = fopen("TXN_ROLLBACK_SEED.DAT", "w", "ctx=stm");
    if (file == NULL) {
        (void)puts("Failed to create seed file.");
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fputs("ORIGINAL LINE\n", file) == EOF) {
        if (fclose(file) != 0) {
            file = NULL;
            (void)puts("Failed to close seed file after write failure.");
            cleanup_seed();
            return EXIT_FAILURE;
        }
        file = NULL;
        (void)puts("Failed to write to seed file.");
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        file = NULL;
        (void)puts("Failed to close seed file after write.");
        cleanup_seed();
        return EXIT_FAILURE;
    }
    file = NULL;

    file = fopen("TXN_ROLLBACK_SEED.DAT", "r", "ctx=stm");
    if (file == NULL) {
        (void)puts("Failed to reopen seed file.");
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fgetname(file, original_spec) == 0) {
        if (fclose(file) != 0) {
            file = NULL;
            (void)puts("Failed to close seed file after fgetname failure.");
            cleanup_seed();
            return EXIT_FAILURE;
        }
        file = NULL;
        (void)puts("fgetname failed on seed file.");
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        file = NULL;
        (void)puts("Failed to close seed file after fgetname.");
        cleanup_seed();
        return EXIT_FAILURE;
    }
    file = NULL;

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_ROLLBACK_SEED.DAT",
                      "MODIFIED LINE\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_rollback(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_seed();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    file = fopen("TXN_ROLLBACK_SEED.DAT", "r", "ctx=stm");
    if (file == NULL) {
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fgetname(file, current_spec) == 0) {
        (void)fclose(file);
        file = NULL;
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (strcmp(original_spec, current_spec) != 0) {
        if (file != NULL) {
            (void)fclose(file);
            file = NULL;
        }
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fgets(line, sizeof line, file) == NULL) {
        if (file != NULL) {
            (void)fclose(file);
            file = NULL;
        }
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (strcmp(line, "ORIGINAL LINE\n") != 0) {
        if (file != NULL) {
            (void)fclose(file);
            file = NULL;
        }
        cleanup_seed();
        return EXIT_FAILURE;
    }

    if (fgets(line, sizeof line, file) != NULL) {
        if (file != NULL) {
            (void)fclose(file);
            file = NULL;
        }
        cleanup_seed();
        return EXIT_FAILURE;
    }

    (void)fclose(file);
    file = NULL;

    cleanup_seed();
    return EXIT_SUCCESS;
}

static void cleanup_stream_lf_test(void)
{
    for (;;) {
        if (remove("TXN_STREAM_LF_TEST.COM") != 0) {
            break;
        }
    }
}

static int verify_one_line(const char *spec, const char *expected)
{
    FILE *file = NULL;
    char line[256];

    file = fopen(spec, "r", "ctx=stm");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    if (fgets(line, sizeof line, file) == NULL) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (strcmp(line, expected) != 0) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fgets(line, sizeof line, file) != NULL) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
static int create_stream_lf_seed(char *original_spec)
{
    FILE *file = NULL;
    struct stat st;

    cleanup_stream_lf_test();

    file = fopen("TXN_STREAM_LF_TEST.COM", "w", "ctx=stm");
    if (file == NULL) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (fputs("ORIGINAL LINE\n", file) == EOF) {
        (void)fclose(file);
        file = NULL;
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        file = NULL;
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }
    file = NULL;

    if (stat("TXN_STREAM_LF_TEST.COM", &st) != 0) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (st.st_fab_rfm != FAB$C_STMLF) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_STREAM_LF_TEST.COM", "r", "ctx=stm");
    if (file == NULL) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (fgetname(file, original_spec) == 0) {
        (void)fclose(file);
        file = NULL;
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        file = NULL;
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }
    file = NULL;

    return EXIT_SUCCESS;
}

static int verify_stream_lf_versions(const char *original_spec)
{
    struct stat st;

    if (stat("TXN_STREAM_LF_TEST.COM", &st) != 0) {
        return EXIT_FAILURE;
    }

    if (st.st_fab_rfm != FAB$C_STMLF) {
        return EXIT_FAILURE;
    }

    if (verify_one_line("TXN_STREAM_LF_TEST.COM", "REPLACEMENT LINE\n") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (stat(original_spec, &st) != 0) {
        return EXIT_FAILURE;
    }

    if (st.st_fab_rfm != FAB$C_STMLF) {
        return EXIT_FAILURE;
    }

    if (verify_one_line(original_spec, "ORIGINAL LINE\n") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void cleanup_multi_file_commit(void)
{
    for (;;) {
        if (remove("TXN_MULTI_ONE.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_MULTI_TWO.DAT") != 0) {
            break;
        }
    }
}

static int test_multi_file_commit(void)
{
    edit_txn transaction;

    cleanup_multi_file_commit();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_MULTI_ONE.DAT", "ONE LINE\n") ||
        !edit_txn_add(&transaction, "TXN_MULTI_TWO.DAT", "TWO LINE\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_multi_file_commit();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_MULTI_ONE.DAT", "ONE LINE\n") != EXIT_SUCCESS ||
        verify_one_line("TXN_MULTI_TWO.DAT", "TWO LINE\n") != EXIT_SUCCESS) {
        cleanup_multi_file_commit();
        return EXIT_FAILURE;
    }

    cleanup_multi_file_commit();
    return EXIT_SUCCESS;
}

static int test_stream_lf_replacement(void)
{
    char original_spec[EDIT_TXN_PATH_SIZE];

    if (create_stream_lf_seed(original_spec) != EXIT_SUCCESS) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (rms_replace_text_file("TXN_STREAM_LF_TEST.COM","REPLACEMENT LINE\n") == 0) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    if (verify_stream_lf_versions(original_spec) != EXIT_SUCCESS) {
        cleanup_stream_lf_test();
        return EXIT_FAILURE;
    }

    cleanup_stream_lf_test();
    return EXIT_SUCCESS;
}

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

    if (test_multi_file_commit() != EXIT_SUCCESS) {
        (void)puts("Multi-file commit test failed.");
        return EXIT_FAILURE;
    }

    if (test_existing_version() != EXIT_SUCCESS) {
        (void)puts("Existing version rollback test failed.");
        return EXIT_FAILURE;
    }
    if (test_stream_lf_replacement() != EXIT_SUCCESS) {
        (void)puts("Stream_LF replacement test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("Create rollback test passed.");
    return EXIT_SUCCESS;
}
