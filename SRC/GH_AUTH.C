#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gh_auth.h"
#include "llm_config.h"

#define GH_AUTH_CFG "SYS$LOGIN:OVMS_AGENT_GH_AUTH.CFG"
#define GH_RUN_COM "SYS$LOGIN:OVMS_AGENT_GH_RUN.COM"
#define GH_RUN_ERR "SYS$LOGIN:OVMS_AGENT_GH_RUN.ERR"
#define GH_PROBE_COM "SYS$LOGIN:OVMS_AGENT_GH_PROBE.COM"
#define GH_PROBE_OUT "SYS$LOGIN:OVMS_AGENT_GH_PROBE.OUT"
#define GH_ENV_COM "SYS$LOGIN:OVMS_AGENT_GH_AUTH_ENV.COM"
#define GH_ENV_TMP "SYS$LOGIN:OVMS_AGENT_GH_AUTH_ENV.TMP"
#define GH_PATH_MAX 512U
#define GH_CMD_MAX 1536U
#define GH_RAW_MAX (GH_PROF_USER_MAX + GH_PROF_TOKEN_MAX + 2U)
#define GH_B64_MAX ((((GH_RAW_MAX) + 2U) / 3U) * 4U + 1U)

static void gh_zero(char *buffer, size_t size)
{
    volatile char *cursor;
    if (buffer == NULL) return;
    cursor = (volatile char *)buffer;
    while (size > 0U) { *cursor++ = '\0'; --size; }
}

static void gh_rm_versions(const char *path)
{
    if (path == NULL || *path == '\0') return;
    while (remove(path) == 0) { }
}

static int gh_ready(const gh_profile *profile)
{
    return profile != NULL && profile->repo[0] != '\0' &&
           profile->user[0] != '\0' && profile->token[0] != '\0';
}

static int gh_net_op(const char *operation)
{
    return operation != NULL &&
           (strcmp(operation, "fetch") == 0 || strcmp(operation, "pull") == 0 ||
            strcmp(operation, "push") == 0 || strcmp(operation, "clone") == 0);
}

static int gh_b64(const unsigned char *input, size_t input_size,
                  char *output, size_t output_size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t in_pos = 0U;
    size_t out_pos = 0U;
    while (in_pos < input_size) {
        size_t remain = input_size - in_pos;
        unsigned long value = ((unsigned long)input[in_pos]) << 16;
        if (remain > 1U) value |= ((unsigned long)input[in_pos + 1U]) << 8;
        if (remain > 2U) value |= (unsigned long)input[in_pos + 2U];
        if (out_pos + 4U >= output_size) return 0;
        output[out_pos++] = alphabet[(value >> 18) & 0x3fU];
        output[out_pos++] = alphabet[(value >> 12) & 0x3fU];
        output[out_pos++] = remain > 1U ? alphabet[(value >> 6) & 0x3fU] : '=';
        output[out_pos++] = remain > 2U ? alphabet[value & 0x3fU] : '=';
        in_pos += remain >= 3U ? 3U : remain;
    }
    output[out_pos] = '\0';
    return 1;
}

static void gh_strip_ver(char *path)
{
    char *suffix;
    char *cursor;
    if (path == NULL || *path == '\0') return;
    suffix = strrchr(path, '.');
    if (suffix == NULL || suffix[1] == '\0') return;
    cursor = suffix + 1;
    while (*cursor >= '0' && *cursor <= '9') ++cursor;
    if (*cursor == '\0') *suffix = '\0';
}

static void gh_auth_cleanup(void)
{
    gh_rm_versions(GH_AUTH_CFG);
}

static int gh_auth_cfg(const gh_profile *profile, char *path, size_t path_size)
{
    FILE *file;
    FILE *check;
    char full[GH_PATH_MAX];
    char translated[GH_PATH_MAX];
    char raw[GH_RAW_MAX];
    char encoded[GH_B64_MAX];
    int written;
    int ok;

    if (!gh_ready(profile) || path == NULL || path_size == 0U) return 0;
    path[0] = '\0'; full[0] = '\0'; translated[0] = '\0';
    raw[0] = '\0'; encoded[0] = '\0';
    gh_auth_cleanup();
    written = snprintf(raw, sizeof(raw), "%s:%s", profile->user, profile->token);
    if (written < 0 || (size_t)written >= sizeof(raw)) return 0;
    if (!gh_b64((const unsigned char *)raw, strlen(raw), encoded, sizeof(encoded))) {
        gh_zero(raw, sizeof(raw)); return 0;
    }
    gh_zero(raw, sizeof(raw));
    file = fopen(GH_AUTH_CFG, "w");
    if (file == NULL) { gh_zero(encoded, sizeof(encoded)); return 0; }
    ok = fgetname(file, full, 1) != NULL;
    if (ok) {
        written = fprintf(file,
            "[http \"https://github.com/%s.git\"]\n"
            "\textraHeader =\n"
            "\textraHeader = Authorization: Basic %s\n",
            profile->repo, encoded);
        ok = written >= 0;
    }
    if (fclose(file) != 0) ok = 0;
    gh_zero(encoded, sizeof(encoded));
    if (!ok || chmod(GH_AUTH_CFG, 0600) != 0) { gh_auth_cleanup(); return 0; }
    check = fopen(full, "r");
    if (check == NULL) { gh_auth_cleanup(); return 0; }
    if (fgetname(check, translated, 0) == NULL || fclose(check) != 0) {
        gh_auth_cleanup(); return 0;
    }
    gh_strip_ver(translated);
    if (strlen(translated) >= path_size) { gh_auth_cleanup(); return 0; }
    (void)strcpy(path, translated);
    return 1;
}

static int gh_preflight(char *result, size_t result_size)
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

    share[0] = '\0'; parse[0] = '\0'; arch[0] = '\0';
    gh_rm_versions(GH_ENV_COM); gh_rm_versions(GH_ENV_TMP);
    file = fopen(GH_ENV_COM, "w");
    if (file == NULL) return 0;
    if (fputs("$ SET NOON\n", file) == EOF ||
        fputs("$ OPEN/WRITE GHOUT SYS$LOGIN:OVMS_AGENT_GH_AUTH_ENV.TMP\n", file) == EOF ||
        fputs("$ P = F$PRIVILEGE(\"SHARE\")\n", file) == EOF ||
        fputs("$ A = F$GETSYI(\"ARCH_NAME\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"SHARE=''P'\"\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"ARCH=''A'\"\n", file) == EOF ||
        fputs("$ IF A .EQS. \"VAX\" THEN GOTO DONE\n", file) == EOF ||
        fputs("$ S = F$GETJPI(\"\",\"PARSE_STYLE_PERM\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"PARSE=''S'\"\n", file) == EOF ||
        fputs("$ DONE:\n$ CLOSE GHOUT\n$ EXIT 1\n", file) == EOF ||
        fclose(file) != 0) return 0;
    (void)system("@SYS$LOGIN:OVMS_AGENT_GH_AUTH_ENV.COM");
    gh_rm_versions(GH_ENV_COM);
    file = fopen(GH_ENV_TMP, "r");
    if (file == NULL) { gh_rm_versions(GH_ENV_TMP); return 0; }
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        while (length > 0U && (line[length-1U] == '\r' || line[length-1U] == '\n'))
            line[--length] = '\0';
        if (strncmp(line, "SHARE=", 6) == 0) (void)snprintf(share, sizeof(share), "%s", line+6);
        else if (strncmp(line, "PARSE=", 6) == 0) (void)snprintf(parse, sizeof(parse), "%s", line+6);
        else if (strncmp(line, "ARCH=", 5) == 0) (void)snprintf(arch, sizeof(arch), "%s", line+5);
    }
    (void)fclose(file); gh_rm_versions(GH_ENV_TMP);
    have_share = strcmp(share, "TRUE") == 0;
    parse_needed = strcmp(arch, "VAX") != 0;
    have_parse = !parse_needed || strcmp(parse, "EXTENDED") == 0;
    if (have_share && have_parse) return 1;
    written = snprintf(result, result_size,
        "GitHub network operation refused by OpenVMS preflight.\n"
        "SHARE privilege: %s\nDCL parse style: %s\n%s%s",
        have_share ? "enabled" : "NOT enabled",
        parse_needed ? (have_parse ? "EXTENDED" : "NOT EXTENDED") : "not applicable on VAX",
        have_share ? "" : "At DCL run: $ SET PROCESS/PRIVILEGE=SHARE\n",
        (!parse_needed || have_parse) ? "" : "At DCL run: $ SET PROCESS/PARSE_STYLE=EXTENDED\n");
    return written >= 0 && (size_t)written < result_size ? -1 : 0;
}

static int gh_probe(const gh_profile *profile, const char *auth_path)
{
    FILE *file;
    char line[1024];
    int ok;
    int have_ref = 0;
    gh_rm_versions(GH_PROBE_COM); gh_rm_versions(GH_PROBE_OUT);
    file = fopen(GH_PROBE_COM, "w");
    if (file == NULL) return 0;
    ok = fputs("$ SET NOON\n", file) != EOF &&
         fputs("$ DEFINE/PROCESS/NOLOG GIT_TERMINAL_PROMPT \"0\"\n", file) != EOF &&
         fprintf(file,
             "$ PIPE GIT \"-c\" \"include.path=%s\" \"ls-remote\" "
             "\"https://github.com/%s.git\" > SYS$LOGIN:OVMS_AGENT_GH_PROBE.OUT 2> NL:\n",
             auth_path, profile->repo) >= 0 &&
         fputs("$ DEASSIGN/PROCESS GIT_TERMINAL_PROMPT\n$ EXIT 1\n", file) != EOF;
    if (fclose(file) != 0) ok = 0;
    if (!ok) return 0;
    (void)system("@SYS$LOGIN:OVMS_AGENT_GH_PROBE.COM");
    gh_rm_versions(GH_PROBE_COM);
    file = fopen(GH_PROBE_OUT, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (strchr(line, '\t') != NULL || strchr(line, ' ') != NULL) { have_ref = 1; break; }
        }
        (void)fclose(file);
    }
    gh_rm_versions(GH_PROBE_OUT);
    return have_ref;
}

static int gh_diag_fail(const char *line)
{
    return line != NULL &&
        (strstr(line, "fatal:") != NULL || strstr(line, "error:") != NULL ||
         strstr(line, "Authentication failed") != NULL ||
         strstr(line, "could not read Username") != NULL ||
         strstr(line, "Repository not found") != NULL ||
         strstr(line, "[rejected]") != NULL || strstr(line, "CONFLICT") != NULL);
}

static int gh_run(const char *command)
{
    FILE *file;
    char line[1024];
    int ok;
    int failed = 0;
    gh_rm_versions(GH_RUN_COM); gh_rm_versions(GH_RUN_ERR);
    file = fopen(GH_RUN_COM, "w");
    if (file == NULL) return 0;
    ok = fputs("$ SET NOON\n", file) != EOF &&
         fputs("$ DEFINE/PROCESS/NOLOG GIT_TERMINAL_PROMPT \"0\"\n", file) != EOF &&
         fprintf(file, "$ PIPE %s 2> SYS$LOGIN:OVMS_AGENT_GH_RUN.ERR\n", command) >= 0 &&
         fputs("$ DEASSIGN/PROCESS GIT_TERMINAL_PROMPT\n$ EXIT 1\n", file) != EOF;
    if (fclose(file) != 0) ok = 0;
    if (!ok) return 0;
    (void)system("@SYS$LOGIN:OVMS_AGENT_GH_RUN.COM");
    gh_rm_versions(GH_RUN_COM);
    file = fopen(GH_RUN_ERR, "r");
    if (file != NULL) {
        while (fgets(line, sizeof(line), file) != NULL) {
            if (gh_diag_fail(line)) failed = 1;
            (void)fputs(line, stdout);
        }
        (void)fclose(file);
    }
    gh_rm_versions(GH_RUN_ERR);
    return !failed;
}

static int gh_split2(const char *arguments, char *first, size_t first_size,
                     char *second, size_t second_size, int second_optional)
{
    const char *cursor = arguments;
    const char *start;
    size_t length;
    first[0] = '\0'; second[0] = '\0';
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (*cursor == '\0') return 0;
    start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') ++cursor;
    length = (size_t)(cursor - start);
    if (length == 0U || length >= first_size) return 0;
    (void)memcpy(first, start, length); first[length] = '\0';
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (*cursor == '\0') return second_optional;
    start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') ++cursor;
    length = (size_t)(cursor - start);
    if (length == 0U || length >= second_size) return 0;
    (void)memcpy(second, start, length); second[length] = '\0';
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    return *cursor == '\0';
}

int gh_saved_exec(const char *operation, const char *arguments,
                  char *result, size_t result_size, void *context)
{
    const gh_profile *profile;
    char first[256];
    char second[256];
    char command[GH_CMD_MAX];
    char auth_path[GH_PATH_MAX];
    int written;
    int success;
    (void)context;
    if (!gh_net_op(operation) || arguments == NULL || result == NULL || result_size == 0U) return 0;
    profile = gh_prof_active();
    if (!gh_ready(profile)) {
        (void)snprintf(result, result_size, "Active GitHub profile has no usable saved credentials.\n");
        return 0;
    }
    if (gh_preflight(result, result_size) <= 0) return 0;
    result[0] = '\0';
    if (!gh_auth_cfg(profile, auth_path, sizeof(auth_path))) {
        (void)snprintf(result, result_size, "Unable to prepare temporary GitHub authentication data.\n");
        return 0;
    }
    if (!gh_probe(profile, auth_path)) {
        gh_zero(auth_path, sizeof(auth_path)); gh_auth_cleanup();
        (void)snprintf(result, result_size,
            "Saved GitHub credentials were rejected or the configured repository could not be read.\n");
        return 0;
    }
    if (strcmp(operation, "fetch") == 0) {
        if (*arguments == '\0') { (void)strcpy(first, "origin"); second[0] = '\0'; }
        else if (!gh_split2(arguments, first, sizeof(first), second, sizeof(second), 1)) goto bad;
        if (second[0] == '\0') written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"fetch\" \"%s\"", auth_path, first);
        else written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"fetch\" \"%s\" \"%s\"", auth_path, first, second);
    } else if (strcmp(operation, "pull") == 0 || strcmp(operation, "push") == 0) {
        if (!gh_split2(arguments, first, sizeof(first), second, sizeof(second), 0)) goto bad;
        written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"%s\" \"%s\" \"%s\"",
            auth_path, operation, first, second);
    } else {
        if (!gh_split2(arguments, first, sizeof(first), second, sizeof(second), 1)) goto bad;
        if (second[0] == '\0') written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"clone\" \"%s\"", auth_path, first);
        else written = snprintf(command, sizeof(command),
            "GIT \"-c\" \"include.path=%s\" \"clone\" \"%s\" \"%s\"", auth_path, first, second);
    }
    if (written < 0 || (size_t)written >= sizeof(command)) goto bad;
    success = gh_run(command);
    gh_zero(command, sizeof(command)); gh_zero(auth_path, sizeof(auth_path)); gh_auth_cleanup();
    if (!success) {
        (void)snprintf(result, result_size, "Authenticated OpenVMS Git command failed.\n");
        return 0;
    }
    written = snprintf(result, result_size,
        "OpenVMS Git command completed successfully using the active GitHub profile.\n");
    return written >= 0 && (size_t)written < result_size;
bad:
    gh_zero(command, sizeof(command)); gh_zero(auth_path, sizeof(auth_path)); gh_auth_cleanup();
    return 0;
}
