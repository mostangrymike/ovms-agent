#include <stdio.h>
#include <string.h>

#include "SETTINGS.H"

#define M280_SETTINGS_FILE "[.BUILD]M280_SETTINGS_TEST.DAT"

static void m280_cleanup(void)
{
    while (remove(M280_SETTINGS_FILE) == 0) {
    }
}

static int m280_expect(int condition, const char *message)
{
    if (!condition) {
        (void)printf("M280 settings regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *value;

    m280_cleanup();
    settings_test_path(M280_SETTINGS_FILE);

    if (!m280_expect(settings_reload(), "initial reload") ||
        !m280_expect(!settings_is_saved("approval_policy"),
                     "unsaved approval default") ||
        !m280_expect(
            settings_effective_bool("guarded_writes", NULL, 0) == 0,
            "default guarded writes") ||
        !m280_expect(
            settings_effective_bool("streaming", NULL, 1) == 0,
            "default streaming off") ||
        !m280_expect(
            settings_effective_long(
                "max_output_tokens", NULL, 2048L, 64L, 32768L) == 2048L,
            "default token limit") ||
        !m280_expect(
            settings_effective_long(
                "auto_turns", NULL, 12L, 1L, 32L) == 12L,
            "default autonomous turns") ||
        !m280_expect(
            settings_effective_long(
                "auto_writes", NULL, 3L, 1L, 8L) == 3L,
            "default autonomous writes") ||
        !m280_expect(
            strcmp(settings_value_source("guarded_writes", NULL),
                   "default") == 0,
            "default source") ||
        !m280_expect(
            strcmp(settings_value_source("streaming", NULL),
                   "default") == 0,
            "default streaming source")) {
        m280_cleanup();
        return 2;
    }

    if (!m280_expect(settings_set_bool("guarded_writes", 1),
                     "save guarded writes") ||
        !m280_expect(settings_set_bool("dcl_execution", 1),
                     "save DCL") ||
        !m280_expect(settings_set_bool("streaming", 1),
                     "save streaming") ||
        !m280_expect(settings_set_long("max_output_tokens", 4096L),
                     "save token limit") ||
        !m280_expect(settings_set_long("auto_turns", 20L),
                     "save autonomous turns") ||
        !m280_expect(settings_set_long("auto_writes", 5L),
                     "save autonomous writes") ||
        !m280_expect(settings_set("approval_policy", "workspace"),
                     "save approval") ||
        !m280_expect(settings_set("net_allow", "example.com,*.test"),
                     "save network allow")) {
        m280_cleanup();
        return 2;
    }

    if (!m280_expect(settings_reload(), "reload saved settings") ||
        !m280_expect(settings_is_saved("approval_policy"),
                     "saved approval presence") ||
        !m280_expect(
            settings_effective_bool("guarded_writes", NULL, 0) == 1,
            "persist guarded writes") ||
        !m280_expect(
            settings_effective_bool("dcl_execution", NULL, 0) == 1,
            "persist DCL") ||
        !m280_expect(
            settings_effective_bool("streaming", NULL, 0) == 1,
            "persist streaming") ||
        !m280_expect(
            settings_effective_long(
                "max_output_tokens", NULL, 2048L, 64L, 32768L) == 4096L,
            "persist token limit") ||
        !m280_expect(
            settings_effective_long(
                "auto_turns", NULL, 12L, 1L, 32L) == 20L,
            "persist autonomous turns") ||
        !m280_expect(
            settings_effective_long(
                "auto_writes", NULL, 3L, 1L, 8L) == 5L,
            "persist autonomous writes") ||
        !m280_expect(
            strcmp(settings_value_source("auto_turns", NULL),
                   "saved") == 0,
            "saved autonomous source") ||
        !m280_expect(
            strcmp(settings_value_source("streaming", NULL),
                   "saved") == 0,
            "saved streaming source") ||
        !m280_expect(
            strcmp(settings_get("approval_policy"), "workspace") == 0,
            "persist approval") ||
        !m280_expect(
            strcmp(settings_get("net_allow"), "example.com,*.test") == 0,
            "persist network allow") ||
        !m280_expect(
            strcmp(settings_value_source("guarded_writes", NULL),
                   "saved") == 0,
            "saved source")) {
        m280_cleanup();
        return 2;
    }

    if (!m280_expect(settings_set_long("auto_turns", 99L),
                     "save high autonomous turns") ||
        !m280_expect(
            settings_effective_long(
                "auto_turns", NULL, 12L, 1L, 32L) == 32L,
            "clamp high autonomous turns") ||
        !m280_expect(settings_set_long("auto_writes", 0L),
                     "save low autonomous writes") ||
        !m280_expect(
            settings_effective_long(
                "auto_writes", NULL, 3L, 1L, 8L) == 1L,
            "clamp low autonomous writes")) {
        m280_cleanup();
        return 2;
    }

    if (!m280_expect(settings_reset(), "reset defaults") ||
        !m280_expect(settings_reload(), "reload defaults") ||
        !m280_expect(settings_is_saved("approval_policy"),
                     "reset defaults are saved") ||
        !m280_expect(
            settings_effective_bool("guarded_writes", NULL, 1) == 0,
            "reset guarded writes") ||
        !m280_expect(
            settings_effective_bool("dcl_execution", NULL, 1) == 0,
            "reset DCL") ||
        !m280_expect(
            settings_effective_bool("streaming", NULL, 1) == 0,
            "reset streaming") ||
        !m280_expect(
            settings_effective_long(
                "max_output_tokens", NULL, 1L, 64L, 32768L) == 2048L,
            "reset token limit") ||
        !m280_expect(
            settings_effective_long(
                "auto_turns", NULL, 1L, 1L, 32L) == 12L,
            "reset autonomous turns") ||
        !m280_expect(
            settings_effective_long(
                "auto_writes", NULL, 1L, 1L, 8L) == 3L,
            "reset autonomous writes")) {
        m280_cleanup();
        return 2;
    }

    value = settings_get("approval_policy");
    if (!m280_expect(value != NULL && strcmp(value, "read-only") == 0,
                     "reset approval")) {
        m280_cleanup();
        return 2;
    }

    m280_cleanup();
    settings_test_path(NULL);
    (void)puts("M280 settings regression passed.");
    return 1;
}
