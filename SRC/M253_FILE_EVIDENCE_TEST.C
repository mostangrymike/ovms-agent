#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unixio.h>
#include <rms.h>
#include <starlet.h>

#include "edit_txn.h"

typedef struct m253_rms_snapshot {
    unsigned int org;
    unsigned int rfm;
    unsigned int rat;
    unsigned int mrs;
} m253_rms_snapshot;

static void m253_cleanup(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m253_seed(const char *path, const char *text)
{
    int fd;
    size_t length;

    m253_cleanup(path);
    fd = creat(path, 0, "rfm=var", "rat=cr");
    if (fd < 0) {
        return 0;
    }

    length = strlen(text);
    if (length > 0U && text[length - 1U] == '\n') {
        --length;
        if (length > 0U && text[length - 1U] == '\r') {
            --length;
        }
    }

    if (write(fd, text, length) != (int)length) {
        (void)close(fd);
        return 0;
    }

    return close(fd) == 0;
}

static int m253_snapshot(
    const char *path,
    m253_rms_snapshot *snapshot)
{
    struct FAB fab = cc$rms_fab;
    unsigned long status;

    if (path == NULL || snapshot == NULL || strlen(path) > 255U) {
        return 0;
    }

    fab.fab$l_fna = (char *)path;
    fab.fab$b_fns = (unsigned char)strlen(path);
    fab.fab$b_fac = FAB$M_GET;

    status = sys$open(&fab);
    if (!(status & 1UL)) {
        return 0;
    }

    snapshot->org = (unsigned int)fab.fab$b_org;
    snapshot->rfm = (unsigned int)fab.fab$b_rfm;
    snapshot->rat = (unsigned int)fab.fab$b_rat;
    snapshot->mrs = (unsigned int)fab.fab$w_mrs;

    status = sys$close(&fab);
    return (status & 1UL) != 0UL;
}

static int m253_same_rms(
    const m253_rms_snapshot *left,
    const m253_rms_snapshot *right)
{
    return left->org == right->org &&
           left->rfm == right->rfm &&
           left->rat == right->rat &&
           left->mrs == right->mrs;
}

static int m253_read_exact(const char *path, const char *expected)
{
    FILE *file;
    char buffer[256];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        return 0;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        (void)fclose(file);
        return 0;
    }

    if (fgets(buffer + strlen(buffer),
              sizeof(buffer) - strlen(buffer),
              file) != NULL) {
        (void)fclose(file);
        return 0;
    }

    (void)fclose(file);
    return strcmp(buffer, expected) == 0;
}

static int m253_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m253_sentinel_ok(void)
{
    return m253_read_exact(
        "M253_SENTINEL.DAT;1",
        "M253 SENTINEL\n");
}

static int m253_delete_evidence(void)
{
    const char *base = "M253_DELETE.DAT";
    const char *versioned = "M253_DELETE.DAT;1";
    m253_rms_snapshot before;
    m253_rms_snapshot after;
    edit_txn txn;
    int result;

    m253_cleanup(base);
    if (!m253_seed(base, "M253 DELETE\n") ||
        !m253_snapshot(versioned, &before)) {
        (void)puts("M253 delete seed/snapshot failed.");
        m253_cleanup(base);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_delete(&txn, versioned) &&
        edit_txn_write(&txn) &&
        m253_absent(versioned) &&
        m253_sentinel_ok() &&
        edit_txn_rollback(&txn) &&
        m253_snapshot(versioned, &after) &&
        m253_same_rms(&before, &after) &&
        m253_sentinel_ok();

    edit_txn_dispose(&txn);
    m253_cleanup(base);

    if (!result) {
        (void)puts("M253 delete RMS/isolation evidence failed.");
    }
    return result;
}

static int m253_rename_evidence(void)
{
    const char *src_base = "M253_RENAME_SRC.DAT";
    const char *src = "M253_RENAME_SRC.DAT;1";
    const char *dst_base = "M253_RENAME_DST.DAT";
    const char *dst = "M253_RENAME_DST.DAT;1";
    m253_rms_snapshot before;
    m253_rms_snapshot moved;
    m253_rms_snapshot restored;
    edit_txn txn;
    int result;

    m253_cleanup(src_base);
    m253_cleanup(dst_base);
    if (!m253_seed(src_base, "M253 RENAME\n") ||
        !m253_snapshot(src, &before)) {
        (void)puts("M253 rename seed/snapshot failed.");
        m253_cleanup(src_base);
        m253_cleanup(dst_base);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_rename(&txn, src, dst) &&
        edit_txn_write(&txn) &&
        m253_snapshot(dst, &moved) &&
        m253_same_rms(&before, &moved) &&
        m253_sentinel_ok() &&
        edit_txn_rollback(&txn) &&
        m253_snapshot(src, &restored) &&
        m253_same_rms(&before, &restored) &&
        m253_absent(dst) &&
        m253_sentinel_ok();

    edit_txn_dispose(&txn);
    m253_cleanup(src_base);
    m253_cleanup(dst_base);

    if (!result) {
        (void)puts("M253 rename RMS/isolation evidence failed.");
    }
    return result;
}

static int m253_move_evidence(void)
{
    const char *src_base = "M253_MOVE_SRC.DAT";
    const char *src = "M253_MOVE_SRC.DAT;1";
    const char *dst = "[.TEST]M253_MOVE_DST.DAT;1";
    m253_rms_snapshot before;
    m253_rms_snapshot moved;
    m253_rms_snapshot restored;
    edit_txn txn;
    int result;

    m253_cleanup(src_base);
    m253_cleanup(dst);
    if (!m253_seed(src_base, "M253 MOVE\n") ||
        !m253_snapshot(src, &before)) {
        (void)puts("M253 move seed/snapshot failed.");
        m253_cleanup(src_base);
        m253_cleanup(dst);
        return 0;
    }

    edit_txn_init(&txn);
    result =
        edit_txn_add_move(&txn, src, dst) &&
        edit_txn_write(&txn) &&
        m253_snapshot(dst, &moved) &&
        m253_same_rms(&before, &moved) &&
        m253_sentinel_ok() &&
        edit_txn_rollback(&txn) &&
        m253_snapshot(src, &restored) &&
        m253_same_rms(&before, &restored) &&
        m253_absent(dst) &&
        m253_sentinel_ok();

    edit_txn_dispose(&txn);
    m253_cleanup(src_base);
    m253_cleanup(dst);

    if (!result) {
        (void)puts("M253 move RMS/isolation evidence failed.");
    }
    return result;
}

int main(void)
{
    int delete_ok;
    int rename_ok;
    int move_ok;
    int result;

    m253_cleanup("M253_SENTINEL.DAT");
    if (!m253_seed("M253_SENTINEL.DAT", "M253 SENTINEL\n")) {
        (void)puts("M253 could not create unrelated-file sentinel.");
        return EXIT_FAILURE;
    }

    delete_ok = m253_delete_evidence();
    rename_ok = m253_rename_evidence();
    move_ok = m253_move_evidence();
    result = delete_ok && rename_ok && move_ok && m253_sentinel_ok();

    m253_cleanup("M253_SENTINEL.DAT");

    if (!result) {
        (void)puts("M253 file-operation RMS/isolation evidence failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M253 file-operation RMS/isolation evidence passed.");
    return EXIT_SUCCESS;
}
