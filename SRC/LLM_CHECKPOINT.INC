#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_PLAN.H"

#define M256_CKPT_FILE "OVMS_AGENT_CHECKPOINTS.DAT"
#define M256_PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define M256_CHECK_FILE "OVMS_AGENT_PLAN.TXT.CHK"
#define M256_LINE_MAX 256U

typedef struct m256_checkpoint {
    char session[9];
    unsigned long plan_hash;
    unsigned int total;
    unsigned int completed;
} m256_checkpoint;

static int m256_id_ok(const char *id)
{
    unsigned int count;
    unsigned char ch;

    if (id == NULL) return 0;
    count = 0U;
    while (*id != '\0') {
        ch = (unsigned char)*id++;
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f'))) return 0;
        ++count;
    }
    return count == 8U;
}

static int m256_plan_hash(unsigned long *value)
{
    FILE *file;
    char line[64];
    unsigned long hash;
    char extra;

    if (value == NULL) return 0;
    file = fopen(M256_CHECK_FILE, "r");
    if (file == NULL) return 0;
    if (fgets(line, sizeof(line), file) == NULL ||
        fclose(file) != 0 ||
        sscanf(line, "%lx %c", &hash, &extra) != 1) return 0;
    *value = hash & 0xffffffffUL;
    return 1;
}

static int m256_plan_steps(unsigned int *total)
{
    FILE *file;
    char line[M256_LINE_MAX];
    unsigned int count;
    int saw_active;

    if (total == NULL) return 0;
    file = fopen(M256_PLAN_FILE, "r");
    if (file == NULL) return 0;
    count = 0U;
    saw_active = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "status=active", 13U) == 0) saw_active = 1;
        if (sscanf(line, "operation_count=%u", &count) == 1) break;
    }
    (void)fclose(file);
    if (!saw_active || count == 0U || count > 32U) return 0;
    *total = count;
    return 1;
}

static int m256_ckpt_load(const char *session, m256_checkpoint *record)
{
    FILE *file;
    char line[M256_LINE_MAX];
    char id[9];
    unsigned long hash;
    unsigned int total;
    unsigned int completed;

    if (!m256_id_ok(session) || record == NULL) return 0;
    file = fopen(M256_CKPT_FILE, "r");
    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%8[^|]|%lx|%u|%u",
                   id, &hash, &total, &completed) == 4 &&
            strcmp(id, session) == 0) {
            (void)fclose(file);
            (void)strcpy(record->session, id);
            record->plan_hash = hash & 0xffffffffUL;
            record->total = total;
            record->completed = completed;
            return 1;
        }
    }
    (void)fclose(file);
    return 0;
}

static int m256_ckpt_save(const m256_checkpoint *record)
{
    FILE *input;
    FILE *output;
    char line[M256_LINE_MAX];
    char id[9];
    const char *temp;
    int replaced;

    if (record == NULL || !m256_id_ok(record->session)) return 0;
    temp = "OVMS_AGENT_CHECKPOINTS.NEW";
    while (remove(temp) == 0) { }
    output = fopen(temp, "w");
    if (output == NULL) return 0;
    replaced = 0;
    input = fopen(M256_CKPT_FILE, "r");
    if (input != NULL) {
        while (fgets(line, sizeof(line), input) != NULL) {
            id[0] = '\0';
            (void)sscanf(line, "%8[^|]", id);
            if (strcmp(id, record->session) == 0) {
                if (fprintf(output, "%s|%08lX|%u|%u\n",
                            record->session, record->plan_hash,
                            record->total, record->completed) < 0) {
                    (void)fclose(input); (void)fclose(output);
                    (void)remove(temp); return 0;
                }
                replaced = 1;
            } else if (fputs(line, output) == EOF) {
                (void)fclose(input); (void)fclose(output);
                (void)remove(temp); return 0;
            }
        }
        (void)fclose(input);
    }
    if (!replaced && fprintf(output, "%s|%08lX|%u|%u\n",
                             record->session, record->plan_hash,
                             record->total, record->completed) < 0) {
        (void)fclose(output); (void)remove(temp); return 0;
    }
    if (fclose(output) != 0) { (void)remove(temp); return 0; }
    while (remove(M256_CKPT_FILE) == 0) { }
    if (rename(temp, M256_CKPT_FILE) != 0) {
        (void)remove(temp); return 0;
    }
    return 1;
}

int openai_checkpoint_resume(const char *session,
                             int may_bind,
                             char *summary,
                             size_t summary_size)
{
    m256_checkpoint record;
    unsigned long hash;
    unsigned int total;
    int have_record;
    int written;

    if (!m256_id_ok(session)) return 0;
    have_record = m256_ckpt_load(session, &record);

    if (!have_record) {
        if (!may_bind) {
            if (summary != NULL && summary_size > 0U) summary[0] = '\0';
            return 1;
        }
        if (!m256_plan_hash(&hash) ||
            !m256_plan_steps(&total) ||
            !openai_plan_file_current(M256_PLAN_FILE, 0)) {
            if (summary != NULL && summary_size > 0U) summary[0] = '\0';
            return 1;
        }
        (void)strcpy(record.session, session);
        record.plan_hash = hash;
        record.total = total;
        record.completed = 0U;
        if (!m256_ckpt_save(&record)) return 0;
    } else {
        if (!m256_plan_hash(&hash) ||
            hash != record.plan_hash ||
            !openai_plan_file_current(M256_PLAN_FILE, 0)) {
            if (summary != NULL && summary_size > 0U) {
                (void)snprintf(summary, summary_size,
                    "Checkpoint stale: saved plan or planned files changed; regenerate the plan.");
            }
            return -1;
        }
    }

    if (summary != NULL && summary_size > 0U) {
        written = snprintf(summary, summary_size,
            "Checkpoint restored: completed %u of %u, pending %u, approval pending.",
            record.completed, record.total,
            record.total >= record.completed ?
                record.total - record.completed : 0U);
        if (written < 0 || (size_t)written >= summary_size) return 0;
    }
    return 1;
}

void openai_test_ckpt_clear(void)
{
    while (remove(M256_CKPT_FILE) == 0) { }
    while (remove("OVMS_AGENT_CHECKPOINTS.NEW") == 0) { }
}
