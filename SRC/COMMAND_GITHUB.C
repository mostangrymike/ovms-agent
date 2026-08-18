#include <stdio.h>
#include <string.h>

#include "command_internal.h"
#include "llm_config.h"
#include "secret_input.h"

static int gh_read_value(const char *prompt, char *buffer, size_t size)
{
    size_t length;
    if (prompt == NULL || buffer == NULL || size < 2U) return 0;
    (void)fputs(prompt, stdout);
    (void)fflush(stdout);
    if (fgets(buffer, size, stdin) == NULL) return 0;
    length = strlen(buffer);
    while (length > 0U &&
           (buffer[length - 1U] == '\n' || buffer[length - 1U] == '\r'))
        buffer[--length] = '\0';
    return buffer[0] != '\0';
}

static const gh_profile *gh_find_prof(const char *name)
{
    unsigned int index;
    if (name == NULL || *name == '\0') return gh_prof_active();
    for (index = 0U; index < gh_prof_count(); ++index) {
        const gh_profile *profile = gh_prof_get(index);
        if (profile != NULL && strcmp(profile->name, name) == 0) return profile;
    }
    return NULL;
}

static void gh_list_prof(void)
{
    const gh_profile *active = gh_prof_active();
    unsigned int count = gh_prof_count();
    unsigned int index;
    if (count == 0U) {
        (void)puts("No GitHub profiles are configured.");
        return;
    }
    (void)puts("GitHub profiles:");
    for (index = 0U; index < count; ++index) {
        const gh_profile *profile = gh_prof_get(index);
        if (profile == NULL) continue;
        (void)printf("  %c %-16s repo=%s branch=%s token=%s\n",
                     profile == active ? '*' : ' ', profile->name,
                     profile->repo, profile->branch,
                     profile->token[0] != '\0' ? "configured" : "not configured");
    }
}

static void gh_show_prof(const char *name)
{
    const gh_profile *profile = gh_find_prof(name);
    if (profile == NULL) {
        (void)puts("GitHub profile not found.");
        return;
    }
    (void)printf("GitHub profile: %s\n", profile->name);
    (void)printf("Repository:     %s\n", profile->repo);
    (void)printf("User:           %s\n", profile->user);
    (void)printf("Branch:         %s\n", profile->branch);
    (void)printf("Token:          %s\n",
                 profile->token[0] != '\0' ? "******** (configured)" : "<not configured>");
}

static void gh_add_prof(const char *name)
{
    char repo[GH_PROF_REPO_MAX];
    char user[GH_PROF_USER_MAX];
    char branch[GH_PROF_BRANCH_MAX];
    char token[GH_PROF_TOKEN_MAX];
    if (name == NULL || *name == '\0') {
        (void)puts("Usage: GITHUB ADD name");
        return;
    }
    if (!gh_read_value("Repository (owner/repo): ", repo, sizeof(repo)) ||
        !gh_read_value("GitHub user: ", user, sizeof(user)) ||
        !gh_read_value("Branch: ", branch, sizeof(branch)) ||
        !secret_read("GitHub token: ", token, sizeof(token))) {
        (void)memset(token, 0, sizeof(token));
        (void)puts("GitHub profile configuration cancelled.");
        return;
    }
    if (!gh_prof_add(name, repo, user, branch, token)) {
        (void)memset(token, 0, sizeof(token));
        (void)puts("Unable to save GitHub profile.");
        return;
    }
    (void)memset(token, 0, sizeof(token));
    (void)printf("GitHub profile saved: %s\n", name);
}

static void command_github(agent_state *state, const char *arguments)
{
    char work[256];
    char *cursor;
    char *verb;
    char *name;
    char *extra;
    (void)state;
    if (arguments == NULL || *arguments == '\0') {
        gh_list_prof();
        return;
    }
    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("GitHub profile command is too long.");
        return;
    }
    (void)strcpy(work, arguments);
    cursor = work;
    verb = command_next_argument(&cursor);
    name = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);
    if (verb == NULL || extra != NULL) {
        (void)puts("Usage: GITHUB [LIST|SHOW [name]|USE name|ADD name|DELETE name]");
        return;
    }
    if (strcmp(verb, "LIST") == 0 && name == NULL) { gh_list_prof(); return; }
    if (strcmp(verb, "SHOW") == 0) { gh_show_prof(name); return; }
    if (strcmp(verb, "USE") == 0) {
        if (name == NULL || !gh_prof_use(name)) (void)puts("Unable to select GitHub profile.");
        else (void)printf("GitHub profile selected: %s\n", name);
        return;
    }
    if (strcmp(verb, "ADD") == 0) { gh_add_prof(name); return; }
    if (strcmp(verb, "DELETE") == 0) {
        if (name == NULL || !gh_prof_delete(name)) (void)puts("Unable to delete GitHub profile.");
        else (void)printf("GitHub profile deleted: %s\n", name);
        return;
    }
    (void)puts("Usage: GITHUB [LIST|SHOW [name]|USE name|ADD name|DELETE name]");
}

static const command_entry github_commands[] = {
    { "GITHUB", "Manage persistent GitHub profiles", command_github }
};

void command_register_github(void)
{
    (void)command_registry_add(github_commands,
        sizeof(github_commands) / sizeof(github_commands[0]));
}
