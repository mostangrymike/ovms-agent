static int execute_sidecar_path(
    const char *path,
    char *sidecar,
    size_t sidecar_size)
{
    int count;

    if (path == NULL ||
        sidecar == NULL ||
        sidecar_size == 0U) {
        return 0;
    }

    count = snprintf(
        sidecar,
        sidecar_size,
        "%s.CHK",
        path);

    return count > 0 &&
        (size_t)count < sidecar_size;
}

static void execute_remove_pair(
    const char *path)
{
    char sidecar[LLM_EXECUTE_PATH_SIZE + 16];

    while (remove(path) == 0) {
    }

    if (execute_sidecar_path(
            path,
            sidecar,
            sizeof(sidecar))) {
        while (remove(sidecar) == 0) {
        }
    }
}

static int execute_rename_pair(
    const char *from_path,
    const char *to_path)
{
    char from_sidecar[LLM_EXECUTE_PATH_SIZE + 16];
    char to_sidecar[LLM_EXECUTE_PATH_SIZE + 16];

    if (!execute_sidecar_path(
            from_path,
            from_sidecar,
            sizeof(from_sidecar)) ||
        !execute_sidecar_path(
            to_path,
            to_sidecar,
            sizeof(to_sidecar))) {
        return 0;
    }

    while (remove(to_path) == 0) {
    }
    while (remove(to_sidecar) == 0) {
    }

    if (rename(from_path, to_path) != 0) {
        return 0;
    }

    if (rename(from_sidecar, to_sidecar) != 0) {
        (void)rename(to_path, from_path);
        return 0;
    }

    return 1;
}

static int execute_mark_consumed_file(
    const char *plan_path,
    const char *new_path,
    const char *backup_path,
    int force_install_failure)
{
    FILE *input;
    FILE *output;
    char line[LLM_EXECUTE_LINE_SIZE];
    int replaced;
    int success;

    if (plan_path == NULL ||
        new_path == NULL ||
        backup_path == NULL ||
        *plan_path == '\0' ||
        *new_path == '\0' ||
        *backup_path == '\0') {
        return 0;
    }

    input = fopen(plan_path, "r");

    if (input == NULL) {
        return 0;
    }

    output = fopen(new_path, "w");

    if (output == NULL) {
        (void)fclose(input);
        return 0;
    }

    replaced = 0;
    success = 1;

    while (fgets(line, sizeof(line), input) != NULL) {
        if (!replaced &&
            strncmp(line, "status=", 7U) == 0) {
            if (fputs(
                    "status=consumed\n",
                    output) == EOF) {
                success = 0;
                break;
            }

            replaced = 1;
        } else if (fputs(line, output) == EOF) {
            success = 0;
            break;
        }
    }

    if (fclose(input) != 0) {
        success = 0;
    }

    if (fclose(output) != 0) {
        success = 0;
    }

    if (!success ||
        !replaced ||
        !llm_plan_checksum_write(new_path)) {
        execute_remove_pair(new_path);
        return 0;
    }

    execute_remove_pair(backup_path);

    if (!execute_rename_pair(
            plan_path,
            backup_path)) {
        execute_remove_pair(new_path);
        return 0;
    }

    if (force_install_failure ||
        !execute_rename_pair(
            new_path,
            plan_path)) {
        (void)execute_rename_pair(
            backup_path,
            plan_path);
        execute_remove_pair(new_path);
        return 0;
    }

    execute_remove_pair(backup_path);
    return 1;
}

static int execute_mark_consumed_safe(void)
{
    return execute_mark_consumed_file(
        LLM_PLAN_FILE,
        "OVMS_AGENT_PLAN_NEW.TXT",
        "OVMS_AGENT_PLAN_OLD.TXT",
        0);
}
