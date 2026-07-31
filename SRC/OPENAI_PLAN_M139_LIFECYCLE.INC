void openai_plan_approval_report(void);

static int openai_m139_file_exists(
    const char *path)
{
    FILE *file;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}

int openai_plan_clear_files(
    const char *plan_path)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];
    char temp_path[OPENAI_PLAN_PATH_SIZE + 16];
    char old_path[OPENAI_PLAN_PATH_SIZE + 16];
    int count;
    int success;

    if (plan_path == NULL ||
        *plan_path == '\0' ||
        !openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path)) ||
        !openai_plan_checksum_temp(
            plan_path,
            temp_path,
            sizeof(temp_path))) {
        return 0;
    }

    count = snprintf(
        old_path,
        sizeof(old_path),
        "%s.CHK_OLD",
        plan_path);

    if (count <= 0 ||
        (size_t)count >= sizeof(old_path)) {
        return 0;
    }

    success = 1;

    while (remove(plan_path) == 0) {
    }

    if (errno != ENOENT) {
        success = 0;
    }

    while (remove(checksum_path) == 0) {
    }
    while (remove(temp_path) == 0) {
    }
    while (remove(old_path) == 0) {
    }

    if (strcmp(plan_path, OPENAI_PLAN_FILE) == 0) {
        while (remove("OVMS_AGENT_PLAN_SAVE_OLD.TXT") == 0) {
        }
        while (remove("OVMS_AGENT_PLAN_SAVE_OLD.CHK") == 0) {
        }

        openai_plan_approval_clear();
    }

    return success;
}

void openai_plan_show(void)
{
    if (!openai_m139_file_exists(
            OPENAI_PLAN_FILE)) {
        (void)puts(
            "No saved implementation plan is available.");
        return;
    }

    if (!openai_plan_checksum_valid(
            OPENAI_PLAN_FILE)) {
        (void)puts(
            "Saved implementation plan failed integrity validation.");
        return;
    }

    openai_plan_show_legacy();
    openai_plan_approval_report();
}

void openai_plan_clear(void)
{
    char answer[32];

    (void)printf(
        "Clear the saved implementation plan [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return;
    }

    if (answer[0] != 'y' &&
        answer[0] != 'Y') {
        (void)puts(
            "Saved implementation plan clear cancelled.");
        return;
    }

    if (!openai_plan_clear_files(
            OPENAI_PLAN_FILE)) {
        (void)printf(
            "Unable to remove %s and its checksum artifacts.\n",
            OPENAI_PLAN_FILE);
        return;
    }

    (void)puts(
        "Saved implementation plan cleared.");
}
