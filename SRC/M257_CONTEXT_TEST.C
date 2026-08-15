#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define openai_agent_mode m257_capture_mode
#include "OPENAI_AGENT.C"
#undef openai_agent_mode

#define ROOT_FILE "M257_CTX_ROOT.TXT"
#define SCOPE_FILE "TEST/OVMS_AGENT_INSTRUCTIONS.TXT"

static char m257_goal[32768];
static int m257_allow_write;
static int m257_build_after;
static int m257_workflow;
static int m257_calls;

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
}

static int write_text(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) return 0;
    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int copy_context(const char *input,
                        char *output,
                        size_t output_size)
{
    int written;

    if (input == NULL || output == NULL || output_size == 0U) return 0;
    written = snprintf(output, output_size, "%s", input);
    return written >= 0 && (size_t)written < output_size;
}

int openai_project_compose(const agent_state *state,
                           const char *goal,
                           char *output,
                           size_t output_size)
{
    (void)state;
    return copy_context(goal, output, output_size);
}

int openai_git_compose(const agent_state *state,
                       const char *goal,
                       char *output,
                       size_t output_size)
{
    (void)state;
    return copy_context(goal, output, output_size);
}

void m257_capture_mode(agent_state *state,
                       const char *goal,
                       int allow_write,
                       int build_after_write,
                       int workflow)
{
    (void)state;
    ++m257_calls;
    m257_allow_write = allow_write;
    m257_build_after = build_after_write;
    m257_workflow = workflow;
    (void)strncpy(m257_goal, goal != NULL ? goal : "",
                  sizeof(m257_goal) - 1U);
    m257_goal[sizeof(m257_goal) - 1U] = '\0';
}

static int context_ok(const char *request)
{
    const char *root;
    const char *scope;

    root = strstr(m257_goal, "ROOT RULE: preserve root policy");
    scope = strstr(m257_goal, "SCOPED RULE: TEST policy wins");

    return root != NULL && scope != NULL && root < scope &&
           strstr(m257_goal, "INSTRUCTION PRECEDENCE") != NULL &&
           strstr(m257_goal, request) != NULL;
}

static void cleanup(void)
{
    openai_test_instr_path(NULL);
    remove_all(ROOT_FILE);
    remove_all(SCOPE_FILE);
}

int main(void)
{
    agent_state state;
    const char *request;

    cleanup();
    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";
    openai_test_instr_path(ROOT_FILE);

    if (!write_text(ROOT_FILE, "ROOT RULE: preserve root policy\n") ||
        !write_text(SCOPE_FILE, "SCOPED RULE: TEST policy wins\n") ||
        !openai_instr_reload(&state)) {
        (void)puts("M257 context failed: instruction fixture setup.");
        cleanup();
        return EXIT_FAILURE;
    }

    request = "Update TEST/M257_CTX_TARGET.C safely.";
    m257_calls = 0;
    m257_goal[0] = '\0';
    openai_agent_plan(&state, request);

    if (m257_calls != 1 ||
        m257_allow_write != 0 ||
        m257_build_after != 0 ||
        m257_workflow != OPENAI_WORKFLOW_PLAN ||
        !context_ok(request)) {
        (void)puts("M257 context failed: PLAN did not receive scoped instructions.");
        cleanup();
        return EXIT_FAILURE;
    }

    m257_calls = 0;
    m257_goal[0] = '\0';
    openai_agent_write(&state, request);

    if (m257_calls != 1 ||
        m257_allow_write != 1 ||
        m257_build_after != 0 ||
        m257_workflow != OPENAI_WORKFLOW_WRITE ||
        !context_ok(request)) {
        (void)puts("M257 context failed: WRITE did not receive scoped instructions.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M257 planning/write instruction enforcement test passed.");
    return EXIT_SUCCESS;
}
