#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "LLM.H"
#include "llm_config.h"
#include "SETTINGS.H"

#define SET_INPUT_MAX 1024U
#define SET_TOKEN_MIN 64L
#define SET_TOKEN_MAX 32768L

#define SET_KIND_UNKNOWN 0
#define SET_KIND_WRITES 1
#define SET_KIND_DCL 2
#define SET_KIND_PROVIDER 3
#define SET_KIND_MODEL 4
#define SET_KIND_TOKENS 5
#define SET_KIND_APPROVAL 6
#define SET_KIND_NET_ALLOW 7
#define SET_KIND_NET_DENY 8

static int set_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)toupper((unsigned char)*left++);
        b = (unsigned char)toupper((unsigned char)*right++);
        if (a != b) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static void set_chomp(char *text)
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

static int set_read(const char *prompt,
                    char *buffer,
                    size_t buffer_size)
{
    if (prompt == NULL || buffer == NULL || buffer_size < 2U) {
        return 0;
    }

    (void)fputs(prompt, stdout);
    (void)fflush(stdout);
    if (fgets(buffer, buffer_size, stdin) == NULL) {
        return 0;
    }

    set_chomp(buffer);
    return 1;
}

static int set_kind(const char *name)
{
    if (set_equal_ci(name, "guarded_writes") ||
        set_equal_ci(name, "writes") ||
        set_equal_ci(name, "write")) {
        return SET_KIND_WRITES;
    }
    if (set_equal_ci(name, "dcl_execution") ||
        set_equal_ci(name, "dcl")) {
        return SET_KIND_DCL;
    }
    if (set_equal_ci(name, "provider") ||
        set_equal_ci(name, "provider_profile")) {
        return SET_KIND_PROVIDER;
    }
    if (set_equal_ci(name, "model")) {
        return SET_KIND_MODEL;
    }
    if (set_equal_ci(name, "max_output_tokens") ||
        set_equal_ci(name, "output_tokens") ||
        set_equal_ci(name, "tokens")) {
        return SET_KIND_TOKENS;
    }
    if (set_equal_ci(name, "approval") ||
        set_equal_ci(name, "approval_policy")) {
        return SET_KIND_APPROVAL;
    }
    if (set_equal_ci(name, "net_allow") ||
        set_equal_ci(name, "network_allow")) {
        return SET_KIND_NET_ALLOW;
    }
    if (set_equal_ci(name, "net_deny") ||
        set_equal_ci(name, "network_deny")) {
        return SET_KIND_NET_DENY;
    }
    return SET_KIND_UNKNOWN;
}

static const char *set_kind_name(int kind)
{
    switch (kind) {
    case SET_KIND_WRITES: return "guarded_writes";
    case SET_KIND_DCL: return "dcl_execution";
    case SET_KIND_PROVIDER: return "provider";
    case SET_KIND_MODEL: return "model";
    case SET_KIND_TOKENS: return "max_output_tokens";
    case SET_KIND_APPROVAL: return "approval_policy";
    case SET_KIND_NET_ALLOW: return "net_allow";
    case SET_KIND_NET_DENY: return "net_deny";
    default: return "unknown";
    }
}

static int set_is_override(const char *key,
                           const char *logical_name)
{
    return strcmp(settings_value_source(key, logical_name),
                  "logical override") == 0;
}

static void set_source_suffix(const char *key,
                              const char *logical_name)
{
    const char *source;

    source = settings_value_source(key, logical_name);
    if (strcmp(source, "logical override") == 0) {
        (void)fputs("  (logical override)", stdout);
    }
}

static void set_apply_state(agent_state *state)
{
    if (state == NULL) {
        return;
    }

    state->write_enabled = settings_effective_bool(
        "guarded_writes", "OVMS_AGENT_WRITE_ENABLED", 0);
    state->dcl_enabled = settings_effective_bool(
        "dcl_execution", "OVMS_AGENT_DCL_ENABLED", 0);
    state->api_key_defined = agent_api_key_present(llm_api_key());
}

static const char *set_active_provider(void)
{
    const llm_provider *provider;

    provider = llm_prov_active();
    return provider != NULL && provider->name[0] != '\0' ?
        provider->name : "<not configured>";
}

static const char *set_active_model(void)
{
    const char *model;

    model = llm_model();
    return model != NULL && *model != '\0' ?
        model : "<not configured>";
}

static const char *set_text_display(const char *value)
{
    return value != NULL && *value != '\0' ? value : "<none>";
}

static void set_show_menu(const agent_state *state)
{
    long tokens;
    const char *allow;
    const char *deny;

    tokens = settings_effective_long(
        "max_output_tokens",
        "OVMS_AGENT_MAX_OUTPUT_TOKENS",
        2048L,
        SET_TOKEN_MIN,
        SET_TOKEN_MAX);
    allow = settings_effective_text(
        "net_allow", "OVMS_AGENT_NET_ALLOW", "");
    deny = settings_effective_text(
        "net_deny", "OVMS_AGENT_NET_DENY", "");

    (void)puts("OVMS Agent Settings");
    (void)puts("===================");
    (void)puts("");
    (void)puts("  General");
    (void)puts("  -------");
    (void)printf("  1. Guarded writes                  [%s]",
                 state != NULL && state->write_enabled ? "ON" : "OFF");
    set_source_suffix("guarded_writes", "OVMS_AGENT_WRITE_ENABLED");
    (void)putchar('\n');
    (void)printf("  2. DCL execution                   [%s]",
                 state != NULL && state->dcl_enabled ? "ON" : "OFF");
    set_source_suffix("dcl_execution", "OVMS_AGENT_DCL_ENABLED");
    (void)putchar('\n');
    (void)puts("");
    (void)puts("  LLM Provider");
    (void)puts("  ------------");
    (void)printf("  3. Provider profile                %s\n",
                 set_active_provider());
    (void)printf("  4. Model                           %s\n",
                 set_active_model());
    (void)printf("  5. Maximum output tokens           %ld", tokens);
    set_source_suffix("max_output_tokens",
                      "OVMS_AGENT_MAX_OUTPUT_TOKENS");
    (void)putchar('\n');
    (void)puts("");
    (void)puts("  Safety");
    (void)puts("  ------");
    (void)printf("  6. Approval policy                 %s",
                 llm_approval_name());
    set_source_suffix("approval_policy",
                      "OVMS_AGENT_APPROVAL_POLICY");
    (void)putchar('\n');
    (void)puts("");
    (void)puts("  Network");
    (void)puts("  -------");
    (void)printf("  7. Allowed network domains         %s",
                 set_text_display(allow));
    set_source_suffix("net_allow", "OVMS_AGENT_NET_ALLOW");
    (void)putchar('\n');
    (void)printf("  8. Denied network domains          %s",
                 set_text_display(deny));
    set_source_suffix("net_deny", "OVMS_AGENT_NET_DENY");
    (void)putchar('\n');
    (void)puts("");
    (void)puts("Enter a setting number to change it.");
    (void)puts("ON/OFF settings toggle immediately.");
    (void)puts("Changes are saved automatically.");
    (void)puts("");
    (void)puts("Commands: R reload  D defaults  Q quit");
}

static int set_parse_bool(const char *value, int *enabled)
{
    if (value == NULL || enabled == NULL) {
        return 0;
    }

    if (set_equal_ci(value, "ON") ||
        set_equal_ci(value, "YES") ||
        set_equal_ci(value, "TRUE") ||
        strcmp(value, "1") == 0) {
        *enabled = 1;
        return 1;
    }

    if (set_equal_ci(value, "OFF") ||
        set_equal_ci(value, "NO") ||
        set_equal_ci(value, "FALSE") ||
        strcmp(value, "0") == 0) {
        *enabled = 0;
        return 1;
    }

    return 0;
}

static void set_warn_override(const char *key,
                              const char *logical_name)
{
    if (set_is_override(key, logical_name)) {
        (void)printf(
            "Saved value changed, but legacy logical %s still overrides it.\n",
            logical_name);
    }
}

static int set_toggle(agent_state *state, int kind)
{
    int current;
    const char *key;
    const char *logical;

    if (kind == SET_KIND_WRITES) {
        key = "guarded_writes";
        logical = "OVMS_AGENT_WRITE_ENABLED";
        current = settings_effective_bool(key, logical, 0);
    } else if (kind == SET_KIND_DCL) {
        key = "dcl_execution";
        logical = "OVMS_AGENT_DCL_ENABLED";
        current = settings_effective_bool(key, logical, 0);
    } else {
        return 0;
    }

    if (!settings_set_bool(key, !current)) {
        (void)puts("Unable to save setting.");
        return 0;
    }

    set_apply_state(state);
    (void)printf("%s: %s -> %s\n",
                 kind == SET_KIND_WRITES ?
                     "Guarded writes" : "DCL execution",
                 current ? "ON" : "OFF",
                 !current ? "ON" : "OFF");
    set_warn_override(key, logical);
    return 1;
}

static int set_choose_provider(agent_state *state)
{
    unsigned int count;
    unsigned int index;
    char input[64];
    char *end;
    long selected;
    const llm_provider *provider;

    count = llm_prov_count();
    if (count == 0U) {
        (void)puts("No provider profiles are configured.");
        (void)puts("Use PROVIDER ADD name to create one.");
        return 0;
    }

    (void)puts("Provider profiles");
    (void)puts("=================");
    for (index = 0U; index < count; ++index) {
        provider = llm_prov_get(index);
        if (provider != NULL) {
            (void)printf("  %u. %s%s\n",
                         index + 1U,
                         provider->name,
                         provider == llm_prov_active() ? "  [current]" : "");
        }
    }

    if (!set_read("Choice> ", input, sizeof(input)) ||
        input[0] == '\0') {
        return 0;
    }

    end = NULL;
    selected = strtol(input, &end, 10);
    if (end == input || end == NULL || *end != '\0' ||
        selected < 1L || selected > (long)count) {
        (void)puts("Invalid provider choice.");
        return 0;
    }

    provider = llm_prov_get((unsigned int)(selected - 1L));
    if (provider == NULL || !llm_prov_use(provider->name)) {
        (void)puts("Unable to select provider profile.");
        return 0;
    }

    set_apply_state(state);
    (void)printf("Provider selected: %s\n", provider->name);
    return 1;
}

static int set_prompt_model(void)
{
    char input[256];

    (void)printf("Current model: %s\n", set_active_model());
    (void)puts("Press RETURN to keep the current value.");
    if (!set_read("Model> ", input, sizeof(input)) || input[0] == '\0') {
        return 0;
    }

    if (!llm_set_model(input)) {
        (void)puts("Unable to set model.");
        return 0;
    }

    (void)printf("Model set to: %s\n", set_active_model());
    return 1;
}

static int set_prompt_tokens(void)
{
    char input[64];
    char *end;
    long value;

    (void)printf("Current value: %ld\n",
        settings_effective_long(
            "max_output_tokens", "OVMS_AGENT_MAX_OUTPUT_TOKENS",
            2048L, SET_TOKEN_MIN, SET_TOKEN_MAX));
    (void)printf("Allowed range: %ld-%ld\n",
                 SET_TOKEN_MIN, SET_TOKEN_MAX);
    (void)puts("Press RETURN to keep the current value.");
    if (!set_read("Maximum output tokens> ", input, sizeof(input)) ||
        input[0] == '\0') {
        return 0;
    }

    end = NULL;
    value = strtol(input, &end, 10);
    if (end == input || end == NULL || *end != '\0' ||
        value < SET_TOKEN_MIN || value > SET_TOKEN_MAX) {
        (void)puts("Invalid output token limit.");
        return 0;
    }

    if (!settings_set_long("max_output_tokens", value)) {
        (void)puts("Unable to save setting.");
        return 0;
    }

    (void)printf("Maximum output tokens set to: %ld\n", value);
    set_warn_override("max_output_tokens",
                      "OVMS_AGENT_MAX_OUTPUT_TOKENS");
    return 1;
}

static int set_prompt_approval(void)
{
    char input[64];
    const char *value;

    (void)puts("Approval policy");
    (void)puts("===============");
    (void)puts("  1. read-only");
    (void)puts("  2. workspace");
    (void)puts("  3. full");
    (void)printf("Current: %s\n", llm_approval_name());

    if (!set_read("Choice> ", input, sizeof(input)) ||
        input[0] == '\0') {
        return 0;
    }

    if (strcmp(input, "1") == 0) {
        value = "read-only";
    } else if (strcmp(input, "2") == 0) {
        value = "workspace";
    } else if (strcmp(input, "3") == 0) {
        value = "full";
    } else {
        (void)puts("Invalid approval-policy choice.");
        return 0;
    }

    if (!settings_set("approval_policy", value) ||
        !llm_set_approval(value)) {
        (void)puts("Unable to set approval policy.");
        return 0;
    }

    (void)printf("Approval policy set to: %s\n", llm_approval_name());
    set_warn_override("approval_policy",
                      "OVMS_AGENT_APPROVAL_POLICY");
    return 1;
}

static int set_prompt_text(const char *key,
                           const char *logical_name,
                           const char *label)
{
    char input[SET_INPUT_MAX];
    const char *current;

    current = settings_effective_text(key, logical_name, "");
    (void)printf("Current value: %s\n", set_text_display(current));
    (void)puts("Press RETURN to keep it; enter NONE to clear it.");
    if (!set_read(label, input, sizeof(input)) || input[0] == '\0') {
        return 0;
    }

    if (set_equal_ci(input, "NONE")) {
        input[0] = '\0';
    }

    if (!settings_set(key, input)) {
        (void)puts("Unable to save setting.");
        return 0;
    }

    (void)printf("Setting saved: %s\n", set_text_display(input));
    set_warn_override(key, logical_name);
    return 1;
}

static void set_apply_defaults(agent_state *state)
{
    char answer[32];
    const char *approval;

    if (!set_read("Restore saved settings to defaults [y/N]? ",
                  answer, sizeof(answer)) ||
        (answer[0] != 'Y' && answer[0] != 'y')) {
        (void)puts("Defaults not restored.");
        return;
    }

    if (!settings_reset()) {
        (void)puts("Unable to restore defaults.");
        return;
    }

    set_apply_state(state);
    approval = settings_effective_text(
        "approval_policy", "OVMS_AGENT_APPROVAL_POLICY", "read-only");
    (void)llm_set_approval(approval);
    (void)puts("Saved settings restored to defaults.");
    (void)puts("Provider profiles and the selected model were not changed.");
}

static void set_reload_now(agent_state *state)
{
    const char *approval;

    if (!settings_reload()) {
        (void)puts("Unable to reload saved settings.");
        return;
    }

    set_apply_state(state);
    approval = settings_effective_text(
        "approval_policy", "OVMS_AGENT_APPROVAL_POLICY", "read-only");
    (void)llm_set_approval(approval);
    (void)puts("Saved settings reloaded.");
}

static void set_interactive(agent_state *state)
{
    char input[64];
    char *end;
    long choice;

    for (;;) {
        (void)putchar('\n');
        set_show_menu(state);
        (void)putchar('\n');
        if (!set_read("Setting> ", input, sizeof(input))) {
            (void)putchar('\n');
            return;
        }

        if (input[0] == '\0') {
            continue;
        }
        if (set_equal_ci(input, "Q") || set_equal_ci(input, "QUIT")) {
            return;
        }
        if (set_equal_ci(input, "R") || set_equal_ci(input, "RELOAD")) {
            set_reload_now(state);
            continue;
        }
        if (set_equal_ci(input, "D") || set_equal_ci(input, "DEFAULTS")) {
            set_apply_defaults(state);
            continue;
        }

        end = NULL;
        choice = strtol(input, &end, 10);
        if (end == input || end == NULL || *end != '\0') {
            (void)puts("Enter a setting number, R, D, or Q.");
            continue;
        }

        switch ((int)choice) {
        case 1:
            (void)set_toggle(state, SET_KIND_WRITES);
            break;
        case 2:
            (void)set_toggle(state, SET_KIND_DCL);
            break;
        case 3:
            (void)set_choose_provider(state);
            break;
        case 4:
            (void)set_prompt_model();
            set_apply_state(state);
            break;
        case 5:
            (void)set_prompt_tokens();
            break;
        case 6:
            (void)set_prompt_approval();
            break;
        case 7:
            (void)set_prompt_text(
                "net_allow", "OVMS_AGENT_NET_ALLOW",
                "Allowed domains> ");
            break;
        case 8:
            (void)set_prompt_text(
                "net_deny", "OVMS_AGENT_NET_DENY",
                "Denied domains> ");
            break;
        default:
            (void)puts("Setting number must be 1 through 8.");
            break;
        }
    }
}

static void set_show_one(const agent_state *state, int kind)
{
    const char *value;
    const char *source;
    long number;

    switch (kind) {
    case SET_KIND_WRITES:
        source = settings_value_source(
            "guarded_writes", "OVMS_AGENT_WRITE_ENABLED");
        (void)printf("guarded_writes=%s source=%s\n",
            state != NULL && state->write_enabled ? "ON" : "OFF", source);
        break;
    case SET_KIND_DCL:
        source = settings_value_source(
            "dcl_execution", "OVMS_AGENT_DCL_ENABLED");
        (void)printf("dcl_execution=%s source=%s\n",
            state != NULL && state->dcl_enabled ? "ON" : "OFF", source);
        break;
    case SET_KIND_PROVIDER:
        (void)printf("provider=%s\n", set_active_provider());
        break;
    case SET_KIND_MODEL:
        (void)printf("model=%s\n", set_active_model());
        break;
    case SET_KIND_TOKENS:
        number = settings_effective_long(
            "max_output_tokens", "OVMS_AGENT_MAX_OUTPUT_TOKENS",
            2048L, SET_TOKEN_MIN, SET_TOKEN_MAX);
        source = settings_value_source(
            "max_output_tokens", "OVMS_AGENT_MAX_OUTPUT_TOKENS");
        (void)printf("max_output_tokens=%ld source=%s\n", number, source);
        break;
    case SET_KIND_APPROVAL:
        source = settings_value_source(
            "approval_policy", "OVMS_AGENT_APPROVAL_POLICY");
        (void)printf("approval_policy=%s source=%s\n",
                     llm_approval_name(), source);
        break;
    case SET_KIND_NET_ALLOW:
        value = settings_effective_text(
            "net_allow", "OVMS_AGENT_NET_ALLOW", "");
        source = settings_value_source("net_allow", "OVMS_AGENT_NET_ALLOW");
        (void)printf("net_allow=%s source=%s\n", value, source);
        break;
    case SET_KIND_NET_DENY:
        value = settings_effective_text(
            "net_deny", "OVMS_AGENT_NET_DENY", "");
        source = settings_value_source("net_deny", "OVMS_AGENT_NET_DENY");
        (void)printf("net_deny=%s source=%s\n", value, source);
        break;
    default:
        (void)puts("Unknown setting name.");
        break;
    }
}

static int set_noninteractive(agent_state *state,
                              int kind,
                              const char *value)
{
    int enabled;
    char *end;
    long number;

    switch (kind) {
    case SET_KIND_WRITES:
        if (!set_parse_bool(value, &enabled) ||
            !settings_set_bool("guarded_writes", enabled)) {
            return 0;
        }
        set_apply_state(state);
        set_warn_override("guarded_writes", "OVMS_AGENT_WRITE_ENABLED");
        return 1;
    case SET_KIND_DCL:
        if (!set_parse_bool(value, &enabled) ||
            !settings_set_bool("dcl_execution", enabled)) {
            return 0;
        }
        set_apply_state(state);
        set_warn_override("dcl_execution", "OVMS_AGENT_DCL_ENABLED");
        return 1;
    case SET_KIND_PROVIDER:
        if (value == NULL || *value == '\0' || !llm_prov_use(value)) {
            return 0;
        }
        set_apply_state(state);
        return 1;
    case SET_KIND_MODEL:
        if (value == NULL || *value == '\0' || !llm_set_model(value)) {
            return 0;
        }
        set_apply_state(state);
        return 1;
    case SET_KIND_TOKENS:
        if (value == NULL || *value == '\0') {
            return 0;
        }
        end = NULL;
        number = strtol(value, &end, 10);
        if (end == value || end == NULL || *end != '\0' ||
            number < SET_TOKEN_MIN || number > SET_TOKEN_MAX) {
            return 0;
        }
        if (!settings_set_long("max_output_tokens", number)) {
            return 0;
        }
        set_warn_override("max_output_tokens",
                          "OVMS_AGENT_MAX_OUTPUT_TOKENS");
        return 1;
    case SET_KIND_APPROVAL:
        if (value == NULL || *value == '\0' ||
            !llm_set_approval(value) ||
            !settings_set("approval_policy", llm_approval_name())) {
            return 0;
        }
        set_warn_override("approval_policy", "OVMS_AGENT_APPROVAL_POLICY");
        return 1;
    case SET_KIND_NET_ALLOW:
        if (value == NULL) {
            return 0;
        }
        if (set_equal_ci(value, "NONE")) {
            value = "";
        }
        if (!settings_set("net_allow", value)) {
            return 0;
        }
        set_warn_override("net_allow", "OVMS_AGENT_NET_ALLOW");
        return 1;
    case SET_KIND_NET_DENY:
        if (value == NULL) {
            return 0;
        }
        if (set_equal_ci(value, "NONE")) {
            value = "";
        }
        if (!settings_set("net_deny", value)) {
            return 0;
        }
        set_warn_override("net_deny", "OVMS_AGENT_NET_DENY");
        return 1;
    default:
        return 0;
    }
}

static void command_settings(agent_state *state,
                             const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *verb;
    char *name;
    char *value;
    char *extra;
    int kind;

    if (arguments == NULL || *arguments == '\0') {
        set_interactive(state);
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("SETTINGS command is too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    verb = command_next_argument(&cursor);

    if (verb == NULL) {
        set_interactive(state);
        return;
    }

    if (set_equal_ci(verb, "SHOW")) {
        extra = command_next_argument(&cursor);
        if (extra != NULL) {
            (void)puts("Usage: SETTINGS SHOW");
            return;
        }
        set_show_menu(state);
        return;
    }

    if (set_equal_ci(verb, "GET")) {
        name = command_next_argument(&cursor);
        extra = command_next_argument(&cursor);
        kind = set_kind(name);
        if (name == NULL || extra != NULL || kind == SET_KIND_UNKNOWN) {
            (void)puts("Usage: SETTINGS GET name");
            return;
        }
        set_show_one(state, kind);
        return;
    }

    if (set_equal_ci(verb, "TOGGLE")) {
        name = command_next_argument(&cursor);
        extra = command_next_argument(&cursor);
        kind = set_kind(name);
        if (name == NULL || extra != NULL ||
            (kind != SET_KIND_WRITES && kind != SET_KIND_DCL)) {
            (void)puts("Usage: SETTINGS TOGGLE guarded_writes|dcl_execution");
            return;
        }
        (void)set_toggle(state, kind);
        return;
    }

    if (set_equal_ci(verb, "SET")) {
        name = command_next_argument(&cursor);
        value = command_next_argument(&cursor);
        extra = command_next_argument(&cursor);
        kind = set_kind(name);
        if (name == NULL || value == NULL || extra != NULL ||
            kind == SET_KIND_UNKNOWN) {
            (void)puts("Usage: SETTINGS SET name value");
            return;
        }
        if (!set_noninteractive(state, kind, value)) {
            (void)printf("Unable to set %s.\n", set_kind_name(kind));
            return;
        }
        (void)printf("Setting saved: %s\n", set_kind_name(kind));
        return;
    }

    if (set_equal_ci(verb, "RESET")) {
        extra = command_next_argument(&cursor);
        if (extra != NULL) {
            (void)puts("Usage: SETTINGS RESET");
            return;
        }
        set_apply_defaults(state);
        return;
    }

    if (set_equal_ci(verb, "RELOAD")) {
        extra = command_next_argument(&cursor);
        if (extra != NULL) {
            (void)puts("Usage: SETTINGS RELOAD");
            return;
        }
        set_reload_now(state);
        return;
    }

    (void)puts(
        "Usage: SETTINGS [SHOW|GET name|SET name value|TOGGLE name|RESET|RELOAD]"
    );
}

static const command_entry settings_commands[] = {
    { "SETTINGS", "View or change persistent OVMS Agent settings",
      command_settings }
};

void command_register_settings(void)
{
    (void)command_registry_add(
        settings_commands,
        sizeof(settings_commands) / sizeof(settings_commands[0])
    );
}
