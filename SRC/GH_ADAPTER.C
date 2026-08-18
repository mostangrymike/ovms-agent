#include <stdio.h>
#include <string.h>

#include "gh_adapter.h"
#include "llm_config.h"
#include "openai_internal.h"

static int gh_blank(const char *text)
{
    if (text == NULL) return 1;
    while (*text == ' ' || *text == '\t') ++text;
    return *text == '\0';
}

static void gh_show_active(const gh_profile *profile)
{
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

void gh_show(const char *operation, const char *arguments)
{
    const gh_profile *profile;
    const char *run_args;
    char defaults[GH_PROF_REPO_MAX + GH_PROF_BRANCH_MAX + 32U];
    int written;

    if (operation == NULL || *operation == '\0') return;
    profile = gh_prof_active();
    if (strcmp(operation, "help") == 0) {
        openai_show_github(operation, arguments);
        gh_show_active(profile);
        return;
    }

    run_args = arguments;
    if (gh_blank(arguments) && profile != NULL) {
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

    openai_show_github(operation, run_args);
}
