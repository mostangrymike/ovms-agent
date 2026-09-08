#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LLM_PLAN_FILE "M310_ACTIVE.PLAN"
#define LLM_STATE_FILE "M310_STATE.TMP"
#define LLM_PLAN_MAX_FILES 16U
#define LLM_PLAN_PATH_SIZE 256U
#define LLM_EXECUTE_MAX_OPERATIONS 32U
#define LLM_WORKFLOW_NONE 0
#define LLM_WORKFLOW_PLAN 4
#define LLM_WORKFLOW_REVIEW 5
#define LLM_WORKFLOW_EXECUTE 6
#define LLM_ROLLBACK_NONE 0

#define REQUIRE(condition, message) \
    do { \
        if (!(condition)) { \
            (void)fprintf(stderr, "M310 regression failed: %s\n", message); \
            return 0; \
        } \
    } while (0)

typedef struct plan_scope_file {
    char path[LLM_PLAN_PATH_SIZE];
    int expect_missing;
} plan_scope_file;

typedef struct llm_saved_operation {
    int is_create;
    int is_block;
    char path[LLM_PLAN_PATH_SIZE];
    const char *old_text;
    const char *new_text;
} llm_saved_operation;

typedef struct edit_txn_file {
    char path[LLM_PLAN_PATH_SIZE];
    char *replacement_text;
} edit_txn_file;

typedef struct edit_txn {
    edit_txn_file files[16];
    unsigned int file_count;
} edit_txn;

typedef struct agent_state {
    const char *project_root;
    int write_enabled;
    int dcl_enabled;
} agent_state;

int llm_plan_approved = 0;
unsigned long llm_approved_hash = 0UL;
int llm_approval_invalidated = 0;
int llm_last_workflow = LLM_WORKFLOW_NONE;
int llm_last_build_known = 0;
int llm_last_build_status = 0;
int llm_last_rollback = LLM_ROLLBACK_NONE;
int llm_state_loaded = 0;
int llm_state_valid = 0;

static int current_fixture = 1;
static int base_save_fixture = 1;
static int load_valid_fixture = 1;
static int load_count = 0;
static int raw_save_count = 0;
static int raw_saved_workflow = -1;

static char *test_copy(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        (void)memcpy(copy, text, length + 1U);
    }
    return copy;
}

static int plan_path_safe(const char *path)
{
    return path != NULL && *path != '\0' && strstr(path, "..") == NULL;
}

static int plan_scope_find(
    plan_scope_file files[],
    unsigned int count,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(files[index].path, path) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static int plan_line_value(
    const char *start,
    const char *end,
    const char *prefix,
    char *output,
    size_t output_size)
{
    size_t prefix_length;
    size_t length;

    prefix_length = strlen(prefix);
    length = (size_t)(end - start);
    if (length < prefix_length ||
        memcmp(start, prefix, prefix_length) != 0) {
        return 0;
    }
    length -= prefix_length;
    if (length == 0U || length >= output_size) {
        return -1;
    }
    (void)memcpy(output, start + prefix_length, length);
    output[length] = '\0';
    return 1;
}

static int plan_collect_paths(
    const char *text,
    plan_scope_file files[],
    unsigned int *count)
{
    (void)text;
    (void)files;
    if (count != NULL) {
        *count = 0U;
    }
    return 1;
}

#include "LLM_PLAN_M310_SCOPE.INC"
#undef plan_collect_paths

static int execute_target_exists(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static int llm_path_is_safe(const char *path)
{
    return plan_path_safe(path);
}

static int llm_path_is_sensitive(const char *path)
{
    (void)path;
    return 0;
}

static int execute_delete_precheck(
    const llm_saved_operation *ops,
    unsigned int count)
{
    (void)ops;
    (void)count;
    return 1;
}

static int execute_rename_precheck(
    const llm_saved_operation *ops,
    unsigned int count)
{
    (void)ops;
    (void)count;
    return 1;
}

static int execute_move_precheck(
    const llm_saved_operation *ops,
    unsigned int count)
{
    (void)ops;
    (void)count;
    return 1;
}

static void edit_txn_init(edit_txn *transaction)
{
    (void)memset(transaction, 0, sizeof(*transaction));
}

static void edit_txn_dispose(edit_txn *transaction)
{
    unsigned int index;

    for (index = 0U; index < transaction->file_count; ++index) {
        free(transaction->files[index].replacement_text);
        transaction->files[index].replacement_text = NULL;
    }
    transaction->file_count = 0U;
}

static int edit_txn_add(
    edit_txn *transaction,
    const char *path,
    const char *text)
{
    edit_txn_file *entry;

    if (transaction->file_count >= 16U) {
        return 0;
    }
    entry = &transaction->files[transaction->file_count++];
    (void)strcpy(entry->path, path);
    entry->replacement_text = test_copy(text);
    return entry->replacement_text != NULL;
}

static int edit_txn_add_delete(edit_txn *t, const char *p)
{
    (void)t; (void)p; return 0;
}
static int edit_txn_add_rename(edit_txn *t, const char *p, const char *q)
{
    (void)t; (void)p; (void)q; return 0;
}
static int edit_txn_add_move(edit_txn *t, const char *p, const char *q)
{
    (void)t; (void)p; (void)q; return 0;
}

static char *execute_apply_replacement_to_content(
    const char *content,
    const llm_saved_operation *operation)
{
    if (operation == NULL || content == NULL ||
        operation->old_text == NULL || operation->new_text == NULL ||
        strcmp(content, operation->old_text) != 0) {
        return NULL;
    }
    return test_copy(operation->new_text);
}

static char *llm_read_text_file(const char *path)
{
    (void)path;
    return NULL;
}

#include "LLM_EXECUTE_M310_CHAIN.INC"

void llm_log_event(const char *workflow, const char *event, int status);
void llm_plan_recovery_sync(const char *path);
int llm_plan_is_current(int verbose);
const char *llm_approval_name(void);
void llm_plan_approval_clear(void);
int execute_parse_operations(
    const char *path,
    llm_saved_operation *operations,
    unsigned int capacity,
    unsigned int *count);
int llm_plan_approval_valid(const char *path);
void llm_plan_approval_consume(void);
int edit_txn_write(edit_txn *transaction);
int edit_txn_rollback(edit_txn *transaction);
int edit_txn_commit(edit_txn *transaction);
void llm_state_save(void);

#include "LLM_EXECUTE_M310_POLICY.INC"
#undef execute_stage_operations_chained
#undef llm_plan_execute

static int execute_plan_recover(void) { return 1; }
static int execute_mark_consumed_safe(void) { return 1; }
void llm_m310_execute_legacy(agent_state *state) { (void)state; }
void llm_log_event(const char *w, const char *e, int s)
{ (void)w; (void)e; (void)s; }
void llm_plan_recovery_sync(const char *path) { (void)path; }
const char *llm_approval_name(void) { return "workspace"; }
void llm_plan_approval_clear(void)
{
    llm_plan_approved = 0;
    llm_approved_hash = 0UL;
}
int execute_parse_operations(
    const char *p, llm_saved_operation *o,
    unsigned int c, unsigned int *n)
{ (void)p; (void)o; (void)c; if (n) *n = 0U; return 0; }
int llm_plan_approval_valid(const char *path) { (void)path; return 0; }
void llm_plan_approval_consume(void) { }
int edit_txn_write(edit_txn *t) { (void)t; return 0; }
int edit_txn_rollback(edit_txn *t) { (void)t; return 1; }
int edit_txn_commit(edit_txn *t) { (void)t; return 1; }

static int llm_plan_file_current(
    const char *path,
    int verbose)
{
    (void)path;
    (void)verbose;
    return current_fixture;
}

#include "LLM_PLAN_CURRENT_WRAPPER.INC"

static int test_base_plan_save(const char *goal, const char *plan)
{
    (void)goal;
    (void)plan;
    return base_save_fixture;
}
#define llm_plan_save test_base_plan_save
#include "LLM_PLAN_M310.H"
#undef llm_plan_save

void llm_load_state(void)
{
    ++load_count;
    llm_state_loaded = 1;
    llm_state_valid = load_valid_fixture;
    if (load_valid_fixture) {
        llm_last_workflow = LLM_WORKFLOW_EXECUTE;
        llm_last_build_known = 1;
        llm_last_build_status = 77;
        llm_last_rollback = LLM_ROLLBACK_NONE;
    }
}

#include "LLM_STATE_M310_SAVE.INC"
#undef llm_state_save

void llm_m310_state_save_raw(void)
{
    ++raw_save_count;
    raw_saved_workflow = llm_last_workflow;
}

#include "LLM_PROMPT_PLAN_M90.INC"
#include "LLM_M150C_VALIDATE.INC"
#undef prompt_plan

static int write_text(const char *path, const char *text)
{
    FILE *file;
    int ok;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }
    ok = fputs(text, file) != EOF;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int test_scope_and_chain(void)
{
    const char *a = "M310_A.TMP";
    const char *b = "M310_B.TMP";
    const char *plan =
        "operation_count=3\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M310_A.TMP\n"
        "BEGIN_NEW_TEXT\n"
        "stage1\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M310_B.TMP\n"
        "BEGIN_NEW_TEXT\n"
        "b1\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=M310_A.TMP\n"
        "BEGIN_OLD_TEXT\n"
        "stage1\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "stage2\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n";
    const char *duplicate =
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "type=create_file\npath=M310_A.TMP\n"
        "BEGIN_NEW_TEXT\none\nEND_NEW_TEXT\nEND_OPERATION\n"
        "BEGIN_OPERATION\n"
        "type=create_file\npath=M310_A.TMP\n"
        "BEGIN_NEW_TEXT\ntwo\nEND_NEW_TEXT\nEND_OPERATION\n";
    plan_scope_file files[LLM_PLAN_MAX_FILES];
    unsigned int count;
    llm_saved_operation operations[3];
    edit_txn transaction;

    (void)remove(a);
    (void)remove(b);
    count = 0U;
    REQUIRE(
        llm_m310_collect_paths(plan, files, &count),
        "create-modify scope rejected");
    REQUIRE(count == 2U, "create-modify scope did not collapse path");
    REQUIRE(files[0].expect_missing == 1, "created path lost missing fingerprint");
    count = 0U;
    REQUIRE(
        !llm_m310_collect_paths(duplicate, files, &count),
        "duplicate create was accepted");

    (void)memset(operations, 0, sizeof(operations));
    operations[0].is_create = 1;
    (void)strcpy(operations[0].path, a);
    operations[0].new_text = "stage1";
    operations[1].is_create = 1;
    (void)strcpy(operations[1].path, b);
    operations[1].new_text = "b1";
    operations[2].is_block = 1;
    (void)strcpy(operations[2].path, a);
    operations[2].old_text = "stage1";
    operations[2].new_text = "stage2";

    edit_txn_init(&transaction);
    REQUIRE(
        llm_m310_stage_operations_chained(&transaction, operations, 3U),
        "create-modify transaction staging failed");
    REQUIRE(transaction.file_count == 2U, "transaction did not collapse created path");
    REQUIRE(
        strcmp(transaction.files[0].replacement_text, "stage2") == 0,
        "created path final content is not stage2");
    REQUIRE(
        strcmp(transaction.files[1].replacement_text, "b1") == 0,
        "unrelated created path changed");
    edit_txn_dispose(&transaction);
    return 1;
}

static int test_build_policy(void)
{
    const char *path = "M310_BUILD.PLAN";

    REQUIRE(write_text(path, "operation_count=1\n"), "write legacy build fixture");
    REQUIRE(llm_m310_build_policy(path) == 1, "legacy plan did not default build required");
    REQUIRE(write_text(path, "build=required\noperation_count=1\n"), "write required fixture");
    REQUIRE(llm_m310_build_policy(path) == 1, "required policy rejected");
    REQUIRE(write_text(path, "build=skip\noperation_count=1\n"), "write skip fixture");
    REQUIRE(llm_m310_build_policy(path) == 0, "skip policy rejected");
    REQUIRE(write_text(path, "build=skip\nbuild=required\noperation_count=1\n"), "write duplicate fixture");
    REQUIRE(llm_m310_build_policy(path) < 0, "duplicate build policy accepted");
    REQUIRE(write_text(path, "build=maybe\noperation_count=1\n"), "write invalid fixture");
    REQUIRE(llm_m310_build_policy(path) < 0, "invalid build policy accepted");
    (void)remove(path);
    return 1;
}

static int test_approval_lifecycle(void)
{
    llm_plan_approved = 1;
    llm_approved_hash = 123UL;
    llm_approval_invalidated = 0;
    current_fixture = 0;
    REQUIRE(!llm_plan_is_current(0), "stale fixture reported current");
    REQUIRE(!llm_plan_approved, "stale plan kept approval");
    REQUIRE(llm_approval_invalidated, "stale plan did not mark approval invalidated");
    current_fixture = 1;
    REQUIRE(llm_plan_is_current(0), "restored fixture not current");
    REQUIRE(!llm_plan_approved, "approval resurrected after freshness restored");

    llm_plan_approved = 1;
    llm_approved_hash = 456UL;
    llm_approval_invalidated = 0;
    base_save_fixture = 0;
    REQUIRE(!llm_m310_plan_save("goal", "plan"), "failed replacement reported success");
    REQUIRE(!llm_plan_approved, "failed replacement kept prior approval");
    REQUIRE(llm_approval_invalidated, "failed replacement did not invalidate authority");
    base_save_fixture = 1;
    return 1;
}

static void reset_state_defaults(void)
{
    llm_last_workflow = LLM_WORKFLOW_NONE;
    llm_last_build_known = 0;
    llm_last_build_status = 0;
    llm_last_rollback = LLM_ROLLBACK_NONE;
    llm_state_loaded = 0;
    llm_state_valid = 0;
    load_count = 0;
    raw_save_count = 0;
    raw_saved_workflow = -1;
}

static int test_state_preservation(void)
{
    reset_state_defaults();
    load_valid_fixture = 1;
    REQUIRE(write_text(LLM_STATE_FILE, "prior\n"), "write prior state fixture");
    llm_state_save();
    REQUIRE(load_count == 1, "existing state was not loaded before save");
    REQUIRE(raw_save_count == 1, "valid prior state was not saved after load");
    REQUIRE(raw_saved_workflow == LLM_WORKFLOW_EXECUTE,
            "process defaults overwrote prior workflow");

    reset_state_defaults();
    load_valid_fixture = 0;
    llm_state_save();
    REQUIRE(load_count == 1, "invalid state load was not attempted");
    REQUIRE(raw_save_count == 0, "invalid state was overwritten by defaults");

    (void)remove(LLM_STATE_FILE);
    reset_state_defaults();
    load_valid_fixture = 1;
    llm_state_save();
    REQUIRE(load_count == 0, "missing state triggered a load");
    REQUIRE(raw_save_count == 1, "new default state was not saved");

    REQUIRE(write_text(LLM_STATE_FILE, "prior\n"), "rewrite prior state fixture");
    reset_state_defaults();
    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_state_save();
    REQUIRE(load_count == 0, "intentional current workflow was replaced by prior state");
    REQUIRE(raw_saved_workflow == LLM_WORKFLOW_REVIEW,
            "intentional current workflow was not saved");
    (void)remove(LLM_STATE_FILE);
    return 1;
}

static int test_prompt(void)
{
    REQUIRE(llm_m150c_has_create(), "base create prompt regression failed");
    REQUIRE(llm_m150c_keeps_replace(), "base replace prompt regression failed");
    REQUIRE(llm_m310_prompt_create_chain(), "M310 create-chain prompt missing");
    REQUIRE(llm_m310_prompt_build_policy(), "M310 build-policy prompt missing");
    return 1;
}

int main(void)
{
    if (!test_scope_and_chain() ||
        !test_build_policy() ||
        !test_approval_lifecycle() ||
        !test_state_preservation() ||
        !test_prompt()) {
        return 2;
    }

    (void)puts("M310 saved-plan lifecycle/execution regression passed.");
    return 0;
}
