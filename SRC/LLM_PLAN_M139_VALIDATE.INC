int openai_m139_clear_sidecar(
    const char *plan_path)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];

    if (!openai_m138_write_text(
            plan_path,
            "format=1\nstatus=active\nfile_count=0\n") ||
        !openai_plan_checksum_write(plan_path) ||
        !openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    return
        openai_plan_clear_files(plan_path) &&
        !openai_m139_file_exists(plan_path) &&
        !openai_m139_file_exists(checksum_path);
}

int openai_m139_missing_checksum(
    const char *plan_path)
{
    if (!openai_m138_write_text(
            plan_path,
            "format=1\nstatus=active\nfile_count=0\n")) {
        return 0;
    }

    return !openai_plan_file_current(
        plan_path,
        0);
}

int openai_m139_orphan_checksum(
    const char *plan_path)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];

    if (!openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    while (remove(plan_path) == 0) {
    }

    if (!openai_m138_write_text(
            checksum_path,
            "00000000\n")) {
        return 0;
    }

    return !openai_plan_file_current(
        plan_path,
        0);
}

int openai_m139_consumed_checksum(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    (void)new_path;
    (void)backup_path;

    if (!openai_m138_write_text(
            plan_path,
            "format=1\nstatus=consumed\nfile_count=0\n") ||
        !openai_plan_checksum_write(plan_path)) {
        return 0;
    }

    return
        openai_plan_checksum_valid(plan_path) &&
        !openai_plan_file_current(plan_path, 0);
}
