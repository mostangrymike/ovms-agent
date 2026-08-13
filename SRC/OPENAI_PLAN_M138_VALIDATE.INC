static int openai_m138_write_text(
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

int openai_m138_body_tamper(
    const char *plan_path)
{
    char *text;
    char *position;
    int result;

    if (!openai_m138_write_text(
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
        !openai_plan_checksum_write(plan_path)) {
        return 0;
    }

    text = openai_m138_read_text(plan_path);

    if (text == NULL) {
        return 0;
    }

    position = strstr(text, "new_text=NEW");

    if (position == NULL) {
        free(text);
        return 0;
    }

    (void)memcpy(position, "new_text=BAD", 12U);
    result = openai_m138_write_text(plan_path, text);
    free(text);

    return
        result &&
        !openai_plan_checksum_valid(plan_path);
}

int openai_m138_meta_tamper(
    const char *plan_path)
{
    char *text;
    char *position;
    int result;

    if (!openai_m138_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") ||
        !openai_plan_checksum_write(plan_path)) {
        return 0;
    }

    text = openai_m138_read_text(plan_path);

    if (text == NULL) {
        return 0;
    }

    position = strstr(text, "status=active");

    if (position == NULL) {
        free(text);
        return 0;
    }

    (void)memcpy(position, "status=altered", 14U);
    result = openai_m138_write_text(plan_path, text);
    free(text);

    return
        result &&
        !openai_plan_checksum_valid(plan_path);
}

int openai_m138_partial_reject(
    const char *plan_path)
{
    return
        openai_m138_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") &&
        !openai_plan_checksum_valid(plan_path);
}

int openai_m138_save_preserves(
    const char *plan_path)
{
    char *before;
    char *after;
    int result;

    before = openai_m138_read_text(plan_path);

    if (before == NULL) {
        return 1;
    }

    result = !openai_plan_save(
        "",
        "INVALID");

    after = openai_m138_read_text(plan_path);

    result =
        result &&
        after != NULL &&
        strcmp(before, after) == 0;

    free(before);
    free(after);
    return result;
}
static int openai_m195_normalized_equals(
    const char *input,
    const char *expected)
{
    char *normalized;
    int result;

    normalized =
        openai_m195_normalize_count(input);

    if (normalized == NULL) {
        return 0;
    }

    result = strcmp(normalized, expected) == 0;
    free(normalized);
    return result;
}

int openai_m195_normalize_one(void)
{
    return openai_m195_normalized_equals(
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

int openai_m195_normalize_two(void)
{
    return openai_m195_normalized_equals(
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

int openai_m195_normalize_correct(void)
{
    return openai_m195_normalized_equals(
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n",
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");
}

int openai_m195_norm_inspect(void)
{
    return openai_m195_normalized_equals(
        "Inspection-only plan.\n"
        "No source changes are required.\n",
        "Inspection-only plan.\n"
        "No source changes are required.\n");
}

int openai_m195_reject_missing(void)
{
    char *normalized;

    normalized = openai_m195_normalize_count(
        "Executable plan\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");

    if (normalized != NULL) {
        free(normalized);
        return 0;
    }

    return 1;
}

int openai_m195_reject_duplicate(void)
{
    char *normalized;

    normalized = openai_m195_normalize_count(
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

int openai_m195_reject_markers(void)
{
    char *nested;
    char *unmatched;

    nested = openai_m195_normalize_count(
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n"
        "END_OPERATION\n");

    unmatched = openai_m195_normalize_count(
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

int openai_m195_reject_early_begin(void)
{
    char *normalized;

    normalized = openai_m195_normalize_count(
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

int openai_m195_failure_preserves(void)
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
        openai_m138_read_text(plan_path);
    check_before =
        openai_m138_read_text(checksum_path);

    result = !openai_plan_save(
        "M195 normalization rejection test",
        "operation_count=1\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n");

    plan_after =
        openai_m138_read_text(plan_path);
    check_after =
        openai_m138_read_text(checksum_path);

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
