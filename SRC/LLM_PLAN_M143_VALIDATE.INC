int llm_m143_first_inv_once(
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

    llm_m143_log_reset();
    (void)llm_plan_approval_valid(path);

    return llm_m143_inv_logs() == 1U;
}

int llm_m143_repeat_no_dup(
    const char *path)
{
    if (!llm_m143_first_inv_once(path)) {
        return 0;
    }

    (void)llm_plan_approval_valid(path);
    return llm_m143_inv_logs() == 1U;
}

int llm_m143_cancel_once(void)
{
    llm_m143_log_reset();
    llm_m143_emit_cancel();
    return llm_m143_cancel_logs() == 1U;
}

int llm_m143_fail_once(void)
{
    llm_m143_log_reset();
    llm_m143_emit_fail();
    return llm_m143_fail_logs() == 1U;
}

int llm_m143_same_recovery(
    const char *path)
{
    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    llm_plan_recovery_sync(path);
    return llm_plan_approved != 0;
}

int llm_m143_diff_recovery(
    const char *path)
{
    FILE *file;

    if (!llm_m143_same_recovery(path)) {
        return 0;
    }

    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    if (fputs("recovered=changed\n", file) == EOF ||
        fclose(file) != 0 ||
        !llm_plan_checksum_write(path)) {
        return 0;
    }

    llm_plan_recovery_sync(path);
    return llm_approval_invalidated != 0;
}

int llm_m143_consumed_clear(
    const char *path)
{
    if (!llm_m143_same_recovery(path) ||
        !llm_m140_write_plan(path, "consumed")) {
        return 0;
    }

    llm_plan_recovery_sync(path);

    return
        !llm_plan_approved &&
        !llm_approval_invalidated;
}

int llm_m143_partial_clear_safe(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M143_CLEAR.TMP";

    llm_plan_approval_clear();

    if (!llm_m140_write_plan(path, "active") ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    llm_plan_approval_clear();

    return
        !llm_plan_approved &&
        llm_approved_hash == 0UL &&
        !llm_approval_invalidated;
}
