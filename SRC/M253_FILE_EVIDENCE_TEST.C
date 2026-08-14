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
        (void)printf("M253 seed open failed: %s\n", path);
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
        (void)printf("M253 seed write failed: %s\n", path);
        (void)close(fd);
        return 0;
    }

    if (close(fd) != 0) {
        (void)printf("M253 seed close failed: %s\n", path);
        return 0;
    }

    return 1;
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
        (void)printf(
            "M253 RMS open failed: %s status=%08lX\n",
            path,
            status);
        return 0;
    }

    snapshot->org = (unsigned int)fab.fab$b_org;
    snapshot->rfm = (unsigned int)fab.fab$b_rfm;
    snapshot->rat = (unsigned int)fab.fab$b_rat;
    snapshot->mrs = (unsigned int)fab.fab$w_mrs;

    status = sys$close(&fab);
    if (!(status & 1UL)) {
        (void)printf(
            "M253 RMS close failed: %s status=%08lX\n",
            path,
            status);
        return 0;
    }

    return 1;
}

static int m253_same_rms(
    const char *label,
    const m253_rms_snapshot *left,
    const m253_rms_snapshot *right)
{
    if (left->org == right->org &&
        left->rfm == right->rfm &&
        left->rat == right->rat &&
        left->mrs == right->mrs) {
        return 1;
    }

    (void)printf(
        "M253 %s RMS mismatch: before org=%u rfm=%u rat=%u mrs=%u; after org=%u rfm=%u rat=%u mrs=%u\n",
        label,
        left->org,
        left->rfm,
        left->rat,
        left->mrs,
        right->org,
        right->rfm,
        right->rat,
        right->mrs);
    return 0;
}

static int m253_read_exact(const char *path, const char *expected)
{
    FILE *file;
    char buffer[256];
    char extra[8];

    file = fopen(path, "r", "ctx=stm");
    if (file == NULL) {
        (void)printf("M253 read open failed: %s\n", path);
        return 0;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        (void)printf("M253 read first record failed: %s\n", path);
        (void)fclose(file);
        return 0;
    }

    if (fgets(extra, sizeof(extra), file) != NULL) {
        (void)printf(
            "M253 read found unexpected extra record: %s first=[%s] extra=[%s]\n",
            path,
            buffer,
            extra);
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        (void)printf("M253 read close failed: %s\n", path);
        return 0;
    }

    if (strcmp(buffer, expected) != 0) {
        (void)printf(
            "M253 content mismatch: %s expected=[%s] actual=[%s]\n",
            path,
            expected,
            buffer);
        return 0;
    }

    return 1;
}

static int m253_absent(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    (void)printf("M253 expected absent but found: %s\n", path);
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

    result = 0;
    m253_cleanup(base);

    if (!m253_seed(base, "M253 DELETE\n")) {
        (void)puts("M253 delete checkpoint: seed failed.");
        return 0;
    }
    if (!m253_snapshot(versioned, &before)) {
        (void)puts("M253 delete checkpoint: initial snapshot failed.");
        m253_cleanup(base);
        return 0;
    }

    edit_txn_init(&txn);
    if (!edit_txn_add_delete(&txn, versioned)) {
        (void)puts("M253 delete checkpoint: add failed.");
        goto done;
    }
    if (!edit_txn_write(&txn)) {
        (void)puts("M253 delete checkpoint: write failed.");
        goto done;
    }
    if (!m253_absent(versioned)) {
        (void)puts("M253 delete checkpoint: source still present after write.");
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 delete checkpoint: sentinel changed after write.");
        goto done;
    }
    if (!edit_txn_rollback(&txn)) {
        (void)puts("M253 delete checkpoint: rollback failed.");
        goto done;
    }
    if (!m253_snapshot(versioned, &after)) {
        (void)puts("M253 delete checkpoint: restored snapshot failed.");
        goto done;
    }
    if (!m253_same_rms("delete rollback", &before, &after)) {
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 delete checkpoint: sentinel changed after rollback.");
        goto done;
    }

    result = 1;

done:
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

    result = 0;
    m253_cleanup(src_base);
    m253_cleanup(dst_base);

    if (!m253_seed(src_base, "M253 RENAME\n")) {
        (void)puts("M253 rename checkpoint: seed failed.");
        return 0;
    }
    if (!m253_snapshot(src, &before)) {
        (void)puts("M253 rename checkpoint: initial snapshot failed.");
        m253_cleanup(src_base);
        return 0;
    }

    edit_txn_init(&txn);
    if (!edit_txn_add_rename(&txn, src, dst)) {
        (void)puts("M253 rename checkpoint: add failed.");
        goto done;
    }
    if (!edit_txn_write(&txn)) {
        (void)puts("M253 rename checkpoint: write failed.");
        goto done;
    }
    if (!m253_snapshot(dst, &moved)) {
        (void)puts("M253 rename checkpoint: destination snapshot failed.");
        goto done;
    }
    if (!m253_same_rms("rename destination", &before, &moved)) {
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 rename checkpoint: sentinel changed after write.");
        goto done;
    }
    if (!edit_txn_rollback(&txn)) {
        (void)puts("M253 rename checkpoint: rollback failed.");
        goto done;
    }
    if (!m253_snapshot(src, &restored)) {
        (void)puts("M253 rename checkpoint: restored snapshot failed.");
        goto done;
    }
    if (!m253_same_rms("rename rollback", &before, &restored)) {
        goto done;
    }
    if (!m253_absent(dst)) {
        (void)puts("M253 rename checkpoint: destination remains after rollback.");
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 rename checkpoint: sentinel changed after rollback.");
        goto done;
    }

    result = 1;

done:
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

    result = 0;
    m253_cleanup(src_base);
    m253_cleanup(dst);

    if (!m253_seed(src_base, "M253 MOVE\n")) {
        (void)puts("M253 move checkpoint: seed failed.");
        return 0;
    }
    if (!m253_snapshot(src, &before)) {
        (void)puts("M253 move checkpoint: initial snapshot failed.");
        m253_cleanup(src_base);
        return 0;
    }

    edit_txn_init(&txn);
    if (!edit_txn_add_move(&txn, src, dst)) {
        (void)puts("M253 move checkpoint: add failed.");
        goto done;
    }
    if (!edit_txn_write(&txn)) {
        (void)puts("M253 move checkpoint: write failed.");
        goto done;
    }
    if (!m253_snapshot(dst, &moved)) {
        (void)puts("M253 move checkpoint: destination snapshot failed.");
        goto done;
    }
    if (!m253_same_rms("move destination", &before, &moved)) {
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 move checkpoint: sentinel changed after write.");
        goto done;
    }
    if (!edit_txn_rollback(&txn)) {
        (void)puts("M253 move checkpoint: rollback failed.");
        goto done;
    }
    if (!m253_snapshot(src, &restored)) {
        (void)puts("M253 move checkpoint: restored snapshot failed.");
        goto done;
    }
    if (!m253_same_rms("move rollback", &before, &restored)) {
        goto done;
    }
    if (!m253_absent(dst)) {
        (void)puts("M253 move checkpoint: destination remains after rollback.");
        goto done;
    }
    if (!m253_sentinel_ok()) {
        (void)puts("M253 move checkpoint: sentinel changed after rollback.");
        goto done;
    }

    result = 1;

done:
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

    if (!m253_sentinel_ok()) {
        (void)puts("M253 sentinel verification failed before operations.");
        m253_cleanup("M253_SENTINEL.DAT");
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
