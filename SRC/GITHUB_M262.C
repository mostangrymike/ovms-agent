#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gh_auth.h"
#include "github_m262.h"
#include "llm_config.h"
#include "llm_internal.h"

#define M269_CHECK_COM "SYS$LOGIN:OVMS_AGENT_GH_CHECK.COM"
#define M269_CHECK_TMP "SYS$LOGIN:OVMS_AGENT_GH_CHECK.TMP"

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

static void m269_rm_versions(const char *path)
{
    if (path == NULL || *path == '\0') return;
    while (remove(path) == 0) { }
}

/* M269: CHECK must not serialize the full CURPRIV list into a DCL
   command element.  F$PRIVILEGE returns bounded TRUE/FALSE text and
   preserves the same SHARE gate used by saved-profile network Git. */
static int m269_check_env(char *result, size_t result_size)
{
    FILE *file;
    char line[256];
    char share[16];
    char parse[64];
    char arch[64];
    int have_share;
    int parse_needed;
    int have_parse;
    int written;
    int status;

    if (result == NULL || result_size == 0U) return 0;
    result[0] = '\0';
    share[0] = '\0';
    parse[0] = '\0';
    arch[0] = '\0';

    m269_rm_versions(M269_CHECK_COM);
    m269_rm_versions(M269_CHECK_TMP);
    file = fopen(M269_CHECK_COM, "w");
    if (file == NULL) return 0;
    if (fputs("$ SET NOON\n", file) == EOF ||
        fputs("$ OPEN/WRITE GHOUT SYS$LOGIN:OVMS_AGENT_GH_CHECK.TMP\n", file) == EOF ||
        fputs("$ P = F$PRIVILEGE(\"SHARE\")\n", file) == EOF ||
        fputs("$ A = F$GETSYI(\"ARCH_NAME\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"SHARE=''P'\"\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"ARCH=''A'\"\n", file) == EOF ||
        fputs("$ IF A .EQS. \"VAX\" THEN GOTO DONE\n", file) == EOF ||
        fputs("$ S = F$GETJPI(\"\",\"PARSE_STYLE_PERM\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"PARSE=''S'\"\n", file) == EOF ||
        fputs("$ DONE:\n$ CLOSE GHOUT\n$ EXIT 1\n", file) == EOF ||
        fclose(file) != 0) {
        m269_rm_versions(M269_CHECK_COM);
        m269_rm_versions(M269_CHECK_TMP);
        return 0;
    }

    status = system("@SYS$LOGIN:OVMS_AGENT_GH_CHECK.COM");
    m269_rm_versions(M269_CHECK_COM);
    if ((status & 1) == 0) {
        m269_rm_versions(M269_CHECK_TMP);
        return 0;
    }

    file = fopen(M269_CHECK_TMP, "r");
    if (file == NULL) {
        m269_rm_versions(M269_CHECK_TMP);
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        while (length > 0U &&
               (line[length - 1U] == '\r' || line[length - 1U] == '\n'))
            line[--length] = '\0';
        if (strncmp(line, "SHARE=", 6U) == 0)
            (void)snprintf(share, sizeof(share), "%s", line + 6);
        else if (strncmp(line, "PARSE=", 6U) == 0)
            (void)snprintf(parse, sizeof(parse), "%s", line + 6);
        else if (strncmp(line, "ARCH=", 5U) == 0)
            (void)snprintf(arch, sizeof(arch), "%s", line + 5);
    }
    (void)fclose(file);
    m269_rm_versions(M269_CHECK_TMP);

    have_share = strcmp(share, "TRUE") == 0;
    parse_needed = strcmp(arch, "VAX") != 0;
    have_parse = !parse_needed || strcmp(parse, "EXTENDED") == 0;

    if (have_share && have_parse) {
        written = snprintf(result, result_size,
            "OpenVMS Git network preflight: ready.\n"
            "SHARE privilege: enabled\n"
            "DCL parse style: %s\n",
            parse_needed ? "EXTENDED" : "not applicable on VAX");
        return written >= 0 && (size_t)written < result_size ? 1 : 0;
    }

    written = snprintf(result, result_size,
        "GitHub network operation refused by OpenVMS preflight.\n"
        "SHARE privilege: %s\n"
        "DCL parse style: %s\n"
        "%s%s"
        "Then retry the GitHub operation.\n",
        have_share ? "enabled" : "NOT enabled",
        parse_needed ? (have_parse ? "EXTENDED" : "NOT EXTENDED") :
                       "not applicable on VAX",
        have_share ? "" :
          "At DCL run: $ SET PROCESS/PRIVILEGE=SHARE\n",
        (!parse_needed || have_parse) ? "" :
          "At DCL run: $ SET PROCESS/PARSE_STYLE=EXTENDED\n");
    return written < 0 || (size_t)written >= result_size ? 0 : -1;
}

static int m269_check_exec(const char *operation,
                           const char *arguments,
                           char *result,
                           size_t result_size,
                           void *context)
{
    int status;
    (void)context;
    if (operation == NULL || strcmp(operation, "check") != 0 ||
        !m262_blank(arguments)) return 0;
    status = m269_check_env(result, result_size);
    return status > 0;
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

    if (strcmp(operation, "check") == 0) {
        output[0] = '\0';
        if (!llm_gh_run_text(operation, arguments, m269_check_exec,
                             NULL, output, sizeof(output))) {
            if (output[0] != '\0') (void)fputs(output, stdout);
            else (void)puts("GitHub preflight failed.");
            return;
        }
        (void)fputs(output, stdout);
        return;
    }

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
