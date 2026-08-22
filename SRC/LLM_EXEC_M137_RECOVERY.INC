static int execute_plan_file_exists(
    const char *path)
{
    FILE *file;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}

static int execute_plan_recover_files(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    int plan_exists;
    int new_exists;
    int backup_exists;

    if (plan_path == NULL ||
        new_path == NULL ||
        backup_path == NULL ||
        *plan_path == '\0' ||
        *new_path == '\0' ||
        *backup_path == '\0') {
        return 0;
    }

    plan_exists = execute_plan_file_exists(plan_path);
    new_exists = execute_plan_file_exists(new_path);
    backup_exists = execute_plan_file_exists(backup_path);

    if (plan_exists &&
        llm_plan_checksum_valid(plan_path)) {
        execute_remove_pair(new_path);
        execute_remove_pair(backup_path);
        return 1;
    }

    if (new_exists &&
        llm_plan_checksum_valid(new_path)) {
        execute_remove_pair(plan_path);

        if (!execute_rename_pair(
                new_path,
                plan_path)) {
            return 0;
        }

        execute_remove_pair(backup_path);
        return 1;
    }

    if (backup_exists &&
        llm_plan_checksum_valid(backup_path)) {
        execute_remove_pair(plan_path);

        if (!execute_rename_pair(
                backup_path,
                plan_path)) {
            return 0;
        }

        execute_remove_pair(new_path);
        return 1;
    }

    return !plan_exists &&
        !new_exists &&
        !backup_exists;
}

static int execute_plan_recover(void)
{
    return execute_plan_recover_files(
        LLM_PLAN_FILE,
        "OVMS_AGENT_PLAN_NEW.TXT",
        "OVMS_AGENT_PLAN_OLD.TXT");
}


int llm_plan_recover_paths(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    return execute_plan_recover_files(
        plan_path,
        new_path,
        backup_path);
}
