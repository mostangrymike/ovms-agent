#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_config.h"

#define GH_CRED_LINE_MAX 512U
#define GH_CRED_PATH_MAX (GH_PROF_REPO_MAX + 16U)

static void gh_cred_chomp(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\n' ||
            text[length - 1U] == '\r')) {
        text[--length] = '\0';
    }
}

static void gh_cred_copy(char *output,
                         size_t output_size,
                         const char *value)
{
    size_t length;

    if (output == NULL || output_size == 0U) {
        return;
    }

    output[0] = '\0';
    if (value == NULL) {
        return;
    }

    length = strlen(value);
    if (length >= output_size) {
        length = output_size - 1U;
    }
    if (length > 0U) {
        (void)memcpy(output, value, length);
    }
    output[length] = '\0';
}

static void gh_cred_read(char *protocol,
                         size_t protocol_size,
                         char *host,
                         size_t host_size,
                         char *path,
                         size_t path_size)
{
    char line[GH_CRED_LINE_MAX];

    gh_cred_copy(protocol, protocol_size, "");
    gh_cred_copy(host, host_size, "");
    gh_cred_copy(path, path_size, "");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        gh_cred_chomp(line);
        if (line[0] == '\0') {
            break;
        }
        if (strncmp(line, "protocol=", 9) == 0) {
            gh_cred_copy(protocol, protocol_size, line + 9);
        } else if (strncmp(line, "host=", 5) == 0) {
            gh_cred_copy(host, host_size, line + 5);
        } else if (strncmp(line, "path=", 5) == 0) {
            gh_cred_copy(path, path_size, line + 5);
        }
    }
}

static int gh_cred_repo_match(const gh_profile *profile,
                              const char *path)
{
    char clean[GH_CRED_PATH_MAX];
    size_t length;
    const char *start;

    if (profile == NULL || path == NULL || *path == '\0') {
        return 0;
    }

    start = path;
    while (*start == '/') {
        ++start;
    }

    gh_cred_copy(clean, sizeof(clean), start);
    length = strlen(clean);
    if (length > 4U && strcmp(clean + length - 4U, ".git") == 0) {
        clean[length - 4U] = '\0';
    }

    return strcmp(clean, profile->repo) == 0;
}

static int gh_cred_ready(const gh_profile *profile)
{
    return profile != NULL &&
           profile->repo[0] != '\0' &&
           profile->user[0] != '\0' &&
           profile->token[0] != '\0';
}

int main(int argc, char **argv)
{
    const gh_profile *profile;
    char protocol[32];
    char host[128];
    char path[GH_CRED_PATH_MAX];

    profile = gh_prof_active();

    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        if (!gh_cred_ready(profile)) {
            (void)puts("GitHub credential helper: active profile is incomplete.");
            return EXIT_FAILURE;
        }

        (void)printf(
            "GitHub credential helper ready: profile=%s user=configured token=configured\n",
            profile->name);
        return EXIT_SUCCESS;
    }

    if (argc != 2) {
        return EXIT_FAILURE;
    }

    gh_cred_read(protocol, sizeof(protocol),
                 host, sizeof(host),
                 path, sizeof(path));

    if (strcmp(argv[1], "store") == 0 ||
        strcmp(argv[1], "erase") == 0) {
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "get") != 0) {
        return EXIT_SUCCESS;
    }

    if (!gh_cred_ready(profile) ||
        strcmp(protocol, "https") != 0 ||
        strcmp(host, "github.com") != 0 ||
        !gh_cred_repo_match(profile, path)) {
        (void)puts("quit=1");
        return EXIT_SUCCESS;
    }

    (void)printf("username=%s\n", profile->user);
    (void)printf("password=%s\n", profile->token);
    return EXIT_SUCCESS;
}
