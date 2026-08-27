/*
 * STORAGE.H - Phase 1 inventory storage public API
 * DEC C / OpenVMS C89 compatible
 * Minimal declarations for local config ingest and
 * simple persistent inventory storage, list, and show.
 * No diffing, rules, scoring, or reporting.
 */

#ifndef STORAGE_H_INCLUDED
#define STORAGE_H_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

/* Limits for fixed-size fields */
#define INV_MAX_ID_LEN      64
#define INV_MAX_ADDR_LEN    64
#define INV_MAX_PATH_LEN    256

/* Result codes */
#define INV_OK              0
#define INV_ERR_PARAM       1
#define INV_ERR_IO          2
#define INV_ERR_NOTFOUND    3
#define INV_ERR_EXISTS      4
#define INV_ERR_NOMEM       5
#define INV_ERR_STATE       6

/* Opaque store handle */
typedef struct inv_store INV_STORE;

/* Inventory item record (public view) */
typedef struct inv_item {
    char device_id[INV_MAX_ID_LEN];      /* device name or unique id */
    char mgmt_addr[INV_MAX_ADDR_LEN];    /* management address (ip/host) */
    char cfg_path[INV_MAX_PATH_LEN];     /* path to stored config file */
    unsigned long cfg_size;              /* bytes */
    unsigned long updated_time;          /* seconds since epoch */
} INV_ITEM;

/* Open or create a persistent inventory store at path. */
INV_STORE *inv_open(const char *path, int create_new);

/* Close a store and release resources. */
void inv_close(INV_STORE *st);

/* Ensure on-disk persistence of recent changes. */
int inv_sync(INV_STORE *st);

/*
 * Ingest (add or replace) a device config from a local file path.
 * If an item with the same device_id exists, it is replaced.
 */
int inv_add_file(INV_STORE *st,
                 const char *device_id,
                 const char *mgmt_addr,
                 const char *cfg_file_path);

/* Number of items currently in the store. */
unsigned long inv_count(INV_STORE *st);

/* Get item by zero-based index into out (metadata only). */
int inv_get_by_index(INV_STORE *st, unsigned long index, INV_ITEM *out);

/* Find item by device_id into out (metadata only). */
int inv_get_by_id(INV_STORE *st, const char *device_id, INV_ITEM *out);

/* Retrieve the stored config file path for a device_id. */
int inv_get_cfg_path(INV_STORE *st,
                     const char *device_id,
                     char *out_path,
                     unsigned int out_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STORAGE_H_INCLUDED */
