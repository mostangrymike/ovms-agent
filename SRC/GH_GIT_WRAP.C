#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "llm_config.h"
#include "ovms_status.h"

#define WRAP_AUTH_CFG "SYS$LOGIN:OVMS_AGENT_GIT_AUTH.CFG"
#define WRAP_RUN_COM  "SYS$LOGIN:OVMS_AGENT_GIT_RUN.COM"
#define WRAP_PATH_MAX 512U
#define WRAP_CMD_MAX  4096U
#define WRAP_RAW_MAX  (GH_PROF_USER_MAX + GH_PROF_TOKEN_MAX + 2U)
#define WRAP_B64_MAX  ((((WRAP_RAW_MAX) + 2U) / 3U) * 4U + 1U)

static void wrap_zero(char *buffer, size_t size)
{
    volatile char *cursor;
    if (buffer == NULL) return;
    cursor = (volatile char *)buffer;
    while (size > 0U) {
        *cursor++ = '\0';
        --size;
    }
}

static void wrap_rm_versions(const char *path)
{
    if (path == NULL || *path == '\0') return;
    while (remove(path) == 0) { }
}

static int wrap_ready(const gh_profile *profile)
{
    return profile != NULL &&
           profile->repo[0] != '\0' &&
           profile->user[0] != '\0' &&
           profile->token[0] != '\0';
}

static int wrap_b64(const unsigned char *input,
                    size_t input_size,
                    char *output,
                    size_t output_size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t in_pos;
    size_t out_pos;

    in_pos = 0U;
    out_pos = 0U;
    while (in_pos < input_size) {
        size_t remain;
        unsigned long value;

        remain = input_size - in_pos;
        value = ((unsigned long)input[in_pos]) << 16;
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

static void wrap_strip_ver(char *path)
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

static void wrap_auth_cleanup(void)
{
    wrap_rm_versions(WRAP_AUTH_CFG);
}

static int wrap_auth_cfg(const gh_profile *profile,
                         char *path,
                         size_t path_size)
{
    FILE *file;
    FILE *check;
    char full[WRAP_PATH_MAX];
    char translated[WRAP_PATH_MAX];
    char raw[WRAP_RAW_MAX];
    char encoded[WRAP_B64_MAX];
    int written;
    int ok;

    if (!wrap_ready(profile) || path == NULL || path_size == 0U) return 0;
    path[0] = '\0';
    full[0] = '\0';
    translated[0] = '\0';
    raw[0] = '\0';
    encoded[0] = '\0';

    wrap_auth_cleanup();
    written = snprintf(raw, sizeof(raw), "%s:%s", profile->user, profile->token);
    if (written < 0 || (size_t)written >= sizeof(raw)) return 0;
    if (!wrap_b64((const unsigned char *)raw, strlen(raw),
                  encoded, sizeof(encoded))) {
        wrap_zero(raw, sizeof(raw));
        return 0;
    }
    wrap_zero(raw, sizeof(raw));

    file = fopen(WRAP_AUTH_CFG, "w");
    if (file == NULL) {
        wrap_zero(encoded, sizeof(encoded));
        return 0;
    }
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
    wrap_zero(encoded, sizeof(encoded));

    if (!ok || chmod(WRAP_AUTH_CFG, 0600) != 0) {
        wrap_auth_cleanup();
        return 0;
    }

    check = fopen(full, "r");
    if (check == NULL) {
        wrap_auth_cleanup();
        return 0;
    }
    if (fgetname(check, translated, 0) == NULL || fclose(check) != 0) {
        wrap_auth_cleanup();
        return 0;
    }
    wrap_strip_ver(translated);
    if (strlen(translated) >= path_size) {
        wrap_auth_cleanup();
        return 0;
    }
    (void)strcpy(path, translated);
    return 1;
}

static int wrap_safe_arg(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL) return 0;
    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor == '"' || *cursor == '\r' || *cursor == '\n') return 0;
        if (*cursor < 32U && *cursor != '\t') return 0;
        ++cursor;
    }
    return 1;
}

static int wrap_append(char *command,
                       size_t command_size,
                       size_t *used,
                       const char *text)
{
    size_t length;

    if (command == NULL || used == NULL || text == NULL) return 0;
    if (!wrap_safe_arg(text)) return 0;
    length = strlen(text);
    if (*used + length + 4U >= command_size) return 0;
    command[(*used)++] = ' ';
    command[(*used)++] = '"';
    if (length > 0U) {
        (void)memcpy(command + *used, text, length);
        *used += length;
    }
    command[(*used)++] = '"';
    command[*used] = '\0';
    return 1;
}

static int wrap_build_cmd(int argc,
                          char **argv,
                          const char *auth_path,
                          char *command,
                          size_t command_size)
{
    char include_arg[WRAP_PATH_MAX + 32U];
    size_t used;
    int index;
    int written;

    if (argc < 2 || argv == NULL || auth_path == NULL ||
        command == NULL || command_size == 0U) return 0;

    written = snprintf(include_arg, sizeof(include_arg),
                       "include.path=%s", auth_path);
    if (written < 0 || (size_t)written >= sizeof(include_arg)) return 0;

    (void)strcpy(command, "GIT");
    used = strlen(command);
    if (!wrap_append(command, command_size, &used, "-c") ||
        !wrap_append(command, command_size, &used, include_arg)) return 0;

    for (index = 1; index < argc; ++index) {
        if (!wrap_append(command, command_size, &used, argv[index])) return 0;
    }
    return 1;
}

static int wrap_run(const char *command)
{
    FILE *file;
    int ok;
    int status;

    if (command == NULL || *command == '\0') return 0;
    wrap_rm_versions(WRAP_RUN_COM);
    file = fopen(WRAP_RUN_COM, "w");
    if (file == NULL) return 0;
    ok = fputs("$ SET NOON\n", file) != EOF &&
         fputs("$ DEFINE/PROCESS/NOLOG GIT_TERMINAL_PROMPT \"0\"\n", file) != EOF &&
         fprintf(file, "$ %s\n", command) >= 0 &&
         fputs("$ GIT_STATUS = $STATUS\n", file) != EOF &&
         fputs("$ GIT_MESSAGE = F$EDIT(F$MESSAGE(GIT_STATUS),\"UPCASE\")\n", file) != EOF &&
         fputs("$ GIT_RETURN = GIT_STATUS\n", file) != EOF &&
         fputs("$ IF F$LOCATE(\"%C-S-EXIT\",GIT_MESSAGE) .LT. F$LENGTH(GIT_MESSAGE) THEN GIT_RETURN = %X00000002\n", file) != EOF &&
         fputs("$ DEASSIGN/PROCESS GIT_TERMINAL_PROMPT\n", file) != EOF &&
         fputs("$ EXIT 'GIT_RETURN'\n", file) != EOF;
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        wrap_rm_versions(WRAP_RUN_COM);
        return 0;
    }

    status = system("@SYS$LOGIN:OVMS_AGENT_GIT_RUN.COM");
    wrap_rm_versions(WRAP_RUN_COM);
    return (int)ovms_status_propagate((unsigned long)(unsigned int)status);
}

static int wrap_check(const gh_profile *profile)
{
    char auth_path[WRAP_PATH_MAX];
    char command[WRAP_CMD_MAX];
    char *check_argv[7];
    int status;

    auth_path[0] = '\0';
    command[0] = '\0';
    if (!wrap_auth_cfg(profile, auth_path, sizeof(auth_path))) {
        (void)puts("GITAUTH: unable to prepare temporary authentication data.");
        return EXIT_FAILURE;
    }

    check_argv[0] = "GITAUTH";
    check_argv[1] = "push";
    check_argv[2] = "--dry-run";
    check_argv[3] = "--no-verify";
    check_argv[4] = "--quiet";
    check_argv[5] = "origin";
    check_argv[6] = "HEAD:refs/heads/__ovms_agent_auth_check__";

    if (!wrap_build_cmd(7, check_argv, auth_path,
                        command, sizeof(command))) {
        wrap_zero(auth_path, sizeof(auth_path));
        wrap_auth_cleanup();
        (void)puts("GITAUTH: unable to build authorization preflight command.");
        return EXIT_FAILURE;
    }

    status = wrap_run(command);
    wrap_zero(command, sizeof(command));
    wrap_zero(auth_path, sizeof(auth_path));
    wrap_auth_cleanup();

    if (!ovms_status_success((unsigned long)(unsigned int)status)) {
        (void)printf(
            "GITAUTH: profile=%s repo=%s is configured, but current-origin write authorization was not verified.\n",
            profile->name, profile->repo);
        return status;
    }

    (void)printf(
        "GITAUTH verified: profile=%s repo=%s authorization=write\n",
        profile->name, profile->repo);
    return status;
}

int main(int argc, char **argv)
{
    const gh_profile *profile;
    char auth_path[WRAP_PATH_MAX];
    char command[WRAP_CMD_MAX];
    int status;

    if (argc == 2 && strcmp(argv[1], "--check-config") == 0) {
        profile = gh_prof_active();
        if (!wrap_ready(profile)) {
            (void)puts("GITAUTH: active GitHub profile is incomplete.");
            return EXIT_FAILURE;
        }
        (void)printf(
            "GITAUTH configured: profile=%s repo=%s credentials=configured authorization=not-checked\n",
            profile->name, profile->repo);
        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "--check") == 0) {
        profile = gh_prof_active();
        if (!wrap_ready(profile)) {
            (void)puts("GITAUTH: active GitHub profile is incomplete.");
            return EXIT_FAILURE;
        }
        return wrap_check(profile);
    }

    if (argc < 2) {
        (void)puts("Usage: GITAUTH git-subcommand [arguments]");
        return EXIT_FAILURE;
    }

    profile = gh_prof_active();
    if (!wrap_ready(profile)) {
        (void)puts("GITAUTH: active GitHub profile has no usable credentials.");
        return EXIT_FAILURE;
    }

    auth_path[0] = '\0';
    command[0] = '\0';
    if (!wrap_auth_cfg(profile, auth_path, sizeof(auth_path))) {
        (void)puts("GITAUTH: unable to prepare temporary authentication data.");
        return EXIT_FAILURE;
    }

    if (!wrap_build_cmd(argc, argv, auth_path, command, sizeof(command))) {
        wrap_zero(auth_path, sizeof(auth_path));
        wrap_auth_cleanup();
        (void)puts("GITAUTH: Git arguments are too long or contain unsupported control characters.");
        return EXIT_FAILURE;
    }

    status = wrap_run(command);
    wrap_zero(command, sizeof(command));
    wrap_zero(auth_path, sizeof(auth_path));
    wrap_auth_cleanup();
    return status;
}
