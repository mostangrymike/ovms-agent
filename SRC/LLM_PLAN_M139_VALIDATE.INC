int llm_m139_clear_sidecar(
    const char *plan_path)
{
    char checksum_path[LLM_PLAN_PATH_SIZE + 16];

    if (!llm_m138_write_text(
            plan_path,
            "format=1\nstatus=active\nfile_count=0\n") ||
        !llm_plan_checksum_write(plan_path) ||
        !llm_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    return
        llm_plan_clear_files(plan_path) &&
        !llm_m139_file_exists(plan_path) &&
        !llm_m139_file_exists(checksum_path);
}

int llm_m139_missing_checksum(
    const char *plan_path)
{
    if (!llm_m138_write_text(
            plan_path,
            "format=1\nstatus=active\nfile_count=0\n")) {
        return 0;
    }

    return !llm_plan_file_current(
        plan_path,
        0);
}

int llm_m139_orphan_checksum(
    const char *plan_path)
{
    char checksum_path[LLM_PLAN_PATH_SIZE + 16];

    if (!llm_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    while (remove(plan_path) == 0) {
    }

    if (!llm_m138_write_text(
            checksum_path,
            "00000000\n")) {
        return 0;
    }

    return !llm_plan_file_current(
        plan_path,
        0);
}

int llm_m139_consumed_checksum(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    (void)new_path;
    (void)backup_path;

    if (!llm_m138_write_text(
            plan_path,
            "format=1\nstatus=consumed\nfile_count=0\n") ||
        !llm_plan_checksum_write(plan_path)) {
        return 0;
    }

    return
        llm_plan_checksum_valid(plan_path) &&
        !llm_plan_file_current(plan_path, 0);
}
