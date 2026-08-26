/*
 * MAIN.C - Phase 1 inventory CLI
 * DEC C / OpenVMS C89 compatible
 * Minimal command-line interface to ingest a device config
 * from a local text file and list or show stored inventory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "storage.h"
#include "diff.h"
#include "rules.h"

#define RC_OK      0
#define RC_ERR     1
#define RC_USAGE   2

static void print_usage(void);
static int do_add(INV_STORE *st, const char *dev, const char *addr, const char *cfg);
static int do_list(INV_STORE *st);
static int do_show(INV_STORE *st, const char *dev);
static void print_item_line(const INV_ITEM *it);
static void print_item_detail(const INV_ITEM *it);
static int do_diff(INV_STORE *st, const char *dev, const char *cand_cfg_path, const char *diff_out_path, unsigned int context_lines);
static int do_check(INV_STORE *st, const char *dev, const char *rules_path, const char *cand_cfg_path);

int main(int argc, char *argv[])
{
    const char *store_path;
    const char *cmd;
    INV_STORE *st;
    int rc;

    if (argc < 3) {
        print_usage();
        return RC_USAGE;
    }

    store_path = argv[1];
    cmd = argv[2];

    st = inv_open(store_path, 0);
    if (st == NULL) {
        fprintf(stderr, "Error: cannot open store '%s'\n", store_path);
        return RC_ERR;
    }

    rc = RC_OK;

    if (strcmp(cmd, "add") == 0) {
        const char *dev;
        const char *addr;
        const char *cfg;
        if (argc < 6) {
            print_usage();
            rc = RC_USAGE;
        } else {
            dev = argv[3];
            addr = argv[4];
            cfg = argv[5];
            if (addr != NULL && strcmp(addr, "-") == 0) {
                addr = NULL; /* optional */
            }
            rc = do_add(st, dev, addr, cfg);
        }
    } else if (strcmp(cmd, "list") == 0) {
        rc = do_list(st);
    } else if (strcmp(cmd, "show") == 0) {
        const char *dev;
        if (argc < 4) {
            print_usage();
            rc = RC_USAGE;
        } else {
            dev = argv[3];
            rc = do_show(st, dev);
        }
    } else if (strcmp(cmd, "diff") == 0) {
        const char *dev;
        const char *cfg;
        const char *diff_out;
        if (argc < 5) {
            print_usage();
            rc = RC_USAGE;
        } else {
            dev = argv[3];
            cfg = argv[4];
            diff_out = (argc >= 6) ? argv[5] : NULL;
            rc = do_diff(st, dev, cfg, diff_out, 3U);
        }
    } else if (strcmp(cmd, "check") == 0) {
        const char *dev;
        const char *rules_path;
        const char *cfg_path;
        if (argc < 4 || argc > 6) {
            print_usage();
            rc = RC_USAGE;
        } else {
            dev = argv[3];
            rules_path = (argc >= 5) ? argv[4] : NULL;
            cfg_path = (argc >= 6) ? argv[5] : NULL;
            rc = do_check(st, dev, rules_path, cfg_path);
        }
    } else {
        print_usage();
        rc = RC_USAGE;
    }    inv_close(st);
    return rc;
}

static void print_usage(void)
{
    printf("Usage:\n");
    printf("  invcli STORE_PATH add DEVICE_ID MGMT_ADDR CFG_FILE\n");
    printf("    Ingest config from local CFG_FILE. Use '-' for MGMT_ADDR if none.\n");
    printf("  invcli STORE_PATH list\n");
    printf("    List all inventory items.\n");
    printf("  invcli STORE_PATH show DEVICE_ID\n");
    printf("    Show one item's stored metadata.\n");
    printf("  invcli STORE_PATH diff DEVICE_ID CFG_FILE [DIFF_OUT]\n");
    printf("    Show differences between stored config and CFG_FILE. Write to DIFF_OUT if given.\n");
    printf("  invcli STORE_PATH check DEVICE_ID [RULES_FILE] [CFG_FILE]\n");
    printf("    Check config for DEVICE_ID using optional RULES_FILE and CFG_FILE.\n");
}

static int do_add(INV_STORE *st, const char *dev, const char *addr, const char *cfg)
{
    int r;
    if (st == NULL || dev == NULL || cfg == NULL) {
        fprintf(stderr, "Error: invalid parameters to add\n");
        return RC_ERR;
    }
    r = inv_add_file(st, dev, addr, cfg);
    if (r != INV_OK) {
        fprintf(stderr, "Error: add failed (%d)\n", r);
        return RC_ERR;
    }
    r = inv_sync(st);
    if (r != INV_OK) {
        fprintf(stderr, "Warning: sync failed (%d)\n", r);
        /* Still consider add mostly successful */
    }
    printf("Added: %s\n", dev);
    return RC_OK;
}

static int do_list(INV_STORE *st)
{
    unsigned long n;
    unsigned long i;
    INV_ITEM it;
    int r;

    if (st == NULL) {
        return RC_ERR;
    }

    n = inv_count(st);
    for (i = 0; i < n; ++i) {
        r = inv_get_by_index(st, i, &it);
        if (r == INV_OK) {
            print_item_line(&it);
        } else {
            fprintf(stderr, "Error: read item %lu failed (%d)\n", i, r);
            return RC_ERR;
        }
    }
    return RC_OK;
}

static int do_show(INV_STORE *st, const char *dev)
{
    INV_ITEM it;
    int r;

    if (st == NULL || dev == NULL) {
        return RC_ERR;
    }

    r = inv_get_by_id(st, dev, &it);
    if (r != INV_OK) {
        fprintf(stderr, "Error: device not found: %s\n", dev);
        return RC_ERR;
    }

    print_item_detail(&it);
    return RC_OK;
}

static void print_item_line(const INV_ITEM *it)
{
    if (it == NULL) return;
    printf("%s\t%s\t%lu\t%lu\n",
           it->device_id,
           it->mgmt_addr,
           (unsigned long)it->cfg_size,
           (unsigned long)it->updated_time);
}

static void print_item_detail(const INV_ITEM *it)
{
    const char *tstr;
    time_t tt;
    if (it == NULL) return;
    printf("Device: %s\n", it->device_id);
    printf("Address: %s\n", it->mgmt_addr);
    printf("Size: %lu bytes\n", (unsigned long)it->cfg_size);
    printf("Updated: %lu\n", (unsigned long)it->updated_time);
    tt = (time_t)it->updated_time;
    tstr = ctime(&tt);
    if (tstr != NULL) {
        size_t len = strlen(tstr);
        if (len > 0 && tstr[len - 1] == '\n') {
            /* print without trailing newline */
            char buf[128];
            size_t i;
            if (len >= sizeof(buf)) len = sizeof(buf) - 1;
            for (i = 0; i + 1 < len; ++i) buf[i] = tstr[i];
            buf[len - 1] = '\0';
            printf("UpdatedText: %s\n", buf);
        } else {
            printf("UpdatedText: %s\n", tstr);
        }
    }
    printf("ConfigPath: %s\n", it->cfg_path);
}

static int do_diff(INV_STORE *st, const char *dev, const char *cand_cfg_path, const char *diff_out_path, unsigned int context_lines)
{
    INV_ITEM it;
    const char *old_path;
    CFGDIFF_SUMMARY sum;
    int r;

    if (st == NULL || dev == NULL || cand_cfg_path == NULL) {
        fprintf(stderr, "Error: invalid parameters to diff\n");
        return RC_ERR;
    }

    r = inv_get_by_id(st, dev, &it);
    if (r != INV_OK) {
        fprintf(stderr, "Error: device not found: %s\n", dev);
        return RC_ERR;
    }

    old_path = (it.cfg_path[0] != '\0') ? it.cfg_path : NULL;

    r = cfgdiff_compare_paths(old_path, cand_cfg_path, diff_out_path, context_lines, &sum);
    if (r != CDIFF_OK) {
        fprintf(stderr, "Error: diff failed (%d)\n", r);
        return RC_ERR;
    }

    printf("Added: %lu\n", (unsigned long)sum.added);
    printf("Removed: %lu\n", (unsigned long)sum.removed);
    printf("Changed: %lu\n", (unsigned long)sum.changed);
    if (diff_out_path != NULL) {
        printf("Diff written to: %s\n", diff_out_path);
    }

    return RC_OK;
}


static int do_check(INV_STORE *st, const char *dev,
                    const char *rules_path,
                    const char *cand_cfg_path)
{
    INV_ITEM it;
    const char *cfg_path;
    struct rule_ctx ctx;
    static struct rule_result results[RULES_MAX_RESULTS];
    unsigned int resn;
    unsigned int i;
    unsigned int warn_count;
    unsigned int error_count;
    int r;
    int rules_trunc;

    cfg_path = NULL;
    warn_count = 0U;
    error_count = 0U;
    rules_trunc = 0;

    if (st == NULL || dev == NULL) {
        fprintf(stderr, "Error: invalid parameters to check\n");
        return RC_ERR;
    }

    r = inv_get_by_id(st, dev, &it);
    if (r != INV_OK) {
        fprintf(stderr, "Error: device not found: %s\n", dev);
        return RC_ERR;
    }

    if (cand_cfg_path != NULL) {
        if (cand_cfg_path[0] != '\0') {
            cfg_path = cand_cfg_path;
        }
    }

    if (cfg_path == NULL) {
        if (it.cfg_path[0] != '\0') {
            cfg_path = it.cfg_path;
        }
    }

    if (cfg_path == NULL) {
        fprintf(stderr, "Error: no configuration path supplied\n");
        return RC_ERR;
    }

    r = rules_ctx_init_def(&ctx);
    if (r == RULES_TRUNC) {
        rules_trunc = 1;
    } else if (r != RULES_OK) {
        fprintf(stderr, "Error: rules init failed (%d)\n", r);
        return RC_ERR;
    }

    if (rules_path != NULL) {
        if (rules_path[0] != '\0') {
            r = rules_load_text(&ctx, rules_path, 1);
            if (r == RULES_TRUNC) {
                rules_trunc = 1;
            } else if (r != RULES_OK) {
                fprintf(stderr, "Error: rules load failed (%d)\n", r);
                rules_ctx_free(&ctx);
                return RC_ERR;
            }
        }
    }

    resn = (unsigned int)RULES_MAX_RESULTS;
    r = rules_check_config(&ctx, cfg_path, results, &resn);
    if (r == RULES_TRUNC) {
        rules_trunc = 1;
    } else if (r != RULES_OK) {
        fprintf(stderr, "Error: rules check failed (%d)\n", r);
        rules_ctx_free(&ctx);
        return RC_ERR;
    }

    for (i = 0U; i < resn; ++i) {
        if (results[i].sev == RULE_SEV_WARN) {
            ++warn_count;
        } else if (results[i].sev == RULE_SEV_ERROR) {
            ++error_count;
        }

        printf("Rule %u type %u sev %u line %lu col %lu match %s\n",
               (unsigned int)results[i].rule_index,
               (unsigned int)results[i].type,
               (unsigned int)results[i].sev,
               (unsigned long)results[i].line_no,
               (unsigned long)results[i].col_no,
               results[i].match);
    }

    printf("Warnings: %u\n", warn_count);
printf("Errors: %u\n", error_count);

    report_risk_score(error_count, warn_count);
    if (rules_trunc) {
        printf("Warning: some rule data was truncated\n");
    }

    rules_ctx_free(&ctx);

    if (error_count > 0U) {
        return RC_ERR;
    }

    return RC_OK;
}
