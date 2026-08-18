#include <stdio.h>
#include <string.h>

#define gh_saved_exec gh_saved_exec_old
#include "GH_AUTH.C"
#undef gh_saved_exec

static int m262_gh_remote_ok(const char *remote,
                             const gh_profile *profile)
{
    if (remote == NULL || profile == NULL) {
        return 0;
    }

    return strcmp(remote, "origin") == 0 ||
           strcmp(remote, profile->repo) == 0;
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

    success = gh_run(command);
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
