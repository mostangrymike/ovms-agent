static int llm_m141_make_approved(
    const char *path)
{
    llm_plan_approval_clear();

    return
        llm_m140_write_plan(path, "active") &&
        llm_plan_approve_file(path);
}

static void llm_m141_remove_pair(
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

int llm_m141_status_unapproved(void)
{
    llm_plan_approval_clear();

    return strcmp(
        llm_plan_approval_name(),
        "NOT APPROVED") == 0;
}

int llm_m141_status_approved(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M141_STATUS.TMP";
    int result;

    llm_m141_remove_pair(path);

    if (!llm_m141_make_approved(path)) {
        llm_m141_remove_pair(path);
        return 0;
    }

    result = strcmp(
        llm_plan_approval_name(),
        "APPROVED FOR THIS SESSION") == 0;

    llm_m141_remove_pair(path);
    llm_plan_approval_clear();
    return result;
}

int llm_m141_show_no_grant(void)
{
    int before;

    llm_plan_approval_clear();
    before = llm_plan_approved;
    llm_plan_approval_report();

    return
        llm_plan_approved == before &&
        !llm_plan_approved;
}

int llm_m141_validate_no_grant(void)
{
    int before;

    llm_plan_approval_clear();
    before = llm_plan_approved;
    (void)llm_plan_is_current(0);

    return
        llm_plan_approved == before &&
        !llm_plan_approved;
}

int llm_m141_dry_unapproved(void)
{
    int before;
    unsigned long hash_before;

    llm_plan_approval_clear();
    before = llm_plan_approved;
    hash_before = llm_approved_hash;

    return
        llm_plan_approved == before &&
        llm_approved_hash == hash_before;
}

int llm_m141_dry_approved(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M141_DRY.TMP";
    int before;
    unsigned long hash_before;
    int result;

    llm_m141_remove_pair(path);

    if (!llm_m141_make_approved(path)) {
        llm_m141_remove_pair(path);
        return 0;
    }

    before = llm_plan_approved;
    hash_before = llm_approved_hash;

    result =
        llm_plan_approved == before &&
        llm_approved_hash == hash_before;

    llm_m141_remove_pair(path);
    llm_plan_approval_clear();
    return result;
}

int llm_m141_change_invalidated(
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

    if (llm_plan_approval_valid(path)) {
        return 0;
    }

    return strcmp(
        llm_plan_approval_name(),
        "INVALIDATED") == 0;
}

int llm_m141_refusal_message(void)
{
    llm_plan_approval_clear();

    return strcmp(
        llm_plan_approval_name(),
        "NOT APPROVED") == 0;
}
