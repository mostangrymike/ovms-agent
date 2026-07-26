#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"
#include "rms_write.h"

#define EDIT_TXN_COMMAND_SIZE 1200U

static char *edit_txn_copy_text(
    const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    (void)memcpy(copy, text, length + 1U);
    return copy;
}

static int edit_txn_path_valid(
    const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (strstr(path, "..") != NULL ||
        strchr(path, ':') != NULL) {
        return 0;
    }

    return strlen(path) < EDIT_TXN_PATH_SIZE;
}

static int edit_txn_find(
    const edit_txn *transaction,
    const char *path)
{
    unsigned int index;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        if (strcmp(
                transaction->files[index].path,
                path) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int edit_txn_restore_one(
    const char *path)
{
    char command[EDIT_TXN_COMMAND_SIZE];
    int status;

    (void)snprintf(
        command,
        sizeof(command),
        "COPY/NOLOG %s;-1 %s",
        path,
        path
    );

    status = system(command);

    return status == 0 ||
           (status & 1) != 0;
}

void edit_txn_init(
    edit_txn *transaction)
{
    if (transaction == NULL) {
        return;
    }

    (void)memset(
        transaction,
        0,
        sizeof(*transaction)
    );
    transaction->active = 1U;
}

int edit_txn_add(
    edit_txn *transaction,
    const char *path,
    const char *replacement_text)
{
    edit_txn_file *file;
    char *copy;

    if (transaction == NULL ||
        !transaction->active ||
        transaction->committed ||
        !edit_txn_path_valid(path) ||
        replacement_text == NULL) {
        return 0;
    }

    if (edit_txn_find(
            transaction,
            path) >= 0) {
        return 0;
    }

    if (transaction->file_count >=
        EDIT_TXN_MAX_FILES) {
        return 0;
    }

    copy = edit_txn_copy_text(
        replacement_text
    );

    if (copy == NULL) {
        return 0;
    }

    file =
        &transaction->files[
            transaction->file_count
        ];

    (void)strcpy(file->path, path);
    file->replacement_text = copy;
    file->written = 0U;
    ++transaction->file_count;

    return 1;
}

int edit_txn_write(
    edit_txn *transaction)
{
    unsigned int index;

    if (transaction == NULL ||
        !transaction->active ||
        transaction->committed ||
        transaction->file_count == 0U) {
        return 0;
    }

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        edit_txn_file *file;

        file = &transaction->files[index];

        if (!rms_replace_text_file(
                file->path,
                file->replacement_text)) {
            (void)edit_txn_rollback(
                transaction
            );
            return 0;
        }

        file->written = 1U;
    }

    return 1;
}

int edit_txn_commit(
    edit_txn *transaction)
{
    if (transaction == NULL ||
        !transaction->active ||
        transaction->committed) {
        return 0;
    }

    transaction->committed = 1U;
    transaction->active = 0U;
    return 1;
}

int edit_txn_rollback(
    edit_txn *transaction)
{
    unsigned int index;
    int success;

    if (transaction == NULL ||
        transaction->committed) {
        return 0;
    }

    success = 1;

    index = transaction->file_count;

    while (index > 0U) {
        edit_txn_file *file;

        --index;
        file = &transaction->files[index];

        if (!file->written) {
            continue;
        }

        if (!edit_txn_restore_one(
                file->path)) {
            success = 0;
        } else {
            file->written = 0U;
        }
    }

    transaction->active = 0U;
    return success;
}

void edit_txn_dispose(
    edit_txn *transaction)
{
    unsigned int index;

    if (transaction == NULL) {
        return;
    }

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        free(
            transaction->files[index].
                replacement_text
        );
        transaction->files[index].
            replacement_text = NULL;
    }

    (void)memset(
        transaction,
        0,
        sizeof(*transaction)
    );
}
