void openai_log_event(
    const char *command,
    const char *event,
    int value);

int openai_plan_approved = 0;
unsigned long openai_approved_hash = 0UL;
int openai_approval_invalidated = 0;
static unsigned int openai_inv_log_count = 0U;
static unsigned int openai_cancel_log_count = 0U;
static unsigned int openai_fail_log_count = 0U;

void openai_plan_approval_clear(void)
{
    openai_plan_approved = 0;
    openai_approved_hash = 0UL;
    openai_approval_invalidated = 0;
}

static void openai_log_invalidated(void)
{
    ++openai_inv_log_count;
    openai_log_event(
        "AGENT/APPROVE",
        "approval_invalidated",
        0);
}

static void openai_log_cancelled(void)
{
    ++openai_cancel_log_count;
    openai_log_event(
        "AGENT/APPROVE",
        "approval_cancelled",
        0);
}

static void openai_log_failed(void)
{
    ++openai_fail_log_count;
    openai_log_event(
        "AGENT/APPROVE",
        "approval_failed",
        0);
}

static void openai_approval_invalidate(void)
{
    openai_plan_approved = 0;
    openai_approved_hash = 0UL;
    openai_approval_invalidated = 1;
    openai_log_invalidated();
}

static int openai_approval_value(
    const char *plan_path,
    unsigned long *value_out)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];
    char line[64];
    unsigned long value;
    char extra;
    FILE *file;

    if (plan_path == NULL ||
        value_out == NULL ||
        !openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path))) {
        return 0;
    }

    file = fopen(checksum_path, "r");

    if (file == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        fclose(file) != 0 ||
        sscanf(line, "%lx %c", &value, &extra) != 1) {
        return 0;
    }

    *value_out = value & 0xffffffffUL;
    return 1;
}

const char *openai_plan_approval_name(void)
{
    if (openai_plan_approved) {
        return "APPROVED FOR THIS SESSION";
    }

    if (openai_approval_invalidated) {
        return "INVALIDATED";
    }

    return "NOT APPROVED";
}

void openai_plan_approval_report(void)
{
    (void)printf(
        "Plan approval:             %s\n",
        openai_plan_approval_name());
}

int openai_plan_approve_file(
    const char *plan_path)
{
    unsigned long value;

    if (!openai_plan_file_current(
            plan_path,
            0) ||
        !openai_approval_value(
            plan_path,
            &value)) {
        openai_plan_approval_clear();
        return 0;
    }

    openai_approved_hash = value;
    openai_plan_approved = 1;
    openai_approval_invalidated = 0;
    return 1;
}

int openai_plan_approval_valid(
    const char *plan_path)
{
    unsigned long value;
    int was_approved;

    was_approved = openai_plan_approved;

    if (!openai_plan_approved ||
        !openai_plan_checksum_valid(plan_path) ||
        !openai_approval_value(
            plan_path,
            &value) ||
        value != openai_approved_hash) {
        if (was_approved) {
            openai_approval_invalidate();
        }

        return 0;
    }

    return 1;
}

int openai_plan_confirm_text(
    const char *text)
{
    if (text == NULL) {
        return 0;
    }

    return
        (text[0] == 'y' || text[0] == 'Y') &&
        text[1] == '\0';
}

static int openai_plan_read_confirm(
    char *answer,
    size_t answer_size)
{
    size_t length;
    int character;

    if (answer == NULL ||
        answer_size < 2U ||
        fgets(answer, answer_size, stdin) == NULL) {
        return 0;
    }

    length = strlen(answer);

    if (length > 0U &&
        answer[length - 1U] == '\n') {
        answer[length - 1U] = '\0';
        return 1;
    }

    while ((character = getchar()) != '\n' &&
           character != EOF) {
    }

    answer[0] = '\0';
    return 0;
}

void openai_plan_approve(void)
{
    char answer[32];

    openai_plan_approval_clear();

    if (!openai_plan_is_current(1)) {
        (void)puts(
            "Saved plan cannot be approved. Run AGENT/PLAN again.");
        openai_log_failed();
        return;
    }

    openai_plan_show();

    (void)printf(
        "Approve this saved plan for execution in this session [y/N]? ");
    (void)fflush(stdout);

    if (!openai_plan_read_confirm(
            answer,
            sizeof(answer))) {
        (void)putchar('\n');
        openai_log_cancelled();
        return;
    }

    if (!openai_plan_confirm_text(answer)) {
        (void)puts("Saved plan approval cancelled.");
        openai_log_cancelled();
        return;
    }

    if (!openai_plan_approve_file(
            OPENAI_PLAN_FILE)) {
        (void)puts(
            "Saved plan approval failed integrity validation.");
        openai_log_failed();
        return;
    }

    (void)puts(
        "Saved plan approved for execution in this session.");
    openai_log_event(
        "AGENT/APPROVE",
        "plan_approved",
        1);
}


static int openai_plan_consumed(
    const char *plan_path)
{
    FILE *file;
    char line[128];
    int consumed;

    file = fopen(plan_path, "r");

    if (file == NULL) {
        return 0;
    }

    consumed = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "status=consumed", 15U) == 0) {
            consumed = 1;
            break;
        }
    }

    (void)fclose(file);
    return consumed;
}

void openai_plan_recovery_sync(
    const char *plan_path)
{
    unsigned long value;

    if (!openai_plan_approved) {
        return;
    }

    if (openai_plan_consumed(plan_path)) {
        openai_plan_approval_clear();
        return;
    }

    if (!openai_plan_checksum_valid(plan_path) ||
        !openai_approval_value(plan_path, &value) ||
        value != openai_approved_hash) {
        openai_approval_invalidate();
    }
}

void openai_m143_log_reset(void)
{
    openai_inv_log_count = 0U;
    openai_cancel_log_count = 0U;
    openai_fail_log_count = 0U;
}

unsigned int openai_m143_inv_logs(void)
{
    return openai_inv_log_count;
}

unsigned int openai_m143_cancel_logs(void)
{
    return openai_cancel_log_count;
}

unsigned int openai_m143_fail_logs(void)
{
    return openai_fail_log_count;
}

void openai_m143_emit_cancel(void)
{
    openai_log_cancelled();
}

void openai_m143_emit_fail(void)
{
    openai_log_failed();
}


void openai_plan_approval_consume(void)
{
    openai_plan_approval_clear();
}


void openai_plan_session_reset(void)
{
    openai_plan_approval_clear();
}
