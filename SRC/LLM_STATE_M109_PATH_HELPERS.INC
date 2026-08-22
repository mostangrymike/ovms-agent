#include <limits.h>
#include <unistd.h>

#define LLM_STATE_PATH_SIZE 512U
#define LLM_STATE_COMMAND_SIZE 1024U

#define LLM_STATE_EXPECT_FORMAT 0
#define LLM_STATE_EXPECT_WORKFLOW 1
#define LLM_STATE_EXPECT_BUILD_KNOWN 2
#define LLM_STATE_EXPECT_BUILD_STATUS 3
#define LLM_STATE_EXPECT_ROLLBACK 4
#define LLM_STATE_EXPECT_END 5

static int llm_state_values_valid(
    int workflow,
    int build_known,
    int build_status,
    int rollback)
{
    return
        workflow >= LLM_WORKFLOW_NONE &&
        workflow <= LLM_WORKFLOW_EXECUTE &&
        (build_known == 0 || build_known == 1) &&
        build_status >= 0 &&
        !(build_known == 0 && build_status != 0) &&
        rollback >= LLM_ROLLBACK_NONE &&
        rollback <= LLM_ROLLBACK_DECLINED;
}

static int llm_state_line_complete(
    FILE *file,
    const char *line)
{
    const char *cursor;

    if (file == NULL || line == NULL) {
        return 0;
    }

    cursor = line;

    while (*cursor != '\0') {
        if (*cursor == '\n' || *cursor == '\r') {
            return 1;
        }

        ++cursor;
    }

    return feof(file) != 0;
}

static int llm_state_parse_uint(
    const char *text,
    int *value_out)
{
    const char *cursor;
    int value;

    if (text == NULL ||
        value_out == NULL ||
        *text == '\0') {
        return 0;
    }

    if (text[0] == '0' &&
        text[1] >= '0' &&
        text[1] <= '9') {
        return 0;
    }

    cursor = text;
    value = 0;

    while (*cursor >= '0' &&
           *cursor <= '9') {
        int digit;

        digit = *cursor - '0';

        if (value > (INT_MAX - digit) / 10) {
            return 0;
        }

        value = value * 10 + digit;
        ++cursor;
    }

    if (cursor == text) {
        return 0;
    }

    if (*cursor == '\0') {
        *value_out = value;
        return 1;
    }

    if (*cursor == '\n' &&
        cursor[1] == '\0') {
        *value_out = value;
        return 1;
    }

    if (*cursor == '\r' &&
        cursor[1] == '\n' &&
        cursor[2] == '\0') {
        *value_out = value;
        return 1;
    }

    return 0;
}

static int llm_state_parse_int_line(
    const char *line,
    const char *key,
    int *value_out)
{
    size_t key_length;

    if (line == NULL ||
        key == NULL ||
        value_out == NULL) {
        return 0;
    }

    key_length = strlen(key);

    if (strncmp(line, key, key_length) != 0 ||
        line[key_length] != '=') {
        return 0;
    }

    return llm_state_parse_uint(
        line + key_length + 1U,
        value_out);
}

static int llm_state_purge_all(
    const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    while (remove(path) == 0) {
    }

    return errno == ENOENT;
}

static void llm_state_purge_file(
    const char *path)
{
    (void)llm_state_purge_all(path);
}

static int llm_state_prune_path_safe(
    const char *path)
{
    const unsigned char *cursor;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    cursor = (const unsigned char *)path;

    while (*cursor != (unsigned char)'\0') {
        if (!isalnum((int)*cursor) &&
            *cursor != (unsigned char)'_' &&
            *cursor != (unsigned char)'-' &&
            *cursor != (unsigned char)'.' &&
            *cursor != (unsigned char)'$') {
            return 0;
        }

        ++cursor;
    }

    return 1;
}

static int llm_state_prune_versions(
    const char *path)
{
    char command[LLM_STATE_COMMAND_SIZE];
    int count;
    int status;

    if (!llm_state_prune_path_safe(path)) {
        return 0;
    }

    count = snprintf(
        command,
        sizeof(command),
        "PURGE/KEEP=2/NOLOG %s",
        path);

    if (count < 0 ||
        (size_t)count >= sizeof(command)) {
        return 0;
    }

    status = system(command);

    return
        status == 0 ||
        (status & 1) != 0;
}

static int llm_state_save_with_tmp(
    const char *path,
    const char *temporary_path,
    int workflow,
    int build_known,
    int build_status,
    int rollback)
{
    FILE *file;
    int success;

    if (path == NULL ||
        *path == '\0' ||
        temporary_path == NULL ||
        *temporary_path == '\0' ||
        strcmp(path, temporary_path) == 0 ||
        !llm_state_values_valid(
            workflow,
            build_known,
            build_status,
            rollback)) {
        return 0;
    }

    llm_state_purge_file(temporary_path);

    file = fopen(temporary_path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(
            file,
            "format=1\n"
            "last_workflow=%d\n"
            "last_build_known=%d\n"
            "last_build_status=%d\n"
            "last_rollback=%d\n",
            workflow,
            build_known,
            build_status,
            rollback) >= 0;

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        llm_state_purge_file(temporary_path);
        return 0;
    }

    if (rename(temporary_path, path) != 0) {
        llm_state_purge_file(temporary_path);
        return 0;
    }

    llm_state_purge_file(temporary_path);
    (void)llm_state_prune_versions(path);
    return 1;
}

static int llm_state_make_tmp_path(
    const char *destination_path,
    char *temporary_path,
    size_t temporary_size)
{
    int count;

    if (destination_path == NULL ||
        *destination_path == '\0' ||
        temporary_path == NULL ||
        temporary_size == 0U) {
        return 0;
    }

    count = snprintf(
        temporary_path,
        temporary_size,
        "%s_%lu.TMP",
        destination_path,
        (unsigned long)getpid());

    return
        count >= 0 &&
        (size_t)count < temporary_size;
}

static int llm_state_save_to(
    const char *path,
    int workflow,
    int build_known,
    int build_status,
    int rollback)
{
    char temporary_path[LLM_STATE_PATH_SIZE];

    if (!llm_state_make_tmp_path(
            path,
            temporary_path,
            sizeof(temporary_path))) {
        return 0;
    }

    return llm_state_save_with_tmp(
        path,
        temporary_path,
        workflow,
        build_known,
        build_status,
        rollback);
}

static int llm_state_load_from(
    const char *path,
    int *workflow_out,
    int *build_known_out,
    int *build_status_out,
    int *rollback_out)
{
    FILE *file;
    char line[256];
    int format;
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int expected_field;
    int valid;

    if (path == NULL ||
        *path == '\0' ||
        workflow_out == NULL ||
        build_known_out == NULL ||
        build_status_out == NULL ||
        rollback_out == NULL) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    format = 0;
    workflow = LLM_WORKFLOW_NONE;
    build_known = 0;
    build_status = 0;
    rollback = LLM_ROLLBACK_NONE;
    expected_field = LLM_STATE_EXPECT_FORMAT;
    valid = 1;

    while (valid &&
           fgets(line, sizeof(line), file) != NULL) {
        int value;

        if (!llm_state_line_complete(file, line)) {
            valid = 0;
        } else if (
            expected_field == LLM_STATE_EXPECT_FORMAT &&
            strncmp(line, "format=", 7U) == 0 &&
            llm_state_parse_int_line(
                line,
                "format",
                &value)) {
            format = value;
            expected_field = LLM_STATE_EXPECT_WORKFLOW;
        } else if (
            expected_field == LLM_STATE_EXPECT_WORKFLOW &&
            strncmp(
                line,
                "last_workflow=",
                14U) == 0 &&
            llm_state_parse_int_line(
                line,
                "last_workflow",
                &value)) {
            workflow = value;
            expected_field = LLM_STATE_EXPECT_BUILD_KNOWN;
        } else if (
            expected_field == LLM_STATE_EXPECT_BUILD_KNOWN &&
            strncmp(
                line,
                "last_build_known=",
                17U) == 0 &&
            llm_state_parse_int_line(
                line,
                "last_build_known",
                &value)) {
            build_known = value;
            expected_field = LLM_STATE_EXPECT_BUILD_STATUS;
        } else if (
            expected_field == LLM_STATE_EXPECT_BUILD_STATUS &&
            strncmp(
                line,
                "last_build_status=",
                18U) == 0 &&
            llm_state_parse_int_line(
                line,
                "last_build_status",
                &value)) {
            build_status = value;
            expected_field = LLM_STATE_EXPECT_ROLLBACK;
        } else if (
            expected_field == LLM_STATE_EXPECT_ROLLBACK &&
            strncmp(
                line,
                "last_rollback=",
                14U) == 0 &&
            llm_state_parse_int_line(
                line,
                "last_rollback",
                &value)) {
            rollback = value;
            expected_field = LLM_STATE_EXPECT_END;
        } else {
            valid = 0;
        }
    }

    (void)fclose(file);

    if (!valid ||
        expected_field != LLM_STATE_EXPECT_END ||
        format != 1 ||
        !llm_state_values_valid(
            workflow,
            build_known,
            build_status,
            rollback)) {
        return 0;
    }

    *workflow_out = workflow;
    *build_known_out = build_known;
    *build_status_out = build_status;
    *rollback_out = rollback;
    return 1;
}

static int llm_state_load_recovery_flag(
    const char *path,
    int *workflow_out,
    int *build_known_out,
    int *build_status_out,
    int *rollback_out,
    int *recovered_out)
{
    char recovery_path[LLM_STATE_PATH_SIZE];
    int count;

    if (recovered_out == NULL) {
        return 0;
    }

    *recovered_out = 0;

    if (llm_state_load_from(
            path,
            workflow_out,
            build_known_out,
            build_status_out,
            rollback_out)) {
        return 1;
    }

    if (path == NULL || *path == '\0') {
        return 0;
    }

    count = snprintf(
        recovery_path,
        sizeof(recovery_path),
        "%s;-1",
        path);

    if (count < 0 ||
        (size_t)count >= sizeof(recovery_path)) {
        return 0;
    }

    if (!llm_state_load_from(
            recovery_path,
            workflow_out,
            build_known_out,
            build_status_out,
            rollback_out)) {
        return 0;
    }

    *recovered_out = 1;
    return 1;
}

static int llm_state_load_recovery(
    const char *path,
    int *workflow_out,
    int *build_known_out,
    int *build_status_out,
    int *rollback_out)
{
    int recovered;

    return llm_state_load_recovery_flag(
        path,
        workflow_out,
        build_known_out,
        build_status_out,
        rollback_out,
        &recovered);
}

static void llm_state_snapshot_current(void)
{
    llm_saved_workflow = llm_last_workflow;
    llm_saved_build_known = llm_last_build_known;
    llm_saved_build_status = llm_last_build_status;
    llm_saved_rollback = llm_last_rollback;
}

static void llm_state_snapshot_clear(void)
{
    llm_saved_workflow = LLM_WORKFLOW_NONE;
    llm_saved_build_known = 0;
    llm_saved_build_status = 0;
    llm_saved_rollback = LLM_ROLLBACK_NONE;
}

static void llm_state_reset_memory(void)
{
    llm_last_workflow = LLM_WORKFLOW_NONE;
    llm_last_build_known = 0;
    llm_last_build_status = 0;
    llm_last_rollback = LLM_ROLLBACK_NONE;

    llm_state_snapshot_clear();

    llm_state_valid = 0;
    llm_state_recovered = 0;
    llm_state_save_known = 0;
    llm_state_save_succeeded = 0;
}

static int llm_state_apply_load(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int recovered;

    llm_state_loaded = 1;
    llm_state_reset_memory();

    if (!llm_state_load_recovery_flag(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback,
            &recovered)) {
        return 0;
    }

    llm_last_workflow = workflow;
    llm_last_build_known = build_known;
    llm_last_build_status = build_status;
    llm_last_rollback = rollback;
    llm_state_snapshot_current();
    llm_state_recovered = recovered;
    llm_state_valid = 1;
    return 1;
}



const char *llm_state_source_name(
    int valid,
    int recovered)
{
    if (!valid) {
        return "unavailable";
    }

    return recovered ?
        "previous version recovery" :
        "newest version";
}


const char *llm_persist_workflow(
    int valid,
    int workflow)
{
    if (!valid) {
        return "unavailable";
    }

    return llm_workflow_name(workflow);
}

const char *llm_persist_build(
    int valid,
    int build_known,
    int build_status)
{
    if (!valid) {
        return "unavailable";
    }

    if (!build_known) {
        return "not recorded";
    }

    return (build_status & 1) != 0 ?
        "success" :
        "failure";
}

const char *llm_persist_rollback(
    int valid,
    int rollback)
{
    if (!valid) {
        return "unavailable";
    }

    return llm_rollback_name(rollback);
}


const char *llm_state_save_name(
    int known,
    int succeeded)
{
    if (!known) {
        return "not attempted";
    }

    return succeeded ?
        "success" :
        "failure";
}
