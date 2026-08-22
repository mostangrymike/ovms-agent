int llm_plan_recover_paths(
    const char *plan_path,
    const char *new_path,
    const char *backup_path);

int llm_state_load_path(
    const char *path);

static void llm_m145_remove_pair(
    const char *path)
{
    char checksum_path[
        LLM_PLAN_PATH_SIZE + 16];

    while (remove(path) == 0) {
    }

    if (llm_plan_checksum_path(
            path,
            checksum_path,
            sizeof(checksum_path))) {
        while (remove(checksum_path) == 0) {
        }
    }
}

static int llm_m145_make_plan(
    const char *path,
    const char *status)
{
    llm_m145_remove_pair(path);
    return llm_m140_write_plan(path, status);
}

static int llm_m145_is_unapproved(void)
{
    return
        !llm_plan_approved &&
        llm_approved_hash == 0UL &&
        !llm_approval_invalidated &&
        strcmp(
            llm_plan_approval_name(),
            "NOT APPROVED") == 0;
}

int llm_m145_fresh_unapproved(void)
{
    llm_plan_session_reset();
    return llm_m145_is_unapproved();
}

int llm_m145_active_no_restore(
    const char *path)
{
    int result;

    if (!llm_m145_make_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    llm_plan_session_reset();
    llm_plan_recovery_sync(path);
    result = llm_m145_is_unapproved();
    llm_m145_remove_pair(path);
    return result;
}

int llm_m145_consumed_no_restore(
    const char *path)
{
    int result;

    if (!llm_m145_make_plan(path, "consumed")) {
        return 0;
    }

    llm_plan_session_reset();
    llm_plan_recovery_sync(path);
    result = llm_m145_is_unapproved();
    llm_m145_remove_pair(path);
    return result;
}

int llm_m145_new_no_restore(
    const char *plan_path,
    const char *new_path,
    const char *old_path)
{
    int result;

    llm_m145_remove_pair(plan_path);
    llm_m145_remove_pair(new_path);
    llm_m145_remove_pair(old_path);

    if (!llm_m145_make_plan(new_path, "active")) {
        return 0;
    }

    llm_plan_session_reset();

    result =
        llm_plan_recover_paths(
            plan_path,
            new_path,
            old_path) &&
        llm_m145_is_unapproved();

    llm_m145_remove_pair(plan_path);
    llm_m145_remove_pair(new_path);
    llm_m145_remove_pair(old_path);
    return result;
}

int llm_m145_old_no_restore(
    const char *plan_path,
    const char *new_path,
    const char *old_path)
{
    int result;

    llm_m145_remove_pair(plan_path);
    llm_m145_remove_pair(new_path);
    llm_m145_remove_pair(old_path);

    if (!llm_m145_make_plan(old_path, "active")) {
        return 0;
    }

    llm_plan_session_reset();

    result =
        llm_plan_recover_paths(
            plan_path,
            new_path,
            old_path) &&
        llm_m145_is_unapproved();

    llm_m145_remove_pair(plan_path);
    llm_m145_remove_pair(new_path);
    llm_m145_remove_pair(old_path);
    return result;
}

int llm_m145_state_no_restore(
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

    llm_plan_session_reset();
    result =
        llm_state_load_path(path) &&
        llm_m145_is_unapproved();

    while (remove(path) == 0) {
    }

    return result;
}

int llm_m145_logs_no_restore(void)
{
    llm_plan_session_reset();
    llm_m143_log_reset();
    llm_m143_emit_cancel();

    return
        llm_m143_cancel_logs() == 1U &&
        llm_m145_is_unapproved();
}

int llm_m145_reset_authority(
    const char *path)
{
    int result;

    if (!llm_m145_make_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    llm_plan_session_reset();
    result = llm_m145_is_unapproved();
    llm_m145_remove_pair(path);
    return result;
}
