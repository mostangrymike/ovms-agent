#include <stdio.h>
#include <string.h>
#include "llm_internal.h"
#include "command_internal.h"

int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 1;
}

int command_read_stream(FILE *stream, char *output, size_t output_size)
{
    (void)stream; (void)output; (void)output_size;
    return 0;
}

static int has(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static int mock_exec(const char *operation,
                     const char *arguments,
                     char *result, size_t result_size,
                     void *context)
{
    int written;
    (void)context;
    written = snprintf(result, result_size, "mock %s %s\n", operation, arguments);
    return written >= 0 && (size_t)written < result_size;
}

int main(void)
{
    char output[4096];
    const command_entry *entry;

    llm_test_reset_approval();
    if (!llm_github_text(output, sizeof(output)) ||
        !has(output, "AGENT/GITHUB/PUSH") ||
        !has(output, "AGENT/GITHUB/CHECK") ||
        !has(output, "SHARE") ||
        !has(output, "OVMS_AGENT_GITHUB_BRIDGE")) {
        (void)puts("M250 failed: GitHub catalog.");
        return 2;
    }
    if (!llm_gh_run_text("status", "", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "Status: success")) {
        (void)puts("M250 failed: read-only Git status.");
        return 2;
    }
    if (!llm_gh_run_text("check", "", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "mock check")) {
        (void)puts("M250 failed: GitHub preflight dispatch.");
        return 2;
    }
    if (!llm_gh_run_text("fetch", "origin", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "WORKSPACE approval policy is required")) {
        (void)puts("M250 failed: fetch approval refusal.");
        return 2;
    }
    if (!llm_set_approval("workspace")) {
        (void)puts("M250 failed: workspace approval setup.");
        return 2;
    }
    if (!llm_gh_run_text("fetch", "origin main", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "mock fetch origin main")) {
        (void)puts("M250 failed: fetch execution.");
        return 2;
    }
    if (!llm_gh_run_text("clone", "https://github.com/openai/codex.git TEST",
                            mock_exec, NULL, output, sizeof(output)) ||
        !has(output, "mock clone")) {
        (void)puts("M250 failed: GitHub clone validation.");
        return 2;
    }
    if (llm_gh_run_text("clone", "https://example.com/nope.git TEST",
                           mock_exec, NULL, output, sizeof(output))) {
        (void)puts("M250 failed: non-GitHub clone accepted.");
        return 2;
    }
    if (!llm_gh_run_text("push", "origin main", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "FULL approval policy is required")) {
        (void)puts("M250 failed: push approval refusal.");
        return 2;
    }
    if (!llm_set_approval("full")) {
        (void)puts("M250 failed: full approval setup.");
        return 2;
    }
    if (!llm_gh_run_text("push", "origin main", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "mock push origin main")) {
        (void)puts("M250 failed: push execution.");
        return 2;
    }
    if (!llm_gh_run_text("issues", "list open", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "mock issues list open")) {
        (void)puts("M250 failed: issue bridge dispatch.");
        return 2;
    }
    if (!llm_gh_run_text("pr", "list", mock_exec, NULL,
                            output, sizeof(output)) ||
        !has(output, "mock pr list")) {
        (void)puts("M250 failed: PR bridge dispatch.");
        return 2;
    }

    entry = command_find("AGENT/GITHUB");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M250 failed: AGENT/GITHUB registration.");
        return 2;
    }
    entry = command_find("AGENT/GITHUB/CHECK");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M250 failed: AGENT/GITHUB/CHECK registration.");
        return 2;
    }
    entry = command_find("AGENT/GITHUB/PUSH");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M250 failed: AGENT/GITHUB/PUSH registration.");
        return 2;
    }
    entry = command_find("AGENT/GITHUB/PR");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M250 failed: AGENT/GITHUB/PR registration.");
        return 2;
    }

    llm_test_reset_approval();
    (void)puts("M250 guarded GitHub regression test passed.");
    return 1;
}
