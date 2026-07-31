int openai_plan_recover_paths(
    const char *plan_path,
    const char *new_path,
    const char *backup_path);

int openai_state_load_path(
    const char *path);

static void openai_m145_remove_pair(
    const char *path)
{
    char checksum_path[
        OPENAI_PLAN_PATH_SIZE + 16];

    while (remove(path) == 0) {
    }

    if (openai_plan_checksum_path(
            path,
            checksum_path,
            sizeof(checksum_path))) {
        while (remove(checksum_path) == 0) {
        }
    }
}

static int openai_m145_make_plan(
    const char *path,
    const char *status)
{
    openai_m145_remove_pair(path);
    return openai_m140_write_plan(path, status);
}

static int openai_m145_is_unapproved(void)
{
    return
        !openai_plan_approved &&
        openai_approved_hash == 0UL &&
        !openai_approval_invalidated &&
        strcmp(
            openai_plan_approval_name(),
            "NOT APPROVED") == 0;
}

int openai_m145_fresh_unapproved(void)
{
    openai_plan_session_reset();
    return openai_m145_is_unapproved();
}

int openai_m145_active_no_restore(
    const char *path)
{
    int result;

    if (!openai_m145_make_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    openai_plan_session_reset();
    openai_plan_recovery_sync(path);
    result = openai_m145_is_unapproved();
    openai_m145_remove_pair(path);
    return result;
}

int openai_m145_consumed_no_restore(
    const char *path)
{
    int result;

    if (!openai_m145_make_plan(path, "consumed")) {
        return 0;
    }

    openai_plan_session_reset();
    openai_plan_recovery_sync(path);
    result = openai_m145_is_unapproved();
    openai_m145_remove_pair(path);
    return result;
}

int openai_m145_new_no_restore(
    const char *plan_path,
    const char *new_path,
    const char *old_path)
{
    int result;

    openai_m145_remove_pair(plan_path);
    openai_m145_remove_pair(new_path);
    openai_m145_remove_pair(old_path);

    if (!openai_m145_make_plan(new_path, "active")) {
        return 0;
    }

    openai_plan_session_reset();

    result =
        openai_plan_recover_paths(
            plan_path,
            new_path,
            old_path) &&
        openai_m145_is_unapproved();

    openai_m145_remove_pair(plan_path);
    openai_m145_remove_pair(new_path);
    openai_m145_remove_pair(old_path);
    return result;
}

int openai_m145_old_no_restore(
    const char *plan_path,
    const char *new_path,
    const char *old_path)
{
    int result;

    openai_m145_remove_pair(plan_path);
    openai_m145_remove_pair(new_path);
    openai_m145_remove_pair(old_path);

    if (!openai_m145_make_plan(old_path, "active")) {
        return 0;
    }

    openai_plan_session_reset();

    result =
        openai_plan_recover_paths(
            plan_path,
            new_path,
            old_path) &&
        openai_m145_is_unapproved();

    openai_m145_remove_pair(plan_path);
    openai_m145_remove_pair(new_path);
    openai_m145_remove_pair(old_path);
    return result;
}

int openai_m145_state_no_restore(
    const char *path)
{
    FILE *file;
    int result;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    result =
        fprintf(
            file,
            "format=1\n"
            "last_workflow=0\n"
            "last_build_known=0\n"
            "last_build_status=0\n"
            "last_rollback=0\n") >= 0 &&
        fclose(file) == 0;

    if (!result) {
        return 0;
    }

    openai_plan_session_reset();
    result =
        openai_state_load_path(path) &&
        openai_m145_is_unapproved();

    while (remove(path) == 0) {
    }

    return result;
}

int openai_m145_logs_no_restore(void)
{
    openai_plan_session_reset();
    openai_m143_log_reset();
    openai_m143_emit_cancel();

    return
        openai_m143_cancel_logs() == 1U &&
        openai_m145_is_unapproved();
}

int openai_m145_reset_authority(
    const char *path)
{
    int result;

    if (!openai_m145_make_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    openai_plan_session_reset();
    result = openai_m145_is_unapproved();
    openai_m145_remove_pair(path);
    return result;
}
