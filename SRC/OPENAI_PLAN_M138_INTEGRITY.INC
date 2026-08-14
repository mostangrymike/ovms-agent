void openai_plan_approval_clear(void);
void openai_plan_recovery_sync(const char *plan_path);

static char *openai_m138_read_text(
    const char *path)
{
    FILE *file;
    long length;
    char *text;
    size_t read_count;

    if (path == NULL || *path == '\0') {
        return NULL;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);

    if (length < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);

    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    read_count = fread(
        text,
        1U,
        (size_t)length,
        file);

    if (fclose(file) != 0 ||
        read_count != (size_t)length) {
        free(text);
        return NULL;
    }

    text[read_count] = '\0';
    return text;
}

static unsigned long openai_plan_fnv1a(
    const unsigned char *data,
    size_t length)
{
    unsigned long value;
    size_t index;

    value = 2166136261UL;

    for (index = 0U; index < length; ++index) {
        value ^= (unsigned long)data[index];
        value *= 16777619UL;
        value &= 0xffffffffUL;
    }

    return value;
}

static int openai_plan_checksum_path(
    const char *plan_path,
    char *checksum_path,
    size_t checksum_size)
{
    int count;

    if (plan_path == NULL ||
        checksum_path == NULL ||
        checksum_size == 0U) {
        return 0;
    }

    count = snprintf(
        checksum_path,
        checksum_size,
        "%s.CHK",
        plan_path);

    return count > 0 &&
        (size_t)count < checksum_size;
}

static int openai_plan_checksum_temp(
    const char *plan_path,
    char *temp_path,
    size_t temp_size)
{
    int count;

    if (plan_path == NULL ||
        temp_path == NULL ||
        temp_size == 0U) {
        return 0;
    }

    count = snprintf(
        temp_path,
        temp_size,
        "%s.CHK_NEW",
        plan_path);

    return count > 0 &&
        (size_t)count < temp_size;
}

static int openai_plan_file_hash(
    const char *plan_path,
    unsigned long *value_out)
{
    char *text;

    if (plan_path == NULL ||
        value_out == NULL) {
        return 0;
    }

    text = openai_m138_read_text(plan_path);

    if (text == NULL) {
        return 0;
    }

    *value_out = openai_plan_fnv1a(
        (const unsigned char *)text,
        strlen(text));

    free(text);
    return 1;
}

int openai_plan_checksum_write(
    const char *plan_path)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];
    char temp_path[OPENAI_PLAN_PATH_SIZE + 16];
    char old_path[OPENAI_PLAN_PATH_SIZE + 16];
    unsigned long value;
    FILE *file;
    int success;
    int had_old;
    int count;

    if (!openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path)) ||
        !openai_plan_checksum_temp(
            plan_path,
            temp_path,
            sizeof(temp_path)) ||
        !openai_plan_file_hash(
            plan_path,
            &value)) {
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

    while (remove(temp_path) == 0) {
    }
    while (remove(old_path) == 0) {
    }

    file = fopen(temp_path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(file, "%08lX\n", value) >= 0;

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        while (remove(temp_path) == 0) {
        }
        return 0;
    }

    had_old = rename(
        checksum_path,
        old_path) == 0;

    if (rename(temp_path, checksum_path) != 0) {
        if (had_old) {
            (void)rename(
                old_path,
                checksum_path);
        }

        while (remove(temp_path) == 0) {
        }
        return 0;
    }

    while (remove(old_path) == 0) {
    }
    return 1;
}

int openai_plan_checksum_valid(
    const char *plan_path)
{
    char checksum_path[OPENAI_PLAN_PATH_SIZE + 16];
    char line[64];
    unsigned long expected;
    unsigned long actual;
    char extra;
    FILE *file;

    if (!openai_plan_checksum_path(
            plan_path,
            checksum_path,
            sizeof(checksum_path)) ||
        !openai_plan_file_hash(
            plan_path,
            &actual)) {
        return 0;
    }

    file = fopen(checksum_path, "r");

    if (file == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        fclose(file) != 0 ||
        sscanf(line, "%lx %c", &expected, &extra) != 1) {
        return 0;
    }

    return (expected & 0xffffffffUL) ==
        (actual & 0xffffffffUL);
}

static int openai_plan_restore_old(void)
{
    const char *old_plan = "OVMS_AGENT_PLAN_SAVE_OLD.TXT";
    const char *old_check = "OVMS_AGENT_PLAN_SAVE_OLD.CHK";
    const char *check_path = "OVMS_AGENT_PLAN.TXT.CHK";

    while (remove(OPENAI_PLAN_FILE) == 0) {
    }
    while (remove(check_path) == 0) {
    }

    if (rename(old_plan, OPENAI_PLAN_FILE) != 0) {
        return 0;
    }

    if (rename(old_check, check_path) != 0) {
        return 0;
    }

    return 1;
}

static int openai_m195_check_begin_order(
    const char *plan_text)
{
    const char *cursor;
    const char *line_start;
    int saw_count;

    if (plan_text == NULL) {
        return 0;
    }

    saw_count = 0;
    line_start = plan_text;
    cursor = plan_text;

    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            const char *content_end;
            size_t content_length;

            content_end = cursor;

            if (content_end > line_start &&
                content_end[-1] == '\r') {
                --content_end;
            }

            content_length =
                (size_t)(content_end - line_start);

            if (content_length >= 15U &&
                memcmp(
                    line_start,
                    "operation_count=",
                    15U) == 0) {
                saw_count = 1;
            } else if (content_length == 15U &&
                       memcmp(
                           line_start,
                           "BEGIN_OPERATION",
                           15U) == 0) {
                if (!saw_count) {
                    (void)puts(
                        "Implementation plan was not saved because "
                        "an operation block appears before "
                        "operation_count.");
                    return 0;
                }
            }

            if (*cursor == '\0') {
                break;
            }

            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    return 1;
}

#define OPENAI_M251_TEXT_LIMIT 4096U

static int openai_m251_payload_ok(
    const char *plan_text)
{
    static const char begin_old[] = "BEGIN_OLD_TEXT";
    static const char end_old[] = "END_OLD_TEXT";
    static const char begin_new[] = "BEGIN_NEW_TEXT";
    static const char end_new[] = "END_NEW_TEXT";
    const char *cursor;
    const char *line_start;
    int in_old;
    int in_new;
    size_t used;

    if (plan_text == NULL) {
        return 0;
    }

    cursor = plan_text;
    line_start = plan_text;
    in_old = 0;
    in_new = 0;
    used = 0U;

    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            const char *content_end;
            const char *payload;
            size_t content_length;
            size_t payload_length;
            size_t index;

            content_end = cursor;

            if (content_end > line_start &&
                content_end[-1] == '\r') {
                --content_end;
            }

            content_length =
                (size_t)(content_end - line_start);

            if (!in_old && !in_new &&
                content_length == sizeof(begin_old) - 1U &&
                memcmp(
                    line_start,
                    begin_old,
                    sizeof(begin_old) - 1U) == 0) {
                in_old = 1;
                used = 0U;
            } else if (!in_old && !in_new &&
                       content_length == sizeof(begin_new) - 1U &&
                       memcmp(
                           line_start,
                           begin_new,
                           sizeof(begin_new) - 1U) == 0) {
                in_new = 1;
                used = 0U;
            } else if (in_old &&
                       content_length == sizeof(end_old) - 1U &&
                       memcmp(
                           line_start,
                           end_old,
                           sizeof(end_old) - 1U) == 0) {
                in_old = 0;
                used = 0U;
            } else if (in_new &&
                       content_length == sizeof(end_new) - 1U &&
                       memcmp(
                           line_start,
                           end_new,
                           sizeof(end_new) - 1U) == 0) {
                in_new = 0;
                used = 0U;
            } else if (in_old || in_new) {
                payload = line_start;
                payload_length = content_length;

                if (in_old &&
                    payload_length == sizeof(end_old) &&
                    payload[0] == '\\' &&
                    memcmp(
                        payload + 1,
                        end_old,
                        sizeof(end_old) - 1U) == 0) {
                    ++payload;
                    --payload_length;
                } else if (in_new &&
                           payload_length == sizeof(end_new) &&
                           payload[0] == '\\' &&
                           memcmp(
                               payload + 1,
                               end_new,
                               sizeof(end_new) - 1U) == 0) {
                    ++payload;
                    --payload_length;
                }

                index = 0U;

                while (index < payload_length &&
                       (payload[index] == ' ' ||
                        payload[index] == '\t')) {
                    ++index;
                }

                if (payload_length - index >= 3U &&
                    payload[index] == '`' &&
                    payload[index + 1U] == '`' &&
                    payload[index + 2U] == '`') {
                    (void)puts(
                        "Implementation plan was not saved because "
                        "an OLD or NEW payload contains a Markdown fence.");
                    return 0;
                }

                if (used + payload_length + 1U >=
                    OPENAI_M251_TEXT_LIMIT) {
                    (void)puts(
                        "Implementation plan was not saved because "
                        "an OLD or NEW payload exceeds executor capacity.");
                    return 0;
                }

                used += payload_length + 1U;
            }

            if (*cursor == '\0') {
                break;
            }

            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    return 1;
}

static char *openai_m195_normalize_count(
    const char *plan_text)
{
    static const char prefix[] =
        "operation_count=";
    const char *cursor;
    const char *line_start;
    const char *count_start;
    const char *count_end;
    unsigned int actual_count;
    unsigned int count_lines;
    size_t prefix_length;
    size_t original_length;
    size_t old_length;
    size_t replacement_length;
    size_t new_length;
    size_t before_length;
    char replacement[64];
    char *normalized;
    int replacement_count;

    if (plan_text == NULL) {
        return NULL;
    }

    if (!openai_m195_check_begin_order(plan_text)) {
        return NULL;
    }

    if (!openai_execute_count_ops(
            plan_text,
            &actual_count)) {
        (void)puts(
            "Implementation plan was not saved because "
            "its operation-block markers are invalid.");
        return NULL;
    }

    if (!openai_m251_payload_ok(plan_text)) {
        return NULL;
    }

    prefix_length = strlen(prefix);
    count_lines = 0U;
    count_start = NULL;
    count_end = NULL;
    line_start = plan_text;
    cursor = plan_text;

    for (;;) {
        if (*cursor == '\n' ||
            *cursor == '\0') {
            const char *content_end;
            size_t content_length;

            content_end = cursor;

            if (content_end > line_start &&
                content_end[-1] == '\r') {
                --content_end;
            }

            content_length =
                (size_t)(content_end - line_start);

            if (content_length >= prefix_length &&
                memcmp(
                    line_start,
                    prefix,
                    prefix_length) == 0) {
                ++count_lines;

                if (count_lines == 1U) {
                    count_start = line_start;
                    count_end = content_end;
                }
            }

            if (*cursor == '\0') {
                break;
            }

            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    if (count_lines > 1U) {
        (void)puts(
            "Implementation plan was not saved because "
            "it contains multiple operation_count lines.");
        return NULL;
    }

    if (actual_count > 0U &&
        count_lines == 0U) {
        (void)puts(
            "Implementation plan was not saved because "
            "executable operations require operation_count.");
        return NULL;
    }

    original_length = strlen(plan_text);

    if (count_lines == 0U) {
        normalized =
            (char *)malloc(original_length + 1U);

        if (normalized == NULL) {
            return NULL;
        }

        (void)memcpy(
            normalized,
            plan_text,
            original_length + 1U);

        return normalized;
    }

    replacement_count = snprintf(
        replacement,
        sizeof(replacement),
        "operation_count=%u",
        actual_count);

    if (replacement_count <= 0 ||
        (size_t)replacement_count >=
            sizeof(replacement)) {
        return NULL;
    }

    replacement_length =
        (size_t)replacement_count;
    old_length =
        (size_t)(count_end - count_start);
    new_length =
        original_length -
        old_length +
        replacement_length;

    normalized =
        (char *)malloc(new_length + 1U);

    if (normalized == NULL) {
        return NULL;
    }

    before_length =
        (size_t)(count_start - plan_text);

    (void)memcpy(
        normalized,
        plan_text,
        before_length);

    (void)memcpy(
        normalized + before_length,
        replacement,
        replacement_length);

    (void)memcpy(
        normalized +
            before_length +
            replacement_length,
        count_end,
        original_length -
            (size_t)(count_end - plan_text) +
            1U);

    return normalized;
}

int openai_plan_save(
    const char *goal,
    const char *plan_text)
{
    const char *old_plan = "OVMS_AGENT_PLAN_SAVE_OLD.TXT";
    const char *old_check = "OVMS_AGENT_PLAN_SAVE_OLD.CHK";
    const char *check_path = "OVMS_AGENT_PLAN.TXT.CHK";
    char *normalized_plan;    
    int had_plan;
    int had_check;
    int result;

    normalized_plan =
        openai_m195_normalize_count(plan_text);

    if (normalized_plan == NULL) {
        return 0;
    }

    had_plan = rename(
        OPENAI_PLAN_FILE,
        old_plan) == 0;

    had_check = rename(
        check_path,
        old_check) == 0;

    result = openai_plan_save_legacy(
        goal,
        normalized_plan);

    free(normalized_plan);

    if (result) {
        result = openai_plan_checksum_write(
            OPENAI_PLAN_FILE);
    }

    if (!result) {
        while (remove(OPENAI_PLAN_FILE) == 0) {
        }
        while (remove(check_path) == 0) {
        }

        if (had_plan) {
            (void)rename(
                old_plan,
                OPENAI_PLAN_FILE);
        }

        if (had_check) {
            (void)rename(
                old_check,
                check_path);
        }

        openai_plan_recovery_sync(
            OPENAI_PLAN_FILE);
        return 0;
    }

    while (remove(old_plan) == 0) {
    }
    while (remove(old_check) == 0) {
    }

    openai_plan_approval_clear();
    return 1;
}

int openai_plan_file_current(
    const char *plan_path,
    int verbose)
{
    FILE *file;

    if (plan_path == NULL ||
        *plan_path == '\0') {
        return 0;
    }

    file = fopen(plan_path, "r");

    if (file == NULL) {
        if (verbose) {
            (void)puts(
                "No saved implementation plan is available.");
        }

        return 0;
    }

    (void)fclose(file);

    if (!openai_plan_checksum_valid(plan_path)) {
        if (verbose) {
            (void)puts(
                "Saved implementation plan failed integrity validation.");
        }

        return 0;
    }

    return openai_plan_file_current_legacy(
        plan_path,
        verbose);
}
