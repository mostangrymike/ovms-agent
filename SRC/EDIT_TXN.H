#ifndef OVMS_AGENT_EDIT_TXN_H
#define OVMS_AGENT_EDIT_TXN_H

#include <stddef.h>

#define EDIT_TXN_MAX_FILES 32U
#define EDIT_TXN_PATH_SIZE 512U

typedef struct edit_txn_file {
    char path[EDIT_TXN_PATH_SIZE];
    char *replacement_text;
    char *original_text;
    char original_spec[EDIT_TXN_PATH_SIZE];
    char held_spec[EDIT_TXN_PATH_SIZE];
    unsigned int existed_before;
    unsigned int written;
    unsigned int is_delete;
} edit_txn_file;

typedef struct edit_txn {
    edit_txn_file files[EDIT_TXN_MAX_FILES];
    unsigned int file_count;
    unsigned int active;
    unsigned int committed;
} edit_txn;

void edit_txn_init(edit_txn *transaction);

int edit_txn_add(
    edit_txn *transaction,
    const char *path,
    const char *replacement_text);

int edit_txn_add_delete(
    edit_txn *transaction,
    const char *path);

int edit_txn_write(edit_txn *transaction);

int edit_txn_commit(edit_txn *transaction);

int edit_txn_rollback(edit_txn *transaction);

void edit_txn_dispose(edit_txn *transaction);

#endif /* OVMS_AGENT_EDIT_TXN_H */
