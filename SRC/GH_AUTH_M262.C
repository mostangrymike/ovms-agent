#include <stdio.h>
#include <string.h>

#define gh_saved_exec gh_saved_exec_old
#include "GH_AUTH.C"
#undef gh_saved_exec

#define GH_M262_REPO_COM "SYS$LOGIN:OVMS_AGENT_GH_REPO.COM"
#define GH_M262_REPO_OUT "SYS$LOGIN:OVMS_AGENT_GH_REPO.OUT"

static int m262_gh_remote_ok(const char *remote,
                             const gh_profile *profile)
{
    if (remote == NULL || profile == NULL) {
        return 0;
    }

    return strcmp(remote, "origin") == 0 ||
           strcmp(remote, profile->repo) == 0;
}

static void m262_gh_trim(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\r' || text[length - 1U] == '\n')) {
        text[--length] = '\0';
    }
}

static int m262_gh_repo_match(const gh_profile *profile)
{
    FILE *file;
    char actual[GH_PROF_REPO_MAX + 64U];
    char https_git[GH_PROF_REPO_MAX + 32U];
    char https_plain[GH_PROF_REPO_MAX + 32U];
    char ssh_git[GH_PROF_REPO_MAX + 32U];
    char ssh_plain[GH_PROF_REPO_MAX + 32U];
    int ok;

    if (profile == NULL || profile->repo[0] == '\0') {
        return 0;
    }

    gh_rm_versions(GH_M262_REPO_COM);
    gh_rm_versions(GH_M262_REPO_OUT);

    file = fopen(GH_M262_REPO_COM, "w");
    if (file == NULL) {
        return 0;
    }

    ok = fputs("$ SET NOON\n", file) != EOF &&
         fputs("$ PIPE GIT \"remote\" \"get-url\" \"origin\" > "
               "SYS$LOGIN:OVMS_AGENT_GH_REPO.OUT 2> NL:\n", file) != EOF &&
         fputs("$ EXIT 1\n", file) != EOF;

    if (fclose(file) != 0) {
        ok = 0;
    }
    if (!ok) {
        gh_rm_versions(GH_M262_REPO_COM);
        gh_rm_versions(GH_M262_REPO_OUT);
        return 0;
    }

    (void)system("@SYS$LOGIN:OVMS_AGENT_GH_REPO.COM");
    gh_rm_versions(GH_M262_REPO_COM);

    actual[0] = '\0';
    file = fopen(GH_M262_REPO_OUT, "r");
    if (file != NULL) {
        if (fgets(actual, sizeof(actual), file) == NULL) {
            actual[0] = '\0';
        }
        (void)fclose(file);
    }
    gh_rm_versions(GH_M262_REPO_OUT);
    m262_gh_trim(actual);

    if (snprintf(https_git, sizeof(https_git),
                 "https://github.com/%s.git", profile->repo) < 0 ||
        snprintf(https_plain, sizeof(https_plain),
                 "https://github.com/%s", profile->repo) < 0 ||
        snprintf(ssh_git, sizeof(ssh_git),
                 "git@github.com:%s.git", profile->repo) < 0 ||
        snprintf(ssh_plain, sizeof(ssh_plain),
                 "git@github.com:%s", profile->repo) < 0) {
        return 0;
    }

    return strcmp(actual, https_git) == 0 ||
           strcmp(actual, https_plain) == 0 ||
           strcmp(actual, ssh_git) == 0 ||
           strcmp(actual, ssh_plain) == 0;
}

static int m262_gh_diag_fail(const char *line)
{
    if (gh_diag_fail(line)) {
        return 1;
    }

    return line != NULL &&
        (strstr(line, "Username for") != NULL ||
         strstr(line, "Password for") != NULL ||
         strstr(line, "terminal prompts disabled") != NULL ||
         strstr(line, "could not read Password") != NULL);
}

static int m262_gh_run(const char *command)
{
    FILE *file;
    char line[1024];
    int ok;
    int failed;

    if (command == NULL || *command == '\0') {
        return 0;
    }

    failed = 0;
    gh_rm_versions(GH_RUN_COM);
    gh_rm_versions(GH_RUN_ERR);

    file = fopen(GH_RUN_COM, "w");
    if (file == NULL) {
        return 0;
    }

    ok = fputs("$ SET NOON\n", file) != EOF &&
         fputs("$ DEFINE/PROCESS/NOLOG GIT_TERMINAL_PROMPT \"0\"\n", file) != EOF &&
         fprintf(file,
                 "$ PIPE %s 2> SYS$LOGIN:OVMS_AGENT_GH_RUN.ERR\n",
                 command) >= 0 &&
         fputs("$ DEASSIGN/PROCESS GIT_TERMINAL_PROMPT\n$ EXIT 1\n", file) != EOF;

    if (fclose(file) != 0) {
        ok = 0;
    }
    if (!ok) {
        gh_rm_versions(GH_RUN_COM);
        gh_rm_versions(GH_RUN_ERR);
        return 0;
    }

    (void)system("@SYS$LOGIN:OVMS_AGENT_GH_RUN.COM");
    gh_rm_versions(GH_RUN_COM);

    file = fopen(GH_RUN_ERR, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (m262_gh_diag_fail(line)) {
                failed = 1;
            }
            (void)fputs(line, stdout);
        }
        (void)fclose(file);
    }
    gh_rm_versions(GH_RUN_ERR);

    return !failed;
}

int gh_saved_exec(const char *operation, const char *arguments,
                  char *result, size_t result_size, void *context)
{
    const gh_profile *profile;
    char first[256];
    char second[256];
    char command[GH_CMD_MAX];
    char auth_path[GH_PATH_MAX];
    char repo_url[GH_PROF_REPO_MAX + 32U];
    int written;
    int success;

    (void)context;

    if (!gh_net_op(operation) || arguments == NULL ||
        result == NULL || result_size == 0U) {
        return 0;
    }

    profile = gh_prof_active();
    if (!gh_ready(profile)) {
        (void)snprintf(result, result_size,
            "Active GitHub profile has no usable saved credentials.\n");
        return 0;
    }

    if (strcmp(operation, "clone") != 0 &&
        !m262_gh_repo_match(profile)) {
        (void)snprintf(result, result_size,
            "GitHub operation refused: current checkout origin does not match the active saved profile repository.\n");
        return 0;
    }

    written = snprintf(repo_url, sizeof(repo_url),
                       "https://github.com/%s.git", profile->repo);
    if (written < 0 || (size_t)written >= sizeof(repo_url)) {
        (void)snprintf(result, result_size,
            "Configured GitHub repository name is too long.\n");
        return 0;
    }

    if (gh_preflight(result, result_size) <= 0) {
        return 0;
    }
    result[0] = '\0';

    if (!gh_auth_cfg(profile, auth_path, sizeof(auth_path))) {
        (void)snprintf(result, result_size,
            "Unable to prepare temporary GitHub authentication data.\n");
        return 0;
    }

    if (!gh_probe(profile, auth_path)) {
        gh_zero(auth_path, sizeof(auth_path));
        gh_auth_cleanup();
        (void)snprintf(result, result_size,
            "Saved GitHub credentials were rejected or the configured repository could not be read.\n");
        return 0;
    }

    if (strcmp(operation, "fetch") == 0) {
        if (*arguments == '\0') {
            (void)strcpy(first, "origin");
            second[0] = '\0';
        } else if (!gh_split2(arguments, first, sizeof(first),
                              second, sizeof(second), 1)) {
            goto bad;
        }

        if (!m262_gh_remote_ok(first, profile)) {
            (void)snprintf(result, result_size,
                "GitHub operation refused: requested remote does not match the active saved profile repository.\n");
            goto refused;
        }

        if (second[0] == '\0') {
            written = snprintf(command, sizeof(command),
                "GIT \"-c\" \"include.path=%s\" \"fetch\" \"%s\"",
                auth_path, repo_url);
        } else {
            written = snprintf(command, sizeof(command),
                "GIT \"-c\" \"include.path=%s\" \"fetch\" \"%s\" \"%s\"",
                auth_path, repo_url, second);
        }
    } else if (strcmp(operation, "pull") == 0 ||
               strcmp(operation, "push") == 0) {
        if (!gh_split2(arguments, first, sizeof(first),
                       second, sizeof(second), 0)) {
            goto bad;
        }

        if (!m262_gh_remote_ok(first, profile)) {
            (void)snprintf(result, result_size,
                "GitHub operation refused: requested remote does not match the active saved profile repository.\n");
            goto refused;
        }

        written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"%s\" \"%s\" \"%s\"",
            auth_path, operation, repo_url, second);
    } else {
        if (!gh_split2(arguments, first, sizeof(first),
                       second, sizeof(second), 1)) {
            goto bad;
        }

        if (strcmp(first, repo_url) != 0) {
            (void)snprintf(result, result_size,
                "GitHub clone refused: requested repository does not match the active saved profile.\n");
            goto refused;
        }

        if (second[0] == '\0') {
            written = snprintf(command, sizeof(command),
                "GIT \"-c\" \"include.path=%s\" \"clone\" \"%s\"",
                auth_path, repo_url);
        } else {
            written = snprintf(command, sizeof(command),
                "GIT \"-c\" \"include.path=%s\" \"clone\" \"%s\" \"%s\"",
                auth_path, repo_url, second);
        }
    }

    if (written < 0 || (size_t)written >= sizeof(command)) {
        goto bad;
    }

    success = m262_gh_run(command);
    gh_zero(command, sizeof(command));
    gh_zero(auth_path, sizeof(auth_path));
    gh_auth_cleanup();

    if (!success) {
        (void)snprintf(result, result_size,
            "Authenticated OpenVMS Git command failed.\n");
        return 0;
    }

    written = snprintf(result, result_size,
        "OpenVMS Git command completed successfully using the active GitHub profile.\n");
    return written >= 0 && (size_t)written < result_size;

refused:
    gh_zero(command, sizeof(command));
    gh_zero(auth_path, sizeof(auth_path));
    gh_auth_cleanup();
    return 0;

bad:
    gh_zero(command, sizeof(command));
    gh_zero(auth_path, sizeof(auth_path));
    gh_auth_cleanup();
    return 0;
}
