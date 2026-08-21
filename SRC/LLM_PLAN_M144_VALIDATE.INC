static int openai_m144_file_lacks(
    const char *path,
    const char *text)
{
    FILE *file;
    char line[256];

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, text) != NULL) {
            (void)fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

int openai_m144_approved_fixture(void)
{
    return openai_m141_status_approved();
}

int openai_m144_invalid_fixture(
    const char *path)
{
    return openai_m141_change_invalidated(path);
}

int openai_m144_unapproved_fixture(void)
{
    return openai_m141_status_unapproved();
}

int openai_m144_reapprove_fixture(
    const char *path)
{
    return openai_m142_reapprove_clears(path);
}

int openai_m144_prewrite_prod(void)
{
    return openai_m142_postwrite_clears();
}

int openai_m144_status_prod(void)
{
    return
        openai_m141_status_unapproved() &&
        openai_m141_status_approved();
}

int openai_m144_recovery_prod(
    const char *path)
{
    return openai_m143_same_recovery(path);
}

int openai_m144_no_direct_globals(void)
{
    const char *files[] = {
        "[.SRC]LLM_PLAN_M141_VALIDATE.INC",
        "[.SRC]LLM_PLAN_M142_VALIDATE.INC",
        "[.SRC]LLM_PLAN_M143_VALIDATE.INC",
        "[.SRC]LLM_PLAN_M144_VALIDATE.INC"
    };
    char approved_text[40];
    char hash_text[40];
    char invalid_text[48];
    size_t index;

    (void)sprintf(
        approved_text,
        "%s%s",
        "openai_plan_",
        "approved = ");

    (void)sprintf(
        hash_text,
        "%s%s",
        "openai_approved_",
        "hash = ");

    (void)sprintf(
        invalid_text,
        "%s%s",
        "openai_approval_",
        "invalidated = ");

    for (index = 0U;
         index < sizeof(files) / sizeof(files[0]);
         ++index) {
        if (!openai_m144_file_lacks(
                files[index],
                approved_text) ||
            !openai_m144_file_lacks(
                files[index],
                hash_text) ||
            !openai_m144_file_lacks(
                files[index],
                invalid_text)) {
            return 0;
        }
    }

    return 1;
}
