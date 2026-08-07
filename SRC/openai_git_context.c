#include "openai_internal.h"

#define OPENAI_GIT_TEXT_MAX 8192U
#define OPENAI_GIT_LINE_MAX 512U

#define OPENAI_GIT_STATUS_FILE "OVMS_AGENT_GIT_STATUS.TXT"
#define OPENAI_GIT_DIFF_FILE   "OVMS_AGENT_GIT_DIFF.TXT"
#define OPENAI_GIT_CMD_FILE    "OVMS_AGENT_GIT_CAPTURE.COM"

static char openai_git_status[OPENAI_GIT_TEXT_MAX];
static char openai_git_diff[OPENAI_GIT_TEXT_MAX];
static int openai_git_loaded = 0;
static int openai_git_status_ok = 0;
static int openai_git_diff_ok = 0;
static int openai_git_truncated = 0;

static const char *openai_git_test_status = NULL;
static const char *openai_git_test_diff = NULL;

static void openai_git_remove(const char *path)
{
    if (path == NULL) {
        return;
    }

    while (remove(path) == 0) {
    }
}

static int openai_git_read(const char *path,
                           char *output,
                           size_t output_size,
                           int *truncated)
{
    FILE *file;
    size_t used;
    int ch;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    used = 0U;

    while ((ch = fgetc(file)) != EOF) {
        if (used + 1U < output_size) {
            if (ch != '\r') {
                output[used++] = (char)ch;
            }
        } else if (truncated != NULL) {
            *truncated = 1;
        }
    }

    if (ferror(file)) {
        (void)fclose(file);
        output[0] = '\0';
        return 0;
    }

    (void)fclose(file);
    output[used] = '\0';

    while (used > 0U &&
           (output[used - 1U] == '\n' ||
            output[used - 1U] == '\r')) {
        output[--used] = '\0';
    }

    return 1;
}

static int openai_git_capture_one(const char *subcommand,
                                  const char *output_file,
                                  char *output,
                                  size_t output_size)
{
    FILE *command;
    char dcl[256];
    int status;
    int truncated;

    openai_git_remove(OPENAI_GIT_CMD_FILE);
    openai_git_remove(output_file);

    command = fopen(OPENAI_GIT_CMD_FILE, "w");

    if (command == NULL) {
        return 0;
    }

    if (fprintf(
            command,
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT \"%s\"",
            output_file,
            subcommand) < 0) {
        (void)fclose(command);
        return 0;
    }

    if (strcmp(subcommand, "status") == 0) {
        if (fputs(" \"--short\"\n", command) == EOF) {
            (void)fclose(command);
            return 0;
        }
    } else if (strcmp(subcommand, "diff") == 0) {
        if (fputs(" \"--\"\n", command) == EOF) {
            (void)fclose(command);
            return 0;
        }
    } else {
        (void)fclose(command);
        return 0;
    }

    if (fputs("$ EXIT $STATUS\n", command) == EOF ||
        fclose(command) != 0) {
        return 0;
    }

    (void)snprintf(
        dcl, sizeof(dcl), "@%s", OPENAI_GIT_CMD_FILE
    );

    status = system(dcl);
    truncated = 0;

    if ((status & 1) == 0) {
        openai_git_remove(OPENAI_GIT_CMD_FILE);
        openai_git_remove(output_file);
        return 0;
    }

    if (!openai_git_read(
            output_file, output, output_size, &truncated)) {
        openai_git_remove(OPENAI_GIT_CMD_FILE);
        openai_git_remove(output_file);
        return 0;
    }

    if (truncated) {
        openai_git_truncated = 1;
    }

    openai_git_remove(OPENAI_GIT_CMD_FILE);
    openai_git_remove(output_file);
    return 1;
}

int openai_git_refresh(const agent_state *state)
{
    (void)state;

    openai_git_status[0] = '\0';
    openai_git_diff[0] = '\0';
    openai_git_status_ok = 0;
    openai_git_diff_ok = 0;
    openai_git_truncated = 0;

    if (openai_git_test_status != NULL ||
        openai_git_test_diff != NULL) {
        if (openai_git_test_status != NULL) {
            (void)strncpy(
                openai_git_status,
                openai_git_test_status,
                sizeof(openai_git_status) - 1U
            );
            openai_git_status[
                sizeof(openai_git_status) - 1U
            ] = '\0';
            openai_git_status_ok = 1;
        }

        if (openai_git_test_diff != NULL) {
            (void)strncpy(
                openai_git_diff,
                openai_git_test_diff,
                sizeof(openai_git_diff) - 1U
            );
            openai_git_diff[
                sizeof(openai_git_diff) - 1U
            ] = '\0';
            openai_git_diff_ok = 1;
        }

        openai_git_loaded = 1;
        return 1;
    }

    openai_git_status_ok = openai_git_capture_one(
        "status",
        OPENAI_GIT_STATUS_FILE,
        openai_git_status,
        sizeof(openai_git_status)
    );

    openai_git_diff_ok = openai_git_capture_one(
        "diff",
        OPENAI_GIT_DIFF_FILE,
        openai_git_diff,
        sizeof(openai_git_diff)
    );

    openai_git_loaded = 1;

    return openai_git_status_ok || openai_git_diff_ok;
}

static int openai_git_ensure(const agent_state *state)
{
    if (!openai_git_loaded) {
        return openai_git_refresh(state);
    }

    return 1;
}

static unsigned int openai_git_changed_count(void)
{
    const char *pos;
    unsigned int count;

    if (!openai_git_status_ok ||
        openai_git_status[0] == '\0') {
        return 0U;
    }

    count = 1U;

    for (pos = openai_git_status; *pos != '\0'; ++pos) {
        if (*pos == '\n') {
            ++count;
        }
    }

    return count;
}

int openai_git_status_text(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_git_ensure(state)) {
        return 0;
    }

    if (!openai_git_status_ok) {
        written = snprintf(
            output, output_size,
            "OVMS Agent Git status\n"
            "---------------------\n"
            "Git status capture unavailable.\n"
        );
    } else {
        written = snprintf(
            output, output_size,
            "OVMS Agent Git status\n"
            "---------------------\n"
            "Changed paths: %u\n"
            "%s%s\n",
            openai_git_changed_count(),
            openai_git_status[0] != '\0' ?
                openai_git_status : "(clean)",
            openai_git_truncated ?
                "\n[output truncated]" : ""
        );
    }

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_git_diff_text(const agent_state *state,
                         char *output,
                         size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_git_ensure(state)) {
        return 0;
    }

    if (!openai_git_diff_ok) {
        written = snprintf(
            output, output_size,
            "OVMS Agent Git diff\n"
            "-------------------\n"
            "Git diff capture unavailable.\n"
        );
    } else {
        written = snprintf(
            output, output_size,
            "OVMS Agent Git diff\n"
            "-------------------\n"
            "%s%s\n",
            openai_git_diff[0] != '\0' ?
                openai_git_diff : "(no unstaged diff)",
            openai_git_truncated ?
                "\n[output truncated]" : ""
        );
    }

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_git_changed_text(const agent_state *state,
                            char *output,
                            size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_git_ensure(state)) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent changed paths\n"
        "------------------------\n"
        "Count: %u\n"
        "%s\n",
        openai_git_changed_count(),
        openai_git_status_ok &&
        openai_git_status[0] != '\0' ?
            openai_git_status : "(clean)"
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_git_context(const agent_state *state,
                       char *output,
                       size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_git_ensure(state)) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "GIT WORKING TREE\n"
        "----------------\n"
        "Changed paths: %u\n"
        "%s\n\n"
        "UNSTAGED DIFF\n"
        "-------------\n"
        "%s%s\n",
        openai_git_changed_count(),
        openai_git_status_ok &&
        openai_git_status[0] != '\0' ?
            openai_git_status : "(clean)",
        openai_git_diff_ok &&
        openai_git_diff[0] != '\0' ?
            openai_git_diff : "(none)",
        openai_git_truncated ?
            "\n[Git context truncated]" : ""
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_git_compose(const agent_state *state,
                       const char *goal,
                       char *output,
                       size_t output_size)
{
    char git_context[OPENAI_GIT_TEXT_MAX * 2U];
    int written;

    if (goal == NULL || *goal == '\0' ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (!openai_git_context(
            state, git_context, sizeof(git_context))) {
        written = snprintf(
            output, output_size, "%s", goal
        );
        return written >= 0 &&
               (size_t)written < output_size;
    }

    written = snprintf(
        output, output_size,
        "%s\n"
        "MODEL TASK CONTEXT\n"
        "------------------\n"
        "%s",
        git_context,
        goal
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

void openai_show_git_status(const agent_state *state)
{
    char output[OPENAI_GIT_TEXT_MAX + 256U];

    if (!openai_git_status_text(
            state, output, sizeof(output))) {
        (void)puts("Unable to show Git status context.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_git_diff(const agent_state *state)
{
    char output[OPENAI_GIT_TEXT_MAX + 256U];

    if (!openai_git_diff_text(
            state, output, sizeof(output))) {
        (void)puts("Unable to show Git diff context.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_git_changed(const agent_state *state)
{
    char output[OPENAI_GIT_TEXT_MAX + 256U];

    if (!openai_git_changed_text(
            state, output, sizeof(output))) {
        (void)puts("Unable to show changed paths.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_git_context(const agent_state *state)
{
    char output[OPENAI_GIT_TEXT_MAX * 2U + 512U];

    if (!openai_git_context(
            state, output, sizeof(output))) {
        (void)puts("Unable to show Git context.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_git_refresh_cmd(const agent_state *state)
{
    if (!openai_git_refresh(state)) {
        (void)puts("Git context refresh failed.");
        return;
    }

    (void)printf(
        "Git context refreshed: %u changed path%s.\n",
        openai_git_changed_count(),
        openai_git_changed_count() == 1U ? "" : "s"
    );
}

void openai_test_git_data(const char *status_text,
                          const char *diff_text)
{
    openai_git_test_status = status_text;
    openai_git_test_diff = diff_text;
    openai_git_loaded = 0;
}
