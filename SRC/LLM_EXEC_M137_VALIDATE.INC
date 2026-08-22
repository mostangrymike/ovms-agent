static int llm_m137_write_text(
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

static int llm_m137_contains(
    const char *path,
    const char *text)
{
    char *content;
    int result;

    content = llm_read_text_file(path);

    if (content == NULL) {
        return 0;
    }

    result = strstr(content, text) != NULL;
    free(content);
    return result;
}

int llm_m137_success_cleanup(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    int result;

    execute_remove_pair(plan_path);
    execute_remove_pair(new_path);
    execute_remove_pair(backup_path);

    if (!llm_m137_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    result = execute_mark_consumed_file(
        plan_path,
        new_path,
        backup_path,
        0);

    return
        result &&
        execute_plan_file_exists(plan_path) &&
        !execute_plan_file_exists(new_path) &&
        !execute_plan_file_exists(backup_path) &&
        llm_m137_contains(
            plan_path,
            "status=consumed") &&
        llm_plan_checksum_valid(plan_path);
}

int llm_m137_failure_restore(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    int result;

    execute_remove_pair(plan_path);
    execute_remove_pair(new_path);
    execute_remove_pair(backup_path);

    if (!llm_m137_write_text(
            plan_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    result = !execute_mark_consumed_file(
        plan_path,
        new_path,
        backup_path,
        1);

    return
        result &&
        execute_plan_file_exists(plan_path) &&
        !execute_plan_file_exists(new_path) &&
        !execute_plan_file_exists(backup_path) &&
        llm_m137_contains(
            plan_path,
            "status=active") &&
        llm_plan_checksum_valid(plan_path);
}

int llm_m137_consumed_rejected(
    const char *plan_path)
{
    execute_remove_pair(plan_path);

    if (!llm_m137_write_text(
            plan_path,
            "format=1\n"
            "status=consumed\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    return
        llm_plan_checksum_valid(plan_path) &&
        !llm_plan_file_current(
            plan_path,
            0);
}

int llm_m137_artifact_recovery(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    int result;

    execute_remove_pair(plan_path);
    execute_remove_pair(new_path);
    execute_remove_pair(backup_path);

    if (!llm_m137_write_text(
            new_path,
            "format=1\n"
            "status=consumed\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(new_path) ||
        !llm_m137_write_text(
            backup_path,
            "format=1\n"
            "status=active\n"
            "file_count=0\n") ||
        !llm_plan_checksum_write(backup_path)) {
        return 0;
    }

    result = execute_plan_recover_files(
        plan_path,
        new_path,
        backup_path);

    return
        result &&
        execute_plan_file_exists(plan_path) &&
        !execute_plan_file_exists(new_path) &&
        !execute_plan_file_exists(backup_path) &&
        llm_m137_contains(
            plan_path,
            "status=consumed") &&
        llm_plan_checksum_valid(plan_path) &&
        !llm_plan_file_current(
            plan_path,
            0);
}
