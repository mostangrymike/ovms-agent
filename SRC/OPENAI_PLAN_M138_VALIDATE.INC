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
