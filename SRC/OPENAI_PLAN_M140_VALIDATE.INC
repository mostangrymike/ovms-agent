static int openai_m140_write_plan(
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

    return openai_plan_checksum_write(path);
}

int openai_m140_unapproved_reject(
    const char *path)
{
    openai_plan_approval_clear();

    return
        openai_m140_write_plan(path, "active") &&
        !openai_plan_approval_valid(path);
}

int openai_m140_approved_accept(
    const char *path)
{
    openai_plan_approval_clear();

    return
        openai_m140_write_plan(path, "active") &&
        openai_plan_approve_file(path) &&
        openai_plan_approval_valid(path);
}

int openai_m140_change_reject(
    const char *path)
{
    FILE *file;

    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
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

    return !openai_plan_approval_valid(path);
}

int openai_m140_new_plan_clears(
    const char *path)
{
    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    openai_plan_approval_clear();
    return !openai_plan_approved;
}

int openai_m140_clear_clears(
    const char *path)
{
    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    if (!openai_plan_clear_files(path)) {
        return 0;
    }

    openai_plan_approval_clear();
    return !openai_plan_approved;
}

int openai_m140_consume_clears(void)
{
    openai_plan_approved = 1;
    openai_approved_hash = 1UL;
    openai_plan_approval_clear();

    return
        !openai_plan_approved &&
        openai_approved_hash == 0UL;
}

int openai_m140_restart_unapproved(void)
{
    openai_plan_approval_clear();
    return !openai_plan_approved;
}

int openai_m140_prewrite_guard(
    const char *path)
{
    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active")) {
        return 0;
    }

    if (openai_plan_approval_valid(path)) {
        return 0;
    }

    return openai_plan_approve_file(path) &&
        openai_plan_approval_valid(path);
}
