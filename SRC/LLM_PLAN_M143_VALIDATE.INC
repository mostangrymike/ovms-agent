int openai_m143_first_inv_once(
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

    openai_m143_log_reset();
    (void)openai_plan_approval_valid(path);

    return openai_m143_inv_logs() == 1U;
}

int openai_m143_repeat_no_dup(
    const char *path)
{
    if (!openai_m143_first_inv_once(path)) {
        return 0;
    }

    (void)openai_plan_approval_valid(path);
    return openai_m143_inv_logs() == 1U;
}

int openai_m143_cancel_once(void)
{
    openai_m143_log_reset();
    openai_m143_emit_cancel();
    return openai_m143_cancel_logs() == 1U;
}

int openai_m143_fail_once(void)
{
    openai_m143_log_reset();
    openai_m143_emit_fail();
    return openai_m143_fail_logs() == 1U;
}

int openai_m143_same_recovery(
    const char *path)
{
    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    openai_plan_recovery_sync(path);
    return openai_plan_approved != 0;
}

int openai_m143_diff_recovery(
    const char *path)
{
    FILE *file;

    if (!openai_m143_same_recovery(path)) {
        return 0;
    }

    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    if (fputs("recovered=changed\n", file) == EOF ||
        fclose(file) != 0 ||
        !openai_plan_checksum_write(path)) {
        return 0;
    }

    openai_plan_recovery_sync(path);
    return openai_approval_invalidated != 0;
}

int openai_m143_consumed_clear(
    const char *path)
{
    if (!openai_m143_same_recovery(path) ||
        !openai_m140_write_plan(path, "consumed")) {
        return 0;
    }

    openai_plan_recovery_sync(path);

    return
        !openai_plan_approved &&
        !openai_approval_invalidated;
}

int openai_m143_partial_clear_safe(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M143_CLEAR.TMP";

    openai_plan_approval_clear();

    if (!openai_m140_write_plan(path, "active") ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    openai_plan_approval_clear();

    return
        !openai_plan_approved &&
        openai_approved_hash == 0UL &&
        !openai_approval_invalidated;
}
