static int llm_m138_write_text(
    const char *path,
    const char *text)
{
    FILE *file;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int llm_m138_body_tamper(
    const char *plan_path)
{
    char *text;
    char *position;
    int result;

    if (!llm_m138_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n"
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=SRC/M138.C\n"
            "old_text=OLD\n"
            "new_text=NEW\n"
            "END_OPERATION\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    text = llm_m138_read_text(plan_path);

    if (text == NULL) {
        return 0;
    }

    position = strstr(text, "new_text=NEW");

    if (position == NULL) {
        free(text);
        return 0;
    }

    (void)memcpy(position, "new_text=BAD", 12U);
    result = llm_m138_write_text(plan_path, text);
    free(text);

    return
        result &&
        !llm_plan_checksum_valid(plan_path);
}

int llm_m138_meta_tamper(
    const char *plan_path)
{
    char *text;
    char *position;
    int result;

    if (!llm_m138_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    text = llm_m138_read_text(plan_path);

    if (text == NULL) {
        return 0;
    }

    position = strstr(text, "status=active");

    if (position == NULL) {
        free(text);
        return 0;
    }

    (void)memcpy(position, "status=altered", 14U);
    result = llm_m138_write_text(plan_path, text);
    free(text);

    return
        result &&
        !llm_plan_checksum_valid(plan_path);
}

int llm_m138_partial_reject(
    const char *plan_path)
{
    return
        llm_m138_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") &&
        !llm_plan_checksum_valid(plan_path);
}

int llm_m138_save_preserves(
    const char *plan_path)
{
    char *before;
    char *after;
    int result;

    before = llm_m138_read_text(plan_path);

    if (before == NULL) {
        return 1;
    }

    result = !llm_plan_save(
        "",
        "INVALID");

    after = llm_m138_read_text(plan_path);

    result =
        result &&
        after != NULL &&
        strcmp(before, after) == 0;

    free(before);
    free(after);
    return result;
}
static int llm_m195_normalized_equals(
    const char *input,
    const char *expected)
{
    char *normalized;
    int result;

    normalized =
        llm_m195_normalize_count(input);

    if (normalized == NULL) {
        return 0;
    }

    result = strcmp(normalized, expected) == 0;
    free(normalized);
    return result;
}

int llm_m195_normalize_one(void)
{
    return llm_m195_normalized_equals(
        "Plan text\n"
        "operation_count=9\n"
        "BEGIN_OPERATION\n"
        "type=replace_text\n"
        "path=SRC/TEST.C\n"
        "old_text=OLD\n"
        "new_text=NEW\n"
        "END_OPERATION\n",
        "Plan text\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_text\n"
        "path=SRC/TEST.C\n"
        "old_text=OLD\n"
        "new_text=NEW\n"
        "END_OPERATION\n");
}

int llm_m195_normalize_two(void)
{
    return llm_m195_normalized_equals(
        "operation_count=0\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n",
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");
}

int llm_m195_normalize_correct(void)
{
    return llm_m195_normalized_equals(
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");
}

int llm_m195_norm_inspect(void)
{
    return llm_m195_normalized_equals(
        "Inspection-only plan.\n"
        "No source changes are required.\n",
        "Inspection-only plan.\n"
        "No source changes are required.\n");
}

int llm_m195_reject_missing(void)
{
    char *normalized;

    normalized = llm_m195_normalize_count(
        "Executable plan\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m195_reject_duplicate(void)
{
    char *normalized;

    normalized = llm_m195_normalize_count(
        "operation_count=1\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m195_reject_markers(void)
{
    char *nested;
    char *unmatched;

    nested = llm_m195_normalize_count(
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n"
        "END_OPERATION\n");

    unmatched = llm_m195_normalize_count(
        "operation_count=1\n"
        "BEGIN_OPERATION\n");

    if (nested != NULL) {
        free(nested);
    }

    if (unmatched != NULL) {
        free(unmatched);
    }

    return nested == NULL &&
           unmatched == NULL;
}

int llm_m195_reject_early_begin(void)
{
    char *normalized;

    normalized = llm_m195_normalize_count(
        "Narrative prose\n"
        "Ordered edits\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n"
        "\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST2.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD2\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW2\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m195_failure_preserves(void)
{
    const char *plan_path =
        "OVMS_AGENT_PLAN.TXT";
    const char *checksum_path =
        "OVMS_AGENT_PLAN.TXT.CHK";
    char *plan_before;
    char *check_before;
    char *plan_after;
    char *check_after;
    int result;

    plan_before =
        llm_m138_read_text(plan_path);
    check_before =
        llm_m138_read_text(checksum_path);

    result = !llm_plan_save(
        "M195 normalization rejection test",
        "operation_count=1\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");

    plan_after =
        llm_m138_read_text(plan_path);
    check_after =
        llm_m138_read_text(checksum_path);

    if (plan_before == NULL) {
        result =
            result &&
            plan_after == NULL;
    } else {
        result =
            result &&
            plan_after != NULL &&
            strcmp(plan_before, plan_after) == 0;
    }

    if (check_before == NULL) {
        result =
            result &&
            check_after == NULL;
    } else {
        result =
            result &&
            check_after != NULL &&
            strcmp(check_before, check_after) == 0;
    }

    free(plan_before);
    free(check_before);
    free(plan_after);
    free(check_after);

    return result;
}

static int llm_m251_append(
    char *buffer,
    size_t capacity,
    size_t *used,
    const char *text)
{
    size_t length;

    if (buffer == NULL || used == NULL || text == NULL) {
        return 0;
    }

    length = strlen(text);

    if (*used + length >= capacity) {
        return 0;
    }

    (void)memcpy(buffer + *used, text, length);
    *used += length;
    buffer[*used] = '\0';
    return 1;
}

static char *llm_m251_big_plan(int old_side)
{
    static const char fill_line[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\n";
    char *plan;
    size_t used;
    int index;

    plan = (char *)malloc(7000U);

    if (plan == NULL) {
        return NULL;
    }

    used = 0U;
    plan[0] = '\0';

    if (!llm_m251_append(plan, 7000U, &used,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_block\n"
            "path=SRC/TEST.C\n"
            "BEGIN_OLD_TEXT\n")) {
        free(plan);
        return NULL;
    }

    if (old_side) {
        for (index = 0; index < 70; ++index) {
            if (!llm_m251_append(
                    plan, 7000U, &used, fill_line)) {
                free(plan);
                return NULL;
            }
        }
    } else if (!llm_m251_append(
                   plan, 7000U, &used, "OLD\n")) {
        free(plan);
        return NULL;
    }

    if (!llm_m251_append(plan, 7000U, &used,
            "END_OLD_TEXT\n"
            "BEGIN_NEW_TEXT\n")) {
        free(plan);
        return NULL;
    }

    if (!old_side) {
        for (index = 0; index < 70; ++index) {
            if (!llm_m251_append(
                    plan, 7000U, &used, fill_line)) {
                free(plan);
                return NULL;
            }
        }
    } else if (!llm_m251_append(
                   plan, 7000U, &used, "NEW\n")) {
        free(plan);
        return NULL;
    }

    if (!llm_m251_append(plan, 7000U, &used,
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        free(plan);
        return NULL;
    }

    return plan;
}

int llm_m251_fence_old(void)
{
    char *normalized;

    normalized = llm_m195_normalize_count(
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "```c\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m251_fence_new(void)
{
    char *normalized;

    normalized = llm_m195_normalize_count(
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "   ``` fence\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m251_size_old(void)
{
    char *plan;
    char *normalized;

    plan = llm_m251_big_plan(1);

    if (plan == NULL) {
        return 0;
    }

    normalized = llm_m195_normalize_count(plan);
    free(plan);

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m251_size_new(void)
{
    char *plan;
    char *normalized;

    plan = llm_m251_big_plan(0);

    if (plan == NULL) {
        return 0;
    }

    normalized = llm_m195_normalize_count(plan);
    free(plan);

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int llm_m251_payload_ok_test(void)
{
    return llm_m195_normalized_equals(
        "operation_count=0\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n");
}

int llm_m251_narr_ok(void)
{
    return llm_m195_normalized_equals(
        "Goal\n"
        "Inline `code` is narrative.\n"
        "``` narrative fence\n"
        "operation_count=0\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        "Goal\n"
        "Inline `code` is narrative.\n"
        "``` narrative fence\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/TEST.C\n"
        "BEGIN_OLD_TEXT\n"
        "OLD\n"
        "END_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\n"
        "NEW\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n");
}
