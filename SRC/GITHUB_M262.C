#include <stdio.h>
#include <string.h>

#include "gh_auth.h"
#include "github_m262.h"
#include "llm_config.h"
#include "llm_internal.h"

static int m262_blank(const char *text)
{
    if (text == NULL) return 1;
    while (*text == ' ' || *text == '\t') ++text;
    return *text == '\0';
}

static int m262_net_op(const char *operation)
{
    return operation != NULL &&
        (strcmp(operation, "fetch") == 0 ||
         strcmp(operation, "pull") == 0 ||
         strcmp(operation, "push") == 0 ||
         strcmp(operation, "clone") == 0);
}

static void m262_active_note(void)
{
    const gh_profile *profile = gh_prof_active();
    if (profile == NULL) {
        (void)puts("Active GitHub profile: not configured.");
        return;
    }
    (void)printf("Active GitHub profile: %s\n", profile->name);
    (void)printf("Repository: %s\n", profile->repo);
    (void)printf("Branch: %s\n", profile->branch);
    (void)printf("Token: %s\n",
        profile->token[0] != '\0' ? "configured" : "not configured");
}

void m262_show_github(const char *operation, const char *arguments)
{
    const gh_profile *profile;
    const char *run_args;
    char defaults[GH_PROF_REPO_MAX + GH_PROF_BRANCH_MAX + 32U];
    char output[4096];
    int written;

    if (operation == NULL || *operation == '\0') return;

    if (!m262_net_op(operation)) {
        llm_show_github(operation, arguments);
        if (strcmp(operation, "help") == 0) m262_active_note();
        return;
    }

    profile = gh_prof_active();
    run_args = arguments;
    if (m262_blank(arguments) && profile != NULL) {
        if (strcmp(operation, "fetch") == 0) {
            run_args = "origin";
        } else if (strcmp(operation, "pull") == 0 ||
                   strcmp(operation, "push") == 0) {
            written = snprintf(defaults, sizeof(defaults),
                               "origin %s", profile->branch);
            if (written < 0 || (size_t)written >= sizeof(defaults)) {
                (void)puts("GitHub profile branch is too long.");
                return;
            }
            run_args = defaults;
        } else if (strcmp(operation, "clone") == 0) {
            written = snprintf(defaults, sizeof(defaults),
                               "https://github.com/%s.git", profile->repo);
            if (written < 0 || (size_t)written >= sizeof(defaults)) {
                (void)puts("GitHub profile repository name is too long.");
                return;
            }
            run_args = defaults;
        }
    }

    if (!llm_gh_run_text(operation, run_args, gh_saved_exec,
                            NULL, output, sizeof(output))) {
        if (output[0] != '\0') {
            (void)fputs(output, stdout);
        } else if (strcmp(operation, "clone") == 0) {
            (void)puts("Usage: AGENT/GITHUB/CLONE github-url [directory]");
        } else if (strcmp(operation, "pull") == 0 ||
                   strcmp(operation, "push") == 0) {
            (void)printf("Usage: AGENT/GITHUB/%s remote branch\n",
                strcmp(operation, "pull") == 0 ? "PULL" : "PUSH");
        } else {
            (void)puts("GitHub operation failed or arguments were invalid.");
        }
        return;
    }

    (void)fputs(output, stdout);
}
