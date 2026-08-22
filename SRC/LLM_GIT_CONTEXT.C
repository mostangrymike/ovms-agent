#include <stdio.h>
#include <string.h>

#define llm_git_refresh llm_git_refresh_base
#define llm_git_status_text llm_git_status_text_base
#define llm_git_diff_text llm_git_diff_text_base
#define llm_git_changed_text llm_git_changed_text_base
#define llm_git_context llm_git_context_base
#define llm_git_compose llm_git_compose_base
#define llm_show_git_status llm_show_git_status_base
#define llm_show_git_diff llm_show_git_diff_base
#define llm_show_git_changed llm_show_git_changed_base
#define llm_show_git_context llm_show_git_context_base
#define llm_git_refresh_cmd llm_git_refresh_cmd_base
#include "LLM_GIT_CONTEXT_M262_CORE.C"
#undef llm_git_refresh
#undef llm_git_status_text
#undef llm_git_diff_text
#undef llm_git_changed_text
#undef llm_git_context
#undef llm_git_compose
#undef llm_show_git_status
#undef llm_show_git_diff
#undef llm_show_git_changed
#undef llm_show_git_context
#undef llm_git_refresh_cmd

#define M263_GIT_HEAD_TMP "OVMS_AGENT_GIT_HEAD.TMP"
#define M263_GIT_RMS_TMP  "OVMS_AGENT_GIT_RMS.TMP"
#define M263_GIT_ONE_TMP  "OVMS_AGENT_GIT_ONE.TXT"
#define M263_GIT_ONE_COM  "OVMS_AGENT_GIT_ONE.COM"

static int m263_git_path_ok(const char *path)
{
    const unsigned char *cursor;

    if (path == NULL || *path == '\0' || path[0] == '-' ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    cursor = (const unsigned char *)path;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'/' ||
               *cursor == (unsigned char)'$')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

int llm_git_rms_copy(const char *path, const char *target)
{
    FILE *input;
    FILE *output;
    int ch;
    int ok;

    if (!m263_git_path_ok(path) || target == NULL || *target == '\0') {
        return 0;
    }

    input = fopen(path, "r");
    if (input == NULL) {
        return 0;
    }

    llm_git_remove(target);
    output = fopen(target, "w");
    if (output == NULL) {
        (void)fclose(input);
        return 0;
    }

    ok = 1;
    while ((ch = fgetc(input)) != EOF) {
        if (fputc(ch, output) == EOF) {
            ok = 0;
            break;
        }
    }

    if (ferror(input)) {
        ok = 0;
    }
    if (fclose(input) != 0) {
        ok = 0;
    }
    if (fclose(output) != 0) {
        ok = 0;
    }

    if (!ok) {
        llm_git_remove(target);
    }

    return ok;
}

static int m263_git_empty_copy(const char *target)
{
    FILE *output;

    llm_git_remove(target);
    output = fopen(target, "w");
    if (output == NULL) {
        return 0;
    }
    return fclose(output) == 0;
}

static int m263_git_head_copy(const char *path)
{
    FILE *command;
    FILE *check;
    char dcl[256];

    if (!m263_git_path_ok(path)) {
        return 0;
    }

    llm_git_remove(M263_GIT_ONE_COM);
    llm_git_remove(M263_GIT_HEAD_TMP);

    command = fopen(M263_GIT_ONE_COM, "w");
    if (command == NULL) {
        return 0;
    }

    if (fprintf(command,
            "$ SET NOON\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"show\" \"HEAD:%s\"\n"
            "$ EXIT 1\n",
            M263_GIT_HEAD_TMP, path) < 0 ||
        fclose(command) != 0) {
        llm_git_remove(M263_GIT_ONE_COM);
        return 0;
    }

    (void)snprintf(dcl, sizeof(dcl), "@%s", M263_GIT_ONE_COM);
    (void)system(dcl);
    llm_git_remove(M263_GIT_ONE_COM);

    check = fopen(M263_GIT_HEAD_TMP, "r");
    if (check == NULL) {
        llm_git_remove(M263_GIT_HEAD_TMP);
        return 0;
    }
    (void)fclose(check);
    return 1;
}

static int m263_git_one_diff(const char *path,
                             int deleted,
                             char *output, size_t output_size)
{
    FILE *command;
    char dcl[256];
    char captured[LLM_GIT_TEXT_MAX];
    int truncated;
    int written;

    if (!m263_git_path_ok(path) || output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    if (deleted) {
        if (!m263_git_empty_copy(M263_GIT_RMS_TMP)) {
            return 0;
        }
    } else if (!llm_git_rms_copy(path, M263_GIT_RMS_TMP)) {
        return 0;
    }

    if (!m263_git_head_copy(path)) {
        llm_git_remove(M263_GIT_RMS_TMP);
        return 0;
    }

    llm_git_remove(M263_GIT_ONE_COM);
    llm_git_remove(M263_GIT_ONE_TMP);
    command = fopen(M263_GIT_ONE_COM, "w");
    if (command == NULL) {
        llm_git_remove(M263_GIT_HEAD_TMP);
        llm_git_remove(M263_GIT_RMS_TMP);
        return 0;
    }

    if (fprintf(command,
            "$ SET NOON\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"diff\" \"--no-index\" \"--\" \"%s\" \"%s\"\n"
            "$ EXIT 1\n",
            M263_GIT_ONE_TMP,
            M263_GIT_HEAD_TMP,
            M263_GIT_RMS_TMP) < 0 ||
        fclose(command) != 0) {
        llm_git_remove(M263_GIT_ONE_COM);
        llm_git_remove(M263_GIT_HEAD_TMP);
        llm_git_remove(M263_GIT_RMS_TMP);
        return 0;
    }

    (void)snprintf(dcl, sizeof(dcl), "@%s", M263_GIT_ONE_COM);
    (void)system(dcl);
    llm_git_remove(M263_GIT_ONE_COM);

    truncated = 0;
    captured[0] = '\0';
    if (!llm_git_read(M263_GIT_ONE_TMP,
                         captured, sizeof(captured), &truncated)) {
        llm_git_remove(M263_GIT_ONE_TMP);
        llm_git_remove(M263_GIT_HEAD_TMP);
        llm_git_remove(M263_GIT_RMS_TMP);
        return 0;
    }

    written = snprintf(output, output_size,
        "RMS-resolved path: %s\n%s",
        path,
        captured[0] != '\0' ? captured : "(no difference)");

    llm_git_remove(M263_GIT_ONE_TMP);
    llm_git_remove(M263_GIT_HEAD_TMP);
    llm_git_remove(M263_GIT_RMS_TMP);

    if (truncated) {
        llm_git_truncated = 1;
    }

    return written >= 0 && (size_t)written < output_size;
}

static int m263_git_work_path(const char *line,
                              char *kind,
                              char *path, size_t path_size)
{
    const char *start;
    size_t length;

    if (line == NULL || kind == NULL || path == NULL || path_size == 0U) {
        return 0;
    }

    if (line[2] != ' ' ||
        !(line[1] == 'M' || line[1] == 'D' || line[1] == 'T')) {
        return 0;
    }

    start = line + 3;
    length = 0U;
    while (start[length] != '\0' && start[length] != '\n' &&
           start[length] != '\r') {
        ++length;
    }

    if (length == 0U || length >= path_size) {
        return 0;
    }

    (void)memcpy(path, start, length);
    path[length] = '\0';
    if (!m263_git_path_ok(path)) {
        return 0;
    }

    *kind = line[1];
    return 1;
}

static int m263_git_rms_diff(void)
{
    const char *cursor;
    char path[512];
    char one[LLM_GIT_TEXT_MAX];
    char kind;
    size_t used;
    int found;

    if (!llm_git_status_ok) {
        return 0;
    }

    llm_git_diff[0] = '\0';
    used = 0U;
    found = 0;
    cursor = llm_git_status;

    while (*cursor != '\0') {
        const char *next;
        size_t one_len;

        next = strchr(cursor, '\n');
        if (m263_git_work_path(cursor, &kind, path, sizeof(path)) &&
            m263_git_one_diff(path, kind == 'D', one, sizeof(one))) {
            one_len = strlen(one);
            if (used != 0U) {
                if (used + 2U >= sizeof(llm_git_diff)) {
                    llm_git_truncated = 1;
                    break;
                }
                llm_git_diff[used++] = '\n';
                llm_git_diff[used++] = '\n';
                llm_git_diff[used] = '\0';
            }
            if (one_len >= sizeof(llm_git_diff) - used) {
                one_len = sizeof(llm_git_diff) - used - 1U;
                llm_git_truncated = 1;
            }
            (void)memcpy(llm_git_diff + used, one, one_len);
            used += one_len;
            llm_git_diff[used] = '\0';
            found = 1;
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    llm_git_diff_ok = 1;
    return found || llm_git_status[0] == '\0';
}

int llm_git_refresh(const agent_state *state)
{
    (void)state;

    if (llm_git_test_status != NULL || llm_git_test_diff != NULL) {
        return llm_git_refresh_base(state);
    }

    llm_git_status[0] = '\0';
    llm_git_diff[0] = '\0';
    llm_git_status_ok = 0;
    llm_git_diff_ok = 0;
    llm_git_truncated = 0;

    llm_git_status_ok = llm_git_capture_one(
        "status",
        LLM_GIT_STATUS_FILE,
        llm_git_status,
        sizeof(llm_git_status)
    );

    if (llm_git_status_ok) {
        (void)m263_git_rms_diff();
    }

    llm_git_loaded = 1;
    return llm_git_status_ok;
}

static int m263_git_prepare(const agent_state *state)
{
    if (!llm_git_loaded) {
        return llm_git_refresh(state);
    }
    return 1;
}

int llm_git_status_text(const agent_state *state,
                        char *output, size_t output_size)
{
    if (!m263_git_prepare(state)) return 0;
    return llm_git_status_text_base(state, output, output_size);
}

int llm_git_diff_text(const agent_state *state,
                      char *output, size_t output_size)
{
    if (!m263_git_prepare(state)) return 0;
    return llm_git_diff_text_base(state, output, output_size);
}

int llm_git_changed_text(const agent_state *state,
                         char *output, size_t output_size)
{
    if (!m263_git_prepare(state)) return 0;
    return llm_git_changed_text_base(state, output, output_size);
}

int llm_git_context(const agent_state *state,
                    char *output, size_t output_size)
{
    if (!m263_git_prepare(state)) return 0;
    return llm_git_context_base(state, output, output_size);
}

int llm_git_compose(const agent_state *state,
                    const char *goal,
                    char *output, size_t output_size)
{
    if (!m263_git_prepare(state)) return 0;
    return llm_git_compose_base(state, goal, output, output_size);
}

void llm_show_git_status(const agent_state *state)
{
    if (!m263_git_prepare(state)) {
        (void)puts("Unable to show Git status context.");
        return;
    }
    llm_show_git_status_base(state);
}

void llm_show_git_diff(const agent_state *state)
{
    if (!m263_git_prepare(state)) {
        (void)puts("Unable to show Git diff context.");
        return;
    }
    llm_show_git_diff_base(state);
}

void llm_show_git_changed(const agent_state *state)
{
    if (!m263_git_prepare(state)) {
        (void)puts("Unable to show changed paths.");
        return;
    }
    llm_show_git_changed_base(state);
}

void llm_show_git_context(const agent_state *state)
{
    if (!m263_git_prepare(state)) {
        (void)puts("Unable to show Git context.");
        return;
    }
    llm_show_git_context_base(state);
}

void llm_git_refresh_cmd(const agent_state *state)
{
    if (!llm_git_refresh(state)) {
        (void)puts("Git context refresh failed.");
        return;
    }
    (void)printf(
        "Git context refreshed: %u changed path%s.\n",
        llm_git_changed_count(),
        llm_git_changed_count() == 1U ? "" : "s");
}
