static int openai_m142_make_approved(
    const char *path)
{
    openai_plan_approval_clear();

    return
        openai_m140_write_plan(path, "active") &&
        openai_plan_approve_file(path);
}

static void openai_m142_remove_pair(
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

int openai_m142_cancel_unapproved(void)
{
    openai_plan_approval_clear();

    return
        !openai_plan_approved &&
        !openai_approval_invalidated;
}

int openai_m142_fail_unapproved(void)
{
    openai_plan_approval_clear();

    return
        !openai_plan_approved &&
        openai_approved_hash == 0UL;
}

int openai_m142_validate_keeps(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_VALID.TMP";
    int before;
    unsigned long hash_before;
    int result;

    openai_m142_remove_pair(path);

    if (!openai_m142_make_approved(path) ||
        !openai_plan_approval_valid(path)) {
        openai_m142_remove_pair(path);
        openai_plan_approval_clear();
        return 0;
    }

    before = openai_plan_approved;
    hash_before = openai_approved_hash;
    result =
        openai_plan_approved == before &&
        openai_approved_hash == hash_before;

    openai_m142_remove_pair(path);
    openai_plan_approval_clear();
    return result;
}

int openai_m142_prewrite_keeps(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_PRE.TMP";
    int result;

    openai_m142_remove_pair(path);
    result =
        openai_m142_make_approved(path) &&
        openai_plan_approval_valid(path) &&
        openai_plan_approved &&
        !openai_approval_invalidated;

    openai_m142_remove_pair(path);
    openai_plan_approval_clear();
    return result;
}

int openai_m142_postwrite_clears(void)
{
    const char *path =
        "OVMS_AGENT_TEST_M142_POST.TMP";
    int result;

    openai_m142_remove_pair(path);

    if (!openai_m142_make_approved(path)) {
        openai_m142_remove_pair(path);
        return 0;
    }

    openai_plan_approval_consume();

    result =
        !openai_plan_approved &&
        openai_approved_hash == 0UL &&
        !openai_approval_invalidated;

    openai_m142_remove_pair(path);
    return result;
}

int openai_m142_success_clears(void)
{
    return openai_m142_postwrite_clears();
}

int openai_m142_checksum_invalid(
    const char *path)
{
    char checksum_path[
        OPENAI_PLAN_PATH_SIZE + 16];

    openai_plan_approval_clear();

    if (!openai_m140_write_plan(
            path,
            "active") ||
        !openai_plan_approve_file(path) ||
        !openai_plan_checksum_path(
            path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    while (remove(checksum_path) == 0) {
    }

    if (openai_plan_approval_valid(path)) {
        return 0;
    }

    return
        openai_approval_invalidated &&
        strcmp(
            openai_plan_approval_name(),
            "INVALIDATED") == 0;
}

int openai_m142_reapprove_clears(
    const char *path)
{
    FILE *file;

    openai_plan_approval_clear();

    if (!openai_m140_write_plan(
            path,
            "active") ||
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

    if (openai_plan_approval_valid(path) ||
        !openai_plan_checksum_write(path) ||
        !openai_plan_approve_file(path)) {
        return 0;
    }

    return
        openai_plan_approved &&
        !openai_approval_invalidated;
}
