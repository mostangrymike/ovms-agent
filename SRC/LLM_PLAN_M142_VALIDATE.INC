static int llm_m142_make_approved(
    const char *path)
{
    llm_plan_approval_clear();

    return
        llm_m140_write_plan(path, "active") &&
        llm_plan_approve_file(path);
}

static void llm_m142_remove_pair(
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

int llm_m142_cancel_unapproved(void)
{
    llm_plan_approval_clear();

    return
        !llm_plan_approved &&
        !llm_approval_invalidated;
}

int llm_m142_fail_unapproved(void)
{
    llm_plan_approval_clear();

    return
        !llm_plan_approved &&
        llm_approved_hash == 0UL;
}

int llm_m142_validate_keeps(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_VALID.TMP";
    int before;
    unsigned long hash_before;
    int result;

    llm_m142_remove_pair(path);

    if (!llm_m142_make_approved(path) ||
        !llm_plan_approval_valid(path)) {
        llm_m142_remove_pair(path);
        llm_plan_approval_clear();
        return 0;
    }

    before = llm_plan_approved;
    hash_before = llm_approved_hash;
    result =
        llm_plan_approved == before &&
        llm_approved_hash == hash_before;

    llm_m142_remove_pair(path);
    llm_plan_approval_clear();
    return result;
}

int llm_m142_prewrite_keeps(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_PRE.TMP";
    int result;

    llm_m142_remove_pair(path);
    result =
        llm_m142_make_approved(path) &&
        llm_plan_approval_valid(path) &&
        llm_plan_approved &&
        !llm_approval_invalidated;

    llm_m142_remove_pair(path);
    llm_plan_approval_clear();
    return result;
}

int llm_m142_postwrite_clears(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_POST.TMP";
    int result;

    llm_m142_remove_pair(path);

    if (!llm_m142_make_approved(path)) {
        llm_m142_remove_pair(path);
        return 0;
    }

    llm_plan_approval_consume();

    result =
        !llm_plan_approved &&
        llm_approved_hash == 0UL &&
        !llm_approval_invalidated;

    llm_m142_remove_pair(path);
    return result;
}

int llm_m142_success_clears(void)
{
    return llm_m142_postwrite_clears();
}

int llm_m142_checksum_invalid(
    const char *path)
{
    char checksum_path[
        LLM_PLAN_PATH_SIZE + 16];

    llm_plan_approval_clear();

    if (!llm_m140_write_plan(
            path,
            "active") ||
        !llm_plan_approve_file(path) ||
        !llm_plan_checksum_path(
            path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    while (remove(checksum_path) == 0) {
    }

    if (llm_plan_approval_valid(path)) {
        return 0;
    }

    return
        llm_approval_invalidated &&
        strcmp(
            llm_plan_approval_name(),
            "INVALIDATED") == 0;
}

int llm_m142_reapprove_clears(
    const char *path)
{
    FILE *file;

    llm_plan_approval_clear();

    if (!llm_m140_write_plan(
            path,
            "active") ||
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

    if (llm_plan_approval_valid(path) ||
        !llm_plan_checksum_write(path) ||
        !llm_plan_approve_file(path)) {
        return 0;
    }

    return
        llm_plan_approved &&
        !llm_approval_invalidated;
}
