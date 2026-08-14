#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "edit_txn.h"
#include "rms_write.h"

extern int fgetname(FILE *, char *);

static unsigned int edit_txn_hold_serial = 0U;

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

static int edit_txn_versioned_path(
    const char *path)
{
    const char *semi;
    const char *cursor;
    unsigned long version;
    char *end;

    if (!edit_txn_path_valid(path) ||
        strchr(path, '*') != NULL ||
        strchr(path, '%') != NULL) {
        return 0;
    }

    semi = strrchr(path, ';');
    if (semi == NULL || semi[1] == '\0') {
        return 0;
    }

    cursor = semi + 1;
    while (*cursor != '\0') {
        if (!isdigit((unsigned char)*cursor)) {
            return 0;
        }
        ++cursor;
    }

    end = NULL;
    version = strtoul(semi + 1, &end, 10);
    return end != NULL && *end == '\0' && version > 0UL;
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

static int edit_txn_path_equal(
    const char *left,
    const char *right)
{
    unsigned char left_char;
    unsigned char right_char;

    if (left == NULL || right == NULL) {
        return 0;
    }

    if (left[0] == '[' && left[1] == ']') {
        left += 2;
    }

    if (right[0] == '[' && right[1] == ']') {
        right += 2;
    }

    while (*left != '\0' && *right != '\0') {
        left_char = (unsigned char)*left;
        right_char = (unsigned char)*right;

        if (toupper(left_char) != toupper(right_char)) {
            return 0;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int edit_txn_find(
    const edit_txn *transaction,
    const char *path)
{
    unsigned int index;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        if (edit_txn_path_equal(
                transaction->files[index].path,
                path)) {
            return (int)index;
        }
    }

    return -1;
}

static int edit_txn_make_hold(edit_txn_file *file)
{
    const char *split;
    const char *other;
    size_t prefix_length;
    unsigned int tries;
    int written;

    if (file == NULL || file->original_spec[0] == '\0') {
        return 0;
    }

    split = strrchr(file->original_spec, ']');
    other = strrchr(file->original_spec, '>');
    if (other != NULL && (split == NULL || other > split)) {
        split = other;
    }
    other = strrchr(file->original_spec, ':');
    if (other != NULL && (split == NULL || other > split)) {
        split = other;
    }

    prefix_length = split == NULL
        ? 0U
        : (size_t)(split - file->original_spec) + 1U;

    for (tries = 0U; tries < 1024U; ++tries) {
        ++edit_txn_hold_serial;
        written = snprintf(
            file->held_spec,
            sizeof(file->held_spec),
            "%.*sOVMS$DEL_%08X.TMP;1",
            (int)prefix_length,
            file->original_spec,
            edit_txn_hold_serial);

        if (written < 0 ||
            (size_t)written >= sizeof(file->held_spec)) {
            file->held_spec[0] = '\0';
            return 0;
        }

        if (!edit_txn_file_exists(file->held_spec)) {
            return 1;
        }
    }

    file->held_spec[0] = '\0';
    return 0;
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
    file->held_spec[0] = '\0';
    file->existed_before = edit_txn_file_exists(path) ? 1U : 0U;
    file->is_delete = 0U;

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

int edit_txn_add_delete(
    edit_txn *transaction,
    const char *path)
{
    edit_txn_file *file;
    unsigned int index;

    if (transaction == NULL ||
        !transaction->active ||
        transaction->committed ||
        !edit_txn_versioned_path(path)) {
        return 0;
    }

    if (edit_txn_find(transaction, path) >= 0 ||
        transaction->file_count >= EDIT_TXN_MAX_FILES) {
        return 0;
    }

    for (index = 0U; index < transaction->file_count; ++index) {
        if (transaction->files[index].is_delete) {
            return 0;
        }
    }

    file = &transaction->files[transaction->file_count];
    (void)memset(file, 0, sizeof(*file));
    (void)strcpy(file->path, path);
    file->existed_before = 1U;
    file->is_delete = 1U;

    if (!edit_txn_get_spec(
            path,
            file->original_spec,
            sizeof(file->original_spec))) {
        (void)memset(file, 0, sizeof(*file));
        return 0;
    }

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

        if (file->is_delete) {
            if (!edit_txn_make_hold(file) ||
                rename(file->original_spec, file->held_spec) != 0) {
                (void)edit_txn_rollback(transaction);
                return 0;
            }

            file->written = 1U;
            continue;
        }

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
    unsigned int index;

    if (transaction == NULL ||
        !transaction->active ||
        transaction->committed) {
        return 0;
    }

    for (index = 0U; index < transaction->file_count; ++index) {
        edit_txn_file *file;

        file = &transaction->files[index];
        if (!file->is_delete || !file->written) {
            continue;
        }

        if (file->held_spec[0] == '\0' ||
            remove(file->held_spec) != 0) {
            return 0;
        }

        file->written = 0U;
        file->held_spec[0] = '\0';
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

        if (file->is_delete) {
            if (file->held_spec[0] == '\0' ||
                rename(file->held_spec, file->original_spec) != 0) {
                success = 0;
            } else {
                file->held_spec[0] = '\0';
                file->written = 0U;
            }
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
