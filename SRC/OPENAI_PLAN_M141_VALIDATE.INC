static int openai_m141_make_approved(
    const char *path)
{
    openai_plan_approval_clear();

    return
        openai_m140_write_plan(path, "active") &&
        openai_plan_approve_file(path);
}

static void openai_m141_remove_pair(
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

int openai_m141_status_unapproved(void)
{
    openai_plan_approval_clear();

    return strcmp(
        openai_plan_approval_name(),
        "NOT APPROVED") == 0;
}

int openai_m141_status_approved(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M141_STATUS.TMP";
    int result;

    openai_m141_remove_pair(path);

    if (!openai_m141_make_approved(path)) {
        openai_m141_remove_pair(path);
        return 0;
    }

    result = strcmp(
        openai_plan_approval_name(),
        "APPROVED FOR THIS SESSION") == 0;

    openai_m141_remove_pair(path);
    openai_plan_approval_clear();
    return result;
}

int openai_m141_show_no_grant(void)
{
    int before;

    openai_plan_approval_clear();
    before = openai_plan_approved;
    openai_plan_approval_report();

    return
        openai_plan_approved == before &&
        !openai_plan_approved;
}

int openai_m141_validate_no_grant(void)
{
    int before;

    openai_plan_approval_clear();
    before = openai_plan_approved;
    (void)openai_plan_is_current(0);

    return
        openai_plan_approved == before &&
        !openai_plan_approved;
}

int openai_m141_dry_unapproved(void)
{
    int before;
    unsigned long hash_before;

    openai_plan_approval_clear();
    before = openai_plan_approved;
    hash_before = openai_approved_hash;

    return
        openai_plan_approved == before &&
        openai_approved_hash == hash_before;
}

int openai_m141_dry_approved(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M141_DRY.TMP";
    int before;
    unsigned long hash_before;
    int result;

    openai_m141_remove_pair(path);

    if (!openai_m141_make_approved(path)) {
        openai_m141_remove_pair(path);
        return 0;
    }

    before = openai_plan_approved;
    hash_before = openai_approved_hash;

    result =
        openai_plan_approved == before &&
        openai_approved_hash == hash_before;

    openai_m141_remove_pair(path);
    openai_plan_approval_clear();
    return result;
}

int openai_m141_change_invalidated(
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

    if (openai_plan_approval_valid(path)) {
        return 0;
    }

    return strcmp(
        openai_plan_approval_name(),
        "INVALIDATED") == 0;
}

int openai_m141_refusal_message(void)
{
    openai_plan_approval_clear();

    return strcmp(
        openai_plan_approval_name(),
        "NOT APPROVED") == 0;
}
