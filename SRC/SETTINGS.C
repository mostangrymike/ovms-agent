#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "SETTINGS.H"

#define SETTINGS_FILE "SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT"
#define SETTINGS_LINE_MAX 1200U
#define SETTINGS_VALUE_MAX 1024U
#define SETTINGS_PATH_MAX 1024U

typedef struct setting_entry {
    const char *key;
    const char *default_value;
    char value[SETTINGS_VALUE_MAX];
    int saved;
} setting_entry;

static setting_entry setting_entries[] = {
    { "guarded_writes", "OFF", "", 0 },
    { "dcl_execution", "OFF", "", 0 },
    { "max_output_tokens", "2048", "", 0 },
    { "approval_policy", "read-only", "", 0 },
    { "net_allow", "", "", 0 },
    { "net_deny", "", "", 0 }
};

static int settings_loaded;
static char settings_test_file[SETTINGS_PATH_MAX];

static const char *settings_file_name(void)
{
    return settings_test_file[0] != '\0' ?
        settings_test_file : SETTINGS_FILE;
}

static size_t settings_count(void)
{
    return sizeof(setting_entries) / sizeof(setting_entries[0]);
}

static void settings_copy(char *destination,
                          size_t destination_size,
                          const char *source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    (void)strncpy(destination, source, destination_size - 1U);
    destination[destination_size - 1U] = '\0';
}

static void settings_chomp(char *text)
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

static void settings_trim(char *text)
{
    char *start;
    size_t length;

    if (text == NULL) {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    if (start != text) {
        (void)memmove(text, start, strlen(start) + 1U);
    }

    length = strlen(text);
    while (length > 0U &&
           isspace((unsigned char)text[length - 1U])) {
        text[--length] = '\0';
    }
}

static setting_entry *settings_find(const char *key)
{
    size_t index;

    if (key == NULL || *key == '\0') {
        return NULL;
    }

    for (index = 0U; index < settings_count(); ++index) {
        if (strcmp(setting_entries[index].key, key) == 0) {
            return &setting_entries[index];
        }
    }

    return NULL;
}

static void settings_defaults(void)
{
    size_t index;

    for (index = 0U; index < settings_count(); ++index) {
        settings_copy(setting_entries[index].value,
                      sizeof(setting_entries[index].value),
                      setting_entries[index].default_value);
        setting_entries[index].saved = 0;
    }
}

int settings_load(void)
{
    FILE *file;
    char line[SETTINGS_LINE_MAX];

    if (settings_loaded) {
        return 1;
    }

    settings_defaults();
    file = fopen(settings_file_name(), "r");
    if (file == NULL) {
        settings_loaded = 1;
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        setting_entry *entry;

        settings_chomp(line);
        settings_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');
        if (equals == NULL) {
            continue;
        }

        *equals++ = '\0';
        settings_trim(line);
        settings_trim(equals);
        entry = settings_find(line);
        if (entry == NULL || strlen(equals) >= sizeof(entry->value)) {
            continue;
        }

        settings_copy(entry->value, sizeof(entry->value), equals);
        entry->saved = 1;
    }

    (void)fclose(file);
    settings_loaded = 1;
    return 1;
}

int settings_reload(void)
{
    settings_loaded = 0;
    return settings_load();
}

int settings_save(void)
{
    FILE *file;
    size_t index;

    if (!settings_load()) {
        return 0;
    }

    file = fopen(settings_file_name(), "w");
    if (file == NULL) {
        return 0;
    }

    (void)fputs("# OVMS Agent user settings\n", file);
    (void)fputs("# Managed by the SETTINGS command.\n", file);

    for (index = 0U; index < settings_count(); ++index) {
        (void)fprintf(file, "%s=%s\n",
                      setting_entries[index].key,
                      setting_entries[index].value);
        setting_entries[index].saved = 1;
    }

    if (fclose(file) != 0) {
        return 0;
    }

    if (chmod(settings_file_name(), 0600) != 0) {
        return 0;
    }

    return 1;
}

int settings_reset(void)
{
    size_t index;

    settings_defaults();
    settings_loaded = 1;
    for (index = 0U; index < settings_count(); ++index) {
        setting_entries[index].saved = 1;
    }
    return settings_save();
}

const char *settings_get(const char *key)
{
    setting_entry *entry;

    (void)settings_load();
    entry = settings_find(key);
    return entry != NULL ? entry->value : NULL;
}

int settings_is_saved(const char *key)
{
    setting_entry *entry;

    (void)settings_load();
    entry = settings_find(key);
    return entry != NULL && entry->saved;
}

int settings_set(const char *key, const char *value)
{
    setting_entry *entry;

    if (value == NULL || strlen(value) >= SETTINGS_VALUE_MAX) {
        return 0;
    }

    (void)settings_load();
    entry = settings_find(key);
    if (entry == NULL) {
        return 0;
    }

    settings_copy(entry->value, sizeof(entry->value), value);
    entry->saved = 1;
    return settings_save();
}

int settings_set_bool(const char *key, int enabled)
{
    return settings_set(key, enabled ? "ON" : "OFF");
}

int settings_set_long(const char *key, long value)
{
    char text[64];

    (void)sprintf(text, "%ld", value);
    return settings_set(key, text);
}

static int settings_true(const char *value, int fallback)
{
    char normalized[16];
    size_t index;

    if (value == NULL || *value == '\0') {
        return fallback;
    }

    index = 0U;
    while (value[index] != '\0' && index + 1U < sizeof(normalized)) {
        normalized[index] =
            (char)toupper((unsigned char)value[index]);
        ++index;
    }
    if (value[index] != '\0') {
        return fallback;
    }
    normalized[index] = '\0';

    if (strcmp(normalized, "YES") == 0 ||
        strcmp(normalized, "TRUE") == 0 ||
        strcmp(normalized, "1") == 0 ||
        strcmp(normalized, "ON") == 0) {
        return 1;
    }

    if (strcmp(normalized, "NO") == 0 ||
        strcmp(normalized, "FALSE") == 0 ||
        strcmp(normalized, "0") == 0 ||
        strcmp(normalized, "OFF") == 0) {
        return 0;
    }

    return fallback;
}

const char *settings_effective_text(const char *key,
                                    const char *logical_name,
                                    const char *fallback)
{
    const char *logical_value;
    const char *saved_value;

    logical_value = logical_name != NULL ? getenv(logical_name) : NULL;
    if (logical_value != NULL && *logical_value != '\0') {
        return logical_value;
    }

    saved_value = settings_get(key);
    if (saved_value != NULL) {
        return saved_value;
    }

    return fallback != NULL ? fallback : "";
}

int settings_effective_bool(const char *key,
                            const char *logical_name,
                            int fallback)
{
    return settings_true(
        settings_effective_text(key, logical_name, fallback ? "ON" : "OFF"),
        fallback
    );
}

long settings_effective_long(const char *key,
                             const char *logical_name,
                             long fallback,
                             long minimum,
                             long maximum)
{
    const char *value;
    char *end;
    long parsed;

    value = settings_effective_text(key, logical_name, "");
    if (value == NULL || *value == '\0') {
        return fallback;
    }

    end = NULL;
    parsed = strtol(value, &end, 10);
    if (end == value || end == NULL || *end != '\0') {
        return fallback;
    }

    if (parsed < minimum) {
        return minimum;
    }
    if (parsed > maximum) {
        return maximum;
    }
    return parsed;
}

const char *settings_value_source(const char *key,
                                  const char *logical_name)
{
    const char *logical_value;
    setting_entry *entry;

    logical_value = logical_name != NULL ? getenv(logical_name) : NULL;
    if (logical_value != NULL && *logical_value != '\0') {
        return "logical override";
    }

    (void)settings_load();
    entry = settings_find(key);
    if (entry != NULL && entry->saved) {
        return "saved";
    }

    return "default";
}

void settings_test_path(const char *path)
{
    if (path == NULL || *path == '\0') {
        settings_test_file[0] = '\0';
    } else {
        settings_copy(settings_test_file,
                      sizeof(settings_test_file), path);
    }
    settings_loaded = 0;
}