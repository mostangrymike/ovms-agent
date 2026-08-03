#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_txn.h"
#include "rms_write.h"

extern int fgetname(FILE *, char *);

static int edit_txn_get_spec(
    const char *path,
    char *spec,
    size_t spec_size)
{
    FILE *file;
    int status;

    if (path == NULL ||
        spec == NULL ||
        spec_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    status = fgetname(file, spec);

    (void)fclose(file);

    if (status == 0) {
        return 0;
    }

    spec[spec_size - 1U] = '\0';
    return 1;
}


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

static char *edit_txn_read_text(const char *path)
{
    FILE *file;
    char *text = NULL;
    char buf[4096];
    size_t size = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    while (fgets(buf, sizeof(buf), file) != NULL) {
        size_t len = strlen(buf);
        char *new_text = (char *)realloc(text, size + len + 1U);
        if (new_text == NULL) {
            free(text);
            (void)fclose(file);
            return NULL;
        }
        text = new_text;
        (void)memcpy(text + size, buf, len);
        size += len;
        text[size] = '\0';
    }

    if (ferror(file)) {
        free(text);
        (void)fclose(file);
        return NULL;
    }

    (void)fclose(file);

    if (text == NULL) {
        text = (char *)malloc(1U);
        if (text == NULL) {
            return NULL;
        }
        text[0] = '\0';
    }

    return text;
}

static int edit_txn_file_exists(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
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
    file->original_text = NULL;
    file->original_spec[0] = '\0';
    file->existed_before = edit_txn_file_exists(path) ? 1U : 0U;

    if (file->existed_before) {
        file->original_text = edit_txn_read_text(path);

        if (file->original_text == NULL) {
            free(file->replacement_text);
            file->replacement_text = NULL;
            file->path[0] = '\0';
            return 0;
        }

        if (!edit_txn_get_spec(
                path,
                file->original_spec,
                sizeof file->original_spec)) {
            free(file->replacement_text);
            file->replacement_text = NULL;
            free(file->original_text);
            file->original_text = NULL;
            file->path[0] = '\0';
            return 0;
        }
    }

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
            if (edit_txn_file_exists(file->path)) {
                file->written = 1U;
            }
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

        if (file->existed_before) {
            if (file->original_text == NULL) {
                success = 0;
            } else {
                char current_spec[EDIT_TXN_PATH_SIZE];

                for (;;) {
                    if (!edit_txn_get_spec(
                            file->path,
                            current_spec,
                            sizeof current_spec)) {
                        success = 0;
                        break;
                    }

                    if (strcmp(
                            current_spec,
                            file->original_spec) == 0) {
                        file->written = 0U;
                        break;
                    }

                    if (remove(current_spec) != 0) {
                        success = 0;
                        break;
                    }
                }
            }
        } else {
            if (remove(file->path) != 0) {
                success = 0;
            } else {
                file->written = 0U;
            }
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

        free(
            transaction->files[index].
                original_text
        );
        transaction->files[index].
            original_text = NULL;
    }

    (void)memset(
        transaction,
        0,
        sizeof(*transaction)
    );
}
