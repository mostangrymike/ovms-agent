static int llm_m140_write_plan(
    const char *path,
    const char *status)
{
    FILE *file;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fprintf(
            file,
            "format=1\n"
            "status=%s\n"
            "file_count=0\n",
            status) < 0 ||
        fclose(file) != 0) {
        return 0;
    }

    return llm_plan_checksum_write(path);
}

int llm_m140_unapproved_reject(
    const char *path)
{
    llm_plan_approval_clear();

    return
        llm_m140_write_plan(path, "active") &&
        !llm_plan_approval_valid(path);
}

int llm_m140_approved_accept(
    const char *path)
{
    llm_plan_approval_clear();

    return
        llm_m140_write_plan(path, "active") &&
        llm_plan_approve_file(path) &&
        llm_plan_approval_valid(path);
}

int llm_m140_change_reject(
    const char *path)
{
    FILE *file;

    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    if (fputs("changed=1\n", file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return !llm_plan_approval_valid(path);
}

int llm_m140_new_plan_clears(
    const char *path)
{
    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    llm_plan_approval_clear();
    return !llm_plan_approved;
}

int llm_m140_clear_clears(
    const char *path)
{
    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    if (!llm_plan_clear_files(path)) {
        return 0;
    }

    llm_plan_approval_clear();
    return !llm_plan_approved;
}

int llm_m140_consume_clears(void)
{
    llm_plan_approved = 1;
    llm_approved_hash = 1UL;
    llm_plan_approval_clear();

    return
        !llm_plan_approved &&
        llm_approved_hash == 0UL;
}

int llm_m140_restart_unapproved(void)
{
    llm_plan_approval_clear();
    return !llm_plan_approved;
}

int llm_m140_prewrite_guard(
    const char *path)
{
    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active")) {
        return 0;
    }

    if (llm_plan_approval_valid(path)) {
        return 0;
    }

    return llm_plan_approve_file(path) &&
        llm_plan_approval_valid(path);
}
