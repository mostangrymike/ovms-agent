void llm_log_event(
    const char *command,
    const char *event,
    int value);

int llm_plan_approved = 0;
unsigned long llm_approved_hash = 0UL;
int llm_approval_invalidated = 0;
static unsigned int llm_inv_log_count = 0U;
static unsigned int llm_cancel_log_count = 0U;
static unsigned int llm_fail_log_count = 0U;

void llm_plan_approval_clear(void)
{
    llm_plan_approved = 0;
    llm_approved_hash = 0UL;
    llm_approval_invalidated = 0;
}

static void llm_log_invalidated(void)
{
    ++llm_inv_log_count;
    llm_log_event(
        "AGENT/APPROVE",
        "approval_invalidated",
        0);
}

static void llm_log_cancelled(void)
{
    ++llm_cancel_log_count;
    llm_log_event(
        "AGENT/APPROVE",
        "approval_cancelled",
        0);
}

static void llm_log_failed(void)
{
    ++llm_fail_log_count;
    llm_log_event(
        "AGENT/APPROVE",
        "approval_failed",
        0);
}

static void llm_approval_invalidate(void)
{
    llm_plan_approved = 0;
    llm_approved_hash = 0UL;
    llm_approval_invalidated = 1;
    llm_log_invalidated();
}

static int llm_approval_value(
    const char *plan_path,
    unsigned long *value_out)
{
    char checksum_path[LLM_PLAN_PATH_SIZE + 16];
    char line[64];
    unsigned long value;
    char extra;
    FILE *file;

    if (plan_path == NULL ||
        value_out == NULL ||
        !llm_plan_checksum_path(
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

const char *llm_plan_approval_name(void)
{
    if (llm_plan_approved) {
        return "APPROVED FOR THIS SESSION";
    }

    if (llm_approval_invalidated) {
        return "INVALIDATED";
    }

    return "NOT APPROVED";
}

void llm_plan_approval_report(void)
{
    (void)printf(
        "Plan approval:             %s\n",
        llm_plan_approval_name());
}

int llm_plan_approve_file(
    const char *plan_path)
{
    unsigned long value;

    if (!llm_plan_file_current(
            plan_path,
            0) ||
        !llm_approval_value(
            plan_path,
            &value)) {
        llm_plan_approval_clear();
        return 0;
    }

    llm_approved_hash = value;
    llm_plan_approved = 1;
    llm_approval_invalidated = 0;
    return 1;
}

int llm_plan_approval_valid(
    const char *plan_path)
{
    unsigned long value;
    int was_approved;

    was_approved = llm_plan_approved;

    if (!llm_plan_approved ||
        !llm_plan_checksum_valid(plan_path) ||
        !llm_approval_value(
            plan_path,
            &value) ||
        value != llm_approved_hash) {
        if (was_approved) {
            llm_approval_invalidate();
        }

        return 0;
    }

    return 1;
}

int llm_plan_confirm_text(
    const char *text)
{
    if (text == NULL) {
        return 0;
    }

    return
        (text[0] == 'y' || text[0] == 'Y') &&
        text[1] == '\0';
}

static int llm_plan_read_confirm(
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

void llm_plan_approve(void)
{
    char answer[32];

    llm_plan_approval_clear();

    if (!llm_plan_is_current(1)) {
        (void)puts(
            "Saved plan cannot be approved. Run AGENT/PLAN again.");
        llm_log_failed();
        return;
    }

    llm_plan_show();

    (void)printf(
        "Approve this saved plan for execution in this session [y/N]? ");
    (void)fflush(stdout);

    if (!llm_plan_read_confirm(
            answer,
            sizeof(answer))) {
        (void)putchar('\n');
        llm_log_cancelled();
        return;
    }

    if (!llm_plan_confirm_text(answer)) {
        (void)puts("Saved plan approval cancelled.");
        llm_log_cancelled();
        return;
    }

    if (!llm_plan_approve_file(
            LLM_PLAN_FILE)) {
        (void)puts(
            "Saved plan approval failed integrity validation.");
        llm_log_failed();
        return;
    }

    (void)puts(
        "Saved plan approved for execution in this session.");
    llm_log_event(
        "AGENT/APPROVE",
        "plan_approved",
        1);
}


static int llm_plan_consumed(
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

void llm_plan_recovery_sync(
    const char *plan_path)
{
    unsigned long value;

    if (!llm_plan_approved) {
        return;
    }

    if (llm_plan_consumed(plan_path)) {
        llm_plan_approval_clear();
        return;
    }

    if (!llm_plan_checksum_valid(plan_path) ||
        !llm_approval_value(plan_path, &value) ||
        value != llm_approved_hash) {
        llm_approval_invalidate();
    }
}

void llm_m143_log_reset(void)
{
    llm_inv_log_count = 0U;
    llm_cancel_log_count = 0U;
    llm_fail_log_count = 0U;
}

unsigned int llm_m143_inv_logs(void)
{
    return llm_inv_log_count;
}

unsigned int llm_m143_cancel_logs(void)
{
    return llm_cancel_log_count;
}

unsigned int llm_m143_fail_logs(void)
{
    return llm_fail_log_count;
}

void llm_m143_emit_cancel(void)
{
    llm_log_cancelled();
}

void llm_m143_emit_fail(void)
{
    llm_log_failed();
}


void llm_plan_approval_consume(void)
{
    llm_plan_approval_clear();
}


void llm_plan_session_reset(void)
{
    llm_plan_approval_clear();
}
