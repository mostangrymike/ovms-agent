#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unixio.h>
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

static void cleanup_invalid_add_recovery(void)
{
    for (;;) {
        if (remove("TXN_VALID_AFTER_INVALID.DAT") != 0) {
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

static int test_invalid_add_recovery(void)
{
    edit_txn transaction;

    cleanup_invalid_add_recovery();

    edit_txn_init(&transaction);

    if (edit_txn_add(&transaction, "", "SHOULD NOT MATTER\n") ||
        edit_txn_add(&transaction, "../BAD.DAT", "SHOULD NOT MATTER\n") ||
        edit_txn_add(&transaction, "SYS$DISK:BAD.DAT", "SHOULD NOT MATTER\n")) {
        edit_txn_dispose(&transaction);
        cleanup_invalid_add_recovery();
        return EXIT_FAILURE;
    }

    if (!edit_txn_add(&transaction, "TXN_VALID_AFTER_INVALID.DAT",
                      "VALID AFTER INVALID\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_invalid_add_recovery();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_VALID_AFTER_INVALID.DAT",
                        "VALID AFTER INVALID\n") != EXIT_SUCCESS) {
        cleanup_invalid_add_recovery();
        return EXIT_FAILURE;
    }

    cleanup_invalid_add_recovery();
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

static int create_existing_multi_seed(const char *path, const char *text, char *exact_spec)
{
    FILE *file = NULL;

    file = fopen(path, "w", "ctx=stm");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        return EXIT_FAILURE;
    }
    file = NULL;

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    if (fgetname(file, exact_spec) == 0) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        return EXIT_FAILURE;
    }
    file = NULL;

    return EXIT_SUCCESS;
}

static int create_existing_record_seed(const char *path, const char *text, char *exact_spec)
{
    int fd;
    int written;
    FILE *file = NULL;

    fd = creat(path, 0, "rat=cr", "rfm=var");
    if (fd == -1) {
        return EXIT_FAILURE;
    }

    written = write(fd, text, strlen(text));
    if (written != (int)strlen(text)) {
        (void)close(fd);
        return EXIT_FAILURE;
    }

    if (close(fd) == -1) {
        return EXIT_FAILURE;
    }

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    if (fgetname(file, exact_spec) == 0) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
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

static void cleanup_multi_file_failure(void)
{
    for (;;) {
        if (remove("TXN_FAIL_ONE.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_FAIL_TWO.OPT") != 0) {
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

static int test_multi_file_failure(void)
{
    edit_txn transaction;
    char *long_text;
    size_t long_len = 32768;
    size_t i;
    FILE *file;

    cleanup_multi_file_failure();

    long_text = malloc(long_len + 2);
    if (long_text == NULL) {
        return EXIT_FAILURE;
    }

    for (i = 0; i < long_len; i++) {
        long_text[i] = 'X';
    }
    long_text[long_len] = '\n';
    long_text[long_len + 1] = '\0';

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_FAIL_ONE.DAT", "ONE LINE\n") ||
        !edit_txn_add(&transaction, "TXN_FAIL_TWO.OPT", long_text) ||
        edit_txn_write(&transaction)) {
        /* We expect edit_txn_write to fail. */
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_multi_file_failure();
        return EXIT_FAILURE;
    }

    free(long_text);
    edit_txn_dispose(&transaction);

    file = fopen("TXN_FAIL_ONE.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_multi_file_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_FAIL_TWO.OPT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_multi_file_failure();
        return EXIT_FAILURE;
    }

    cleanup_multi_file_failure();
    return EXIT_SUCCESS;
}

static int verify_existing_multi_seed(const char *path, const char *exact_spec, const char *expected_text)
{
    FILE *file = NULL;
    char name[EDIT_TXN_PATH_SIZE];
    char line[256];

    file = fopen(path, "r");
    if (file == NULL) {
        return EXIT_FAILURE;
    }

    if (fgetname(file, name) == 0) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (strcmp(name, exact_spec) != 0) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (fgets(line, sizeof line, file) == NULL) {
        (void)fclose(file);
        return EXIT_FAILURE;
    }

    if (strcmp(line, expected_text) != 0) {
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

static void cleanup_existing_multi_failure(void)
{
    for (;;) {
        if (remove("TXN_EXIST_ONE.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_EXIST_TWO.OPT") != 0) {
            break;
        }
    }
}

static int test_existing_multi_file_failure(void)
{
    edit_txn transaction;
    char *long_text;
    size_t long_len = 32768;
    size_t i;
    char spec_one[EDIT_TXN_PATH_SIZE];
    char spec_two[EDIT_TXN_PATH_SIZE];

    cleanup_existing_multi_failure();

    if (create_existing_multi_seed("TXN_EXIST_ONE.DAT", "ONE ORIGINAL\n", spec_one) != EXIT_SUCCESS) {
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }

    if (create_existing_record_seed("TXN_EXIST_TWO.OPT", "TWO ORIGINAL\n", spec_two) != EXIT_SUCCESS) {
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }

    long_text = malloc(long_len + 2);
    if (long_text == NULL) {
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }

    for (i = 0; i < long_len; i++) {
        long_text[i] = 'X';
    }
    long_text[long_len] = '\n';
    long_text[long_len + 1] = '\0';

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_EXIST_ONE.DAT", "ONE REPLACED\n") ||
        !edit_txn_add(&transaction, "TXN_EXIST_TWO.OPT", long_text) ||
        edit_txn_write(&transaction)) {
        /* We expect edit_txn_write to fail. */
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }

    free(long_text);
    edit_txn_dispose(&transaction);

    if (verify_existing_multi_seed("TXN_EXIST_ONE.DAT", spec_one, "ONE ORIGINAL\n") != EXIT_SUCCESS) {
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }

    if (verify_existing_multi_seed("TXN_EXIST_TWO.OPT", spec_two, "TWO ORIGINAL\n") != EXIT_SUCCESS) {
        cleanup_existing_multi_failure();
        return EXIT_FAILURE;
    }
    cleanup_existing_multi_failure();
    return EXIT_SUCCESS;
}

static void cleanup_mixed_multi_failure(void)
{
    for (;;) {
        if (remove("TXN_MIX_EXIST.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_MIX_NEW.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_MIX_FAIL.OPT") != 0) {
            break;
        }
    }
}

static int test_mixed_multi_file_failure(void)
{
    edit_txn transaction;
    char *long_text;
    size_t long_len = 32768;
    size_t i;
    char spec_exist[EDIT_TXN_PATH_SIZE];
    FILE *file;

    cleanup_mixed_multi_failure();

    if (create_existing_multi_seed("TXN_MIX_EXIST.DAT", "MIX ORIGINAL\n", spec_exist) != EXIT_SUCCESS) {
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    long_text = malloc(long_len + 2);
    if (long_text == NULL) {
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    for (i = 0; i < long_len; i++) {
        long_text[i] = 'X';
    }
    long_text[long_len] = '\n';
    long_text[long_len + 1] = '\0';

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_MIX_EXIST.DAT", "MIX REPLACED\n") ||
        !edit_txn_add(&transaction, "TXN_MIX_NEW.DAT", "MIX NEW\n") ||
        !edit_txn_add(&transaction, "TXN_MIX_FAIL.OPT", long_text) ||
        edit_txn_write(&transaction)) {
        /* We expect edit_txn_write to fail. */
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    if (!edit_txn_rollback(&transaction) ||
        edit_txn_write(&transaction) ||
        edit_txn_commit(&transaction)) {
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    free(long_text);
    edit_txn_dispose(&transaction);

    file = fopen("TXN_MIX_EXIST.DAT", "r", "ctx=stm");
    if (file == NULL) {
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    if (fclose(file) != 0) {
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    if (verify_existing_multi_seed("TXN_MIX_EXIST.DAT", spec_exist, "MIX ORIGINAL\n") != EXIT_SUCCESS) {
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_MIX_NEW.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_MIX_FAIL.OPT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_mixed_multi_failure();
        return EXIT_FAILURE;
    }

    cleanup_mixed_multi_failure();
    return EXIT_SUCCESS;
}

static void cleanup_large_mixed_failure(void)
{
    for (;;) {
        if (remove("TXN_LARGE_EXIST_ONE.DAT") != 0) break;
    }
    for (;;) {
        if (remove("TXN_LARGE_EXIST_TWO.DAT") != 0) break;
    }
    for (;;) {
        if (remove("TXN_LARGE_NEW_ONE.DAT") != 0) break;
    }
    for (;;) {
        if (remove("TXN_LARGE_NEW_TWO.DAT") != 0) break;
    }
    for (;;) {
        if (remove("TXN_LARGE_FAIL.OPT") != 0) break;
    }
}

static int test_large_mixed_multi_failure(void)
{
    edit_txn transaction;
    char *long_text;
    size_t long_len = 32768;
    size_t i;
    char spec_exist_one[EDIT_TXN_PATH_SIZE];
    char spec_exist_two[EDIT_TXN_PATH_SIZE];
    FILE *file;

    cleanup_large_mixed_failure();

    if (create_existing_multi_seed("TXN_LARGE_EXIST_ONE.DAT", "LARGE ONE ORIGINAL\n", spec_exist_one) != EXIT_SUCCESS) {
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    if (create_existing_multi_seed("TXN_LARGE_EXIST_TWO.DAT", "LARGE TWO ORIGINAL\n", spec_exist_two) != EXIT_SUCCESS) {
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    long_text = malloc(long_len + 2);
    if (long_text == NULL) {
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    for (i = 0; i < long_len; i++) {
        long_text[i] = 'X';
    }
    long_text[long_len] = '\n';
    long_text[long_len + 1] = '\0';

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_LARGE_EXIST_ONE.DAT", "LARGE ONE REPLACED\n") ||
        !edit_txn_add(&transaction, "TXN_LARGE_EXIST_TWO.DAT", "LARGE TWO REPLACED\n") ||
        !edit_txn_add(&transaction, "TXN_LARGE_NEW_ONE.DAT", "LARGE NEW ONE\n") ||
        !edit_txn_add(&transaction, "TXN_LARGE_NEW_TWO.DAT", "LARGE NEW TWO\n") ||
        !edit_txn_add(&transaction, "TXN_LARGE_FAIL.OPT", long_text) ||
        edit_txn_write(&transaction)) {
        /* We expect edit_txn_write to fail. */
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    free(long_text);
    edit_txn_dispose(&transaction);

    if (verify_existing_multi_seed("TXN_LARGE_EXIST_ONE.DAT", spec_exist_one, "LARGE ONE ORIGINAL\n") != EXIT_SUCCESS) {
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    if (verify_existing_multi_seed("TXN_LARGE_EXIST_TWO.DAT", spec_exist_two, "LARGE TWO ORIGINAL\n") != EXIT_SUCCESS) {
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_LARGE_NEW_ONE.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_LARGE_NEW_TWO.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_LARGE_FAIL.OPT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_large_mixed_failure();
        return EXIT_FAILURE;
    }

    cleanup_large_mixed_failure();
    return EXIT_SUCCESS;
}

static void cleanup_duplicate_add_recovery(void)
{
    for (;;) {
        if (remove("TXN_DUPLICATE_ADD.DAT") != 0) {
            break;
        }
    }
}

static int test_duplicate_add_recovery(void)
{
    edit_txn transaction;

    cleanup_duplicate_add_recovery();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUPLICATE_ADD.DAT",
                      "ORIGINAL ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_duplicate_add_recovery();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "TXN_DUPLICATE_ADD.DAT",
                     "DUPLICATE ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_duplicate_add_recovery();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_duplicate_add_recovery();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_DUPLICATE_ADD.DAT",
                        "ORIGINAL ENTRY\n") != EXIT_SUCCESS) {
        cleanup_duplicate_add_recovery();
        return EXIT_FAILURE;
    }

    cleanup_duplicate_add_recovery();
    return EXIT_SUCCESS;
}

static void cleanup_case_duplicate(void)
{
    for (;;) {
        if (remove("TXN_CASE_DUP.DAT") != 0) {
            break;
        }
    }
}

static int test_case_duplicate(void)
{
    edit_txn transaction;

    cleanup_case_duplicate();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_CASE_DUP.DAT",
                      "ORIGINAL CASE ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_case_duplicate();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "txn_case_dup.dat",
                     "LOWERCASE DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_case_duplicate();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_case_duplicate();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_CASE_DUP.DAT",
                        "ORIGINAL CASE ENTRY\n") != EXIT_SUCCESS) {
        cleanup_case_duplicate();
        return EXIT_FAILURE;
    }

    cleanup_case_duplicate();
    return EXIT_SUCCESS;
}

static void cleanup_equivalent_duplicate(void)
{
    for (;;) {
        if (remove("TXN_EQUIV.DAT") != 0) {
            break;
        }
    }
}

static int test_equivalent_duplicate(void)
{
    edit_txn transaction;

    cleanup_equivalent_duplicate();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_EQUIV.DAT",
                      "ORIGINAL EQUIVALENT ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_equivalent_duplicate();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "[]TXN_EQUIV.DAT",
                     "BRACKET DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_equivalent_duplicate();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_equivalent_duplicate();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_EQUIV.DAT",
                        "ORIGINAL EQUIVALENT ENTRY\n") != EXIT_SUCCESS) {
        cleanup_equivalent_duplicate();
        return EXIT_FAILURE;
    }

    cleanup_equivalent_duplicate();
    return EXIT_SUCCESS;
}

static void cleanup_reverse_equivalent(void)
{
    for (;;) {
        if (remove("TXN_EQUIV_REV.DAT") != 0) {
            break;
        }
    }
}

static int test_reverse_equivalent(void)
{
    edit_txn transaction;

    cleanup_reverse_equivalent();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "[]TXN_EQUIV_REV.DAT",
                      "BRACKET FIRST ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_reverse_equivalent();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "TXN_EQUIV_REV.DAT",
                     "PLAIN DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_reverse_equivalent();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_reverse_equivalent();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_EQUIV_REV.DAT",
                        "BRACKET FIRST ENTRY\n") != EXIT_SUCCESS) {
        cleanup_reverse_equivalent();
        return EXIT_FAILURE;
    }

    cleanup_reverse_equivalent();
    return EXIT_SUCCESS;
}

static void cleanup_combined_duplicate(void)
{
    for (;;) {
        if (remove("TXN_COMBINED_DUP.DAT") != 0) {
            break;
        }
    }
}

static int test_combined_duplicate(void)
{
    edit_txn transaction;

    cleanup_combined_duplicate();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_COMBINED_DUP.DAT",
                      "ORIGINAL COMBINED ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_combined_duplicate();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "[]txn_combined_dup.dat",
                     "COMBINED DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_combined_duplicate();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_combined_duplicate();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_COMBINED_DUP.DAT",
                        "ORIGINAL COMBINED ENTRY\n") != EXIT_SUCCESS) {
        cleanup_combined_duplicate();
        return EXIT_FAILURE;
    }

    cleanup_combined_duplicate();
    return EXIT_SUCCESS;
}

static void cleanup_combined_reverse(void)
{
    for (;;) {
        if (remove("TXN_COMBINED_REV.DAT") != 0) {
            break;
        }
    }
}

static int test_combined_reverse(void)
{
    edit_txn transaction;

    cleanup_combined_reverse();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "[]TXN_COMBINED_REV.DAT",
                      "BRACKET COMBINED ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_combined_reverse();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "txn_combined_rev.dat",
                     "LOWERCASE DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_combined_reverse();
        return EXIT_FAILURE;
    }

    if (!edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_combined_reverse();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_COMBINED_REV.DAT",
                        "BRACKET COMBINED ENTRY\n") != EXIT_SUCCESS) {
        cleanup_combined_reverse();
        return EXIT_FAILURE;
    }

    cleanup_combined_reverse();
    return EXIT_SUCCESS;
}

static void cleanup_dup_recovery_multi(void)
{
    for (;;) {
        if (remove("TXN_DUP_RECOVER_ONE.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_DUP_RECOVER_TWO.DAT") != 0) {
            break;
        }
    }
}

static int test_dup_recovery_multi(void)
{
    edit_txn transaction;

    cleanup_dup_recovery_multi();

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUP_RECOVER_ONE.DAT",
                      "FIRST VALID ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_dup_recovery_multi();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "[]txn_dup_recover_one.dat",
                     "REJECTED DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_dup_recovery_multi();
        return EXIT_FAILURE;
    }

    if (!edit_txn_add(&transaction, "TXN_DUP_RECOVER_TWO.DAT",
                      "SECOND VALID ENTRY\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_dup_recovery_multi();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_DUP_RECOVER_ONE.DAT",
                        "FIRST VALID ENTRY\n") != EXIT_SUCCESS ||
        verify_one_line("TXN_DUP_RECOVER_TWO.DAT",
                        "SECOND VALID ENTRY\n") != EXIT_SUCCESS) {
        cleanup_dup_recovery_multi();
        return EXIT_FAILURE;
    }

    cleanup_dup_recovery_multi();
    return EXIT_SUCCESS;
}

static void cleanup_existing_dup_recovery(void)
{
    for (;;) {
        if (remove("TXN_DUP_EXIST_ONE.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_DUP_EXIST_TWO.DAT") != 0) {
            break;
        }
    }
}

static int test_existing_dup_recovery(void)
{
    edit_txn transaction;
    char original_spec[EDIT_TXN_PATH_SIZE];

    cleanup_existing_dup_recovery();

    if (create_existing_multi_seed(
            "TXN_DUP_EXIST_ONE.DAT",
            "ORIGINAL EXISTING ENTRY\n",
            original_spec) != EXIT_SUCCESS) {
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUP_EXIST_ONE.DAT",
                      "REPLACED EXISTING ENTRY\n")) {
        edit_txn_dispose(&transaction);
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "[]txn_dup_exist_one.dat",
                     "REJECTED DUPLICATE\n")) {
        edit_txn_dispose(&transaction);
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    if (!edit_txn_add(&transaction, "TXN_DUP_EXIST_TWO.DAT",
                      "SECOND VALID ENTRY\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_DUP_EXIST_ONE.DAT",
                        "REPLACED EXISTING ENTRY\n") != EXIT_SUCCESS ||
        verify_one_line("TXN_DUP_EXIST_TWO.DAT",
                        "SECOND VALID ENTRY\n") != EXIT_SUCCESS) {
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    if (verify_one_line(original_spec,
                        "ORIGINAL EXISTING ENTRY\n") != EXIT_SUCCESS) {
        cleanup_existing_dup_recovery();
        return EXIT_FAILURE;
    }

    cleanup_existing_dup_recovery();
    return EXIT_SUCCESS;
}

static void cleanup_dup_failure_rollback(void)
{
    for (;;) {
        if (remove("TXN_DUP_FAIL_EXIST.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_DUP_FAIL_NEW.DAT") != 0) {
            break;
        }
    }

    for (;;) {
        if (remove("TXN_DUP_FAIL_LONG.OPT") != 0) {
            break;
        }
    }
    for (;;) {
        if (remove("TXN_DUP_FAIL_REUSE.DAT") != 0) {
            break;
        }
    }
    for (;;) {
        if (remove("TXN_DUP_FAIL_REUSE_TWO.DAT") != 0) {
            break;
        }
    }
}

static int test_dup_failure_rollback(void)
{
    edit_txn transaction;
    char original_spec[EDIT_TXN_PATH_SIZE];
    char *long_text;
    size_t long_len = 32768;
    size_t i;
    FILE *file;

    cleanup_dup_failure_rollback();

    if (create_existing_multi_seed(
            "TXN_DUP_FAIL_EXIST.DAT",
            "ORIGINAL FAILURE ENTRY\n",
            original_spec) != EXIT_SUCCESS) {
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    long_text = malloc(long_len + 2);
    if (long_text == NULL) {
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    for (i = 0; i < long_len; ++i) {
        long_text[i] = 'X';
    }

    long_text[long_len] = '\n';
    long_text[long_len + 1] = '\0';

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUP_FAIL_EXIST.DAT",
                      "REPLACED FAILURE ENTRY\n")) {
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    if (edit_txn_add(&transaction, "[]txn_dup_fail_exist.dat",
                     "REJECTED DUPLICATE\n")) {
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    if (!edit_txn_add(&transaction, "TXN_DUP_FAIL_NEW.DAT",
                      "NEW FAILURE ENTRY\n") ||
        !edit_txn_add(&transaction, "TXN_DUP_FAIL_LONG.OPT",
                      long_text) ||
        edit_txn_write(&transaction)) {
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    if (!edit_txn_rollback(&transaction) ||
        edit_txn_write(&transaction) ||
        edit_txn_commit(&transaction)) {
        free(long_text);
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    free(long_text);
    edit_txn_dispose(&transaction);
    edit_txn_dispose(&transaction);

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUP_FAIL_REUSE.DAT",
                      "REUSED TRANSACTION ENTRY\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_DUP_FAIL_REUSE.DAT",
                        "REUSED TRANSACTION ENTRY\n") != EXIT_SUCCESS) {
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    edit_txn_init(&transaction);

    if (!edit_txn_add(&transaction, "TXN_DUP_FAIL_REUSE_TWO.DAT",
                      "SECOND REUSED ENTRY\n") ||
        !edit_txn_write(&transaction) ||
        !edit_txn_commit(&transaction)) {
        edit_txn_dispose(&transaction);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);

    if (verify_one_line("TXN_DUP_FAIL_REUSE_TWO.DAT",
                        "SECOND REUSED ENTRY\n") != EXIT_SUCCESS) {
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    if (verify_existing_multi_seed(
            "TXN_DUP_FAIL_EXIST.DAT",
            original_spec,
            "ORIGINAL FAILURE ENTRY\n") != EXIT_SUCCESS) {
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_DUP_FAIL_NEW.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    file = fopen("TXN_DUP_FAIL_LONG.OPT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_dup_failure_rollback();
        return EXIT_FAILURE;
    }

    cleanup_dup_failure_rollback();
    return EXIT_SUCCESS;
}

static void cleanup_add_limit_failure(void)
{
    int i;
    char name[32];

    for (i = 0; i <= 31; i++) {
        (void)sprintf(name, "TXN_ADD_LIMIT_%02d.DAT", i);
        for (;;) {
            if (remove(name) != 0) {
                break;
            }
        }
    }

    for (;;) {
        if (remove("TXN_ADD_LIMIT_OVER.DAT") != 0) {
            break;
        }
    }
}

static int test_add_limit_failure(void)
{
    edit_txn transaction;
    int i;
    char name[32];
    FILE *file;

    cleanup_add_limit_failure();

    edit_txn_init(&transaction);

    for (i = 0; i < 32; i++) {
        (void)sprintf(name, "TXN_ADD_LIMIT_%02d.DAT", i);
        if (!edit_txn_add(&transaction, name, "LIMIT TEST\n")) {
            edit_txn_dispose(&transaction);
            cleanup_add_limit_failure();
            return EXIT_FAILURE;
        }
    }

    if (edit_txn_add(&transaction, "TXN_ADD_LIMIT_OVER.DAT", "OVER LIMIT\n")) {
        edit_txn_dispose(&transaction);
        cleanup_add_limit_failure();
        return EXIT_FAILURE;
    }

    edit_txn_dispose(&transaction);
    edit_txn_dispose(&transaction);

    for (i = 0; i < 32; i++) {
        (void)sprintf(name, "TXN_ADD_LIMIT_%02d.DAT", i);
        file = fopen(name, "r", "ctx=stm");
        if (file != NULL) {
            (void)fclose(file);
            cleanup_add_limit_failure();
            return EXIT_FAILURE;
        }
    }

    file = fopen("TXN_ADD_LIMIT_OVER.DAT", "r", "ctx=stm");
    if (file != NULL) {
        (void)fclose(file);
        cleanup_add_limit_failure();
        return EXIT_FAILURE;
    }

    cleanup_add_limit_failure();
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

    if (test_large_mixed_multi_failure() != EXIT_SUCCESS) {
        (void)puts("Large mixed multi-file failure rollback test failed.");
        return EXIT_FAILURE;
    }

    if (test_mixed_multi_file_failure() != EXIT_SUCCESS) {
        (void)puts("Mixed multi-file failure rollback test failed.");
        return EXIT_FAILURE;
    }

    if (test_existing_multi_file_failure() != EXIT_SUCCESS) {
        (void)puts("Existing multi-file failure rollback test failed.");
        return EXIT_FAILURE;
    }

    if (test_multi_file_failure() != EXIT_SUCCESS) {
        (void)puts("Multi-file failure rollback test failed.");
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

    if (test_dup_failure_rollback() != EXIT_SUCCESS) {
        (void)puts("Duplicate failure rollback test failed.");
        return EXIT_FAILURE;
    }

    if (test_existing_dup_recovery() != EXIT_SUCCESS) {
        (void)puts("Existing duplicate-recovery test failed.");
        return EXIT_FAILURE;
    }

    if (test_dup_recovery_multi() != EXIT_SUCCESS) {
        (void)puts("Duplicate-recovery multi-file test failed.");
        return EXIT_FAILURE;
    }

    if (test_combined_reverse() != EXIT_SUCCESS) {
        (void)puts("Reverse combined-path duplicate test failed.");
        return EXIT_FAILURE;
    }

    if (test_combined_duplicate() != EXIT_SUCCESS) {
        (void)puts("Combined-path duplicate test failed.");
        return EXIT_FAILURE;
    }

    if (test_reverse_equivalent() != EXIT_SUCCESS) {
        (void)puts("Reverse equivalent-path duplicate test failed.");
        return EXIT_FAILURE;
    }

    if (test_equivalent_duplicate() != EXIT_SUCCESS) {
        (void)puts("Equivalent-path duplicate test failed.");
        return EXIT_FAILURE;
    }

    if (test_case_duplicate() != EXIT_SUCCESS) {
        (void)puts("Case-duplicate recovery test failed.");
        return EXIT_FAILURE;
    }
    if (test_duplicate_add_recovery() != EXIT_SUCCESS) {
        (void)puts("Duplicate-add recovery test failed.");
        return EXIT_FAILURE;
    }
    if (test_invalid_add_recovery() != EXIT_SUCCESS) {
        (void)puts("Invalid-add recovery test failed.");
        return EXIT_FAILURE;
    }
    if (test_add_limit_failure() != EXIT_SUCCESS) {
        (void)puts("Add-limit failure test failed.");
        return EXIT_FAILURE;
    }
    if (test_stream_lf_replacement() != EXIT_SUCCESS) {
        (void)puts("Stream_LF replacement test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("Create rollback test passed.");
    return EXIT_SUCCESS;
}
