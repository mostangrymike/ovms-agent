#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "llm_config.h"

#define LLM_MODEL_MAX 128U
#define LLM_CFG_FILE "SYS$LOGIN:OVMS_AGENT_CONFIG.DAT"
#define LLM_CFG_LINE 1024U

static char llm_model_value[LLM_MODEL_MAX];
static int llm_model_has_override = 0;
static llm_provider llm_profiles[LLM_PROV_MAX];
static unsigned int llm_profile_count = 0U;
static int llm_active_index = -1;
static int llm_profiles_loaded = 0;

static void llm_cfg_copy(char *dest,
                         size_t dest_size,
                         const char *source)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }

    if (source == NULL) {
        dest[0] = '\0';
        return;
    }

    (void)strncpy(dest, source, dest_size - 1U);
    dest[dest_size - 1U] = '\0';
}

static void llm_cfg_chomp(char *text)
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

static void llm_cfg_trim(char *text)
{
    char *start;
    size_t length;

    if (text == NULL) {
        return;
    }

    start = text;
    while (*start != '\0' &&
           isspace((unsigned char)*start)) {
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

static int llm_cfg_find(const char *name)
{
    unsigned int index;

    if (name == NULL || *name == '\0') {
        return -1;
    }

    for (index = 0U; index < llm_profile_count; ++index) {
        if (strcmp(llm_profiles[index].name, name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int llm_cfg_slot(const char *name)
{
    int found;
    unsigned int index;

    found = llm_cfg_find(name);
    if (found >= 0) {
        return found;
    }

    if (llm_profile_count >= LLM_PROV_MAX) {
        return -1;
    }

    index = llm_profile_count++;
    (void)memset(&llm_profiles[index], 0,
                 sizeof(llm_profiles[index]));
    llm_cfg_copy(llm_profiles[index].name,
                 sizeof(llm_profiles[index].name), name);
    return (int)index;
}

int llm_prov_load(void)
{
    FILE *file;
    char line[LLM_CFG_LINE];
    char active[LLM_PROV_NAME_MAX];
    int section;

    if (llm_profiles_loaded) {
        return 1;
    }

    llm_profile_count = 0U;
    llm_active_index = -1;
    active[0] = '\0';
    section = -1;

    file = fopen(LLM_CFG_FILE, "r");
    if (file == NULL) {
        llm_profiles_loaded = 1;
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *value;
        size_t length;

        llm_cfg_chomp(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if (strncmp(line, "active=", 7U) == 0) {
            llm_cfg_copy(active, sizeof(active), line + 7);
            continue;
        }

        if (strncmp(line, "[provider ", 10U) == 0) {
            length = strlen(line);
            if (length > 11U && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                section = llm_cfg_slot(line + 10);
            } else {
                section = -1;
            }
            continue;
        }

        if (section < 0) {
            continue;
        }

        value = strchr(line, '=');
        if (value == NULL) {
            continue;
        }

        *value++ = '\0';

        if (strcmp(line, "url") == 0) {
            llm_cfg_copy(llm_profiles[section].url,
                         sizeof(llm_profiles[section].url), value);
            llm_cfg_trim(llm_profiles[section].url);
        } else if (strcmp(line, "model") == 0) {
            llm_cfg_copy(llm_profiles[section].model,
                         sizeof(llm_profiles[section].model), value);
            llm_cfg_trim(llm_profiles[section].model);
        } else if (strcmp(line, "key") == 0) {
            llm_cfg_copy(llm_profiles[section].api_key,
                         sizeof(llm_profiles[section].api_key), value);
        }
    }

    (void)fclose(file);
    llm_profiles_loaded = 1;

    if (active[0] != '\0') {
        llm_active_index = llm_cfg_find(active);
    }

    if (llm_active_index >= 0 &&
        llm_profiles[llm_active_index].model[0] != '\0') {
        llm_cfg_copy(llm_model_value,
                     sizeof(llm_model_value),
                     llm_profiles[llm_active_index].model);
        llm_model_has_override = 1;
    }

    return 1;
}

int llm_prov_save(void)
{
    FILE *file;
    unsigned int index;

    if (!llm_prov_load()) {
        return 0;
    }

    file = fopen(LLM_CFG_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    (void)fputs("# OVMS Agent persistent configuration\n", file);
    (void)fputs("# Secrets are intentionally stored here.\n", file);

    if (llm_active_index >= 0) {
        (void)fprintf(file, "active=%s\n\n",
                      llm_profiles[llm_active_index].name);
    } else {
        (void)fputs("active=\n\n", file);
    }

    for (index = 0U; index < llm_profile_count; ++index) {
        (void)fprintf(file, "[provider %s]\n",
                      llm_profiles[index].name);
        (void)fprintf(file, "url=%s\n",
                      llm_profiles[index].url);
        (void)fprintf(file, "model=%s\n",
                      llm_profiles[index].model);
        (void)fprintf(file, "key=%s\n\n",
                      llm_profiles[index].api_key);
    }

    if (fclose(file) != 0) {
        return 0;
    }

    if (chmod(LLM_CFG_FILE, 0600) != 0) {
        return 0;
    }

    return 1;
}

unsigned int llm_prov_count(void)
{
    (void)llm_prov_load();
    return llm_profile_count;
}

const llm_provider *llm_prov_get(unsigned int index)
{
    (void)llm_prov_load();

    if (index >= llm_profile_count) {
        return NULL;
    }

    return &llm_profiles[index];
}

const llm_provider *llm_prov_active(void)
{
    (void)llm_prov_load();

    if (llm_active_index < 0) {
        return NULL;
    }

    return &llm_profiles[llm_active_index];
}

int llm_prov_add(const char *name,
                 const char *url,
                 const char *model,
                 const char *api_key)
{
    int index;
    char clean_url[LLM_PROV_URL_MAX];
    char clean_model[LLM_MODEL_MAX];

    if (name == NULL || *name == '\0' ||
        url == NULL || *url == '\0' ||
        model == NULL || *model == '\0' ||
        api_key == NULL || *api_key == '\0') {
        return 0;
    }

    if (strlen(name) >= LLM_PROV_NAME_MAX ||
        strlen(url) >= LLM_PROV_URL_MAX ||
        strlen(model) >= LLM_MODEL_MAX ||
        strlen(api_key) >= LLM_PROV_KEY_MAX) {
        return 0;
    }

    llm_cfg_copy(clean_url, sizeof(clean_url), url);
    llm_cfg_copy(clean_model, sizeof(clean_model), model);
    llm_cfg_trim(clean_url);
    llm_cfg_trim(clean_model);

    if (clean_url[0] == '\0' || clean_model[0] == '\0') {
        return 0;
    }

    (void)llm_prov_load();
    index = llm_cfg_slot(name);
    if (index < 0) {
        return 0;
    }

    llm_cfg_copy(llm_profiles[index].url,
                 sizeof(llm_profiles[index].url), clean_url);
    llm_cfg_copy(llm_profiles[index].model,
                 sizeof(llm_profiles[index].model), clean_model);
    llm_cfg_copy(llm_profiles[index].api_key,
                 sizeof(llm_profiles[index].api_key), api_key);

    if (llm_active_index < 0) {
        llm_active_index = index;
    }

    return llm_prov_save();
}

int llm_prov_use(const char *name)
{
    int index;

    (void)llm_prov_load();
    index = llm_cfg_find(name);
    if (index < 0) {
        return 0;
    }

    llm_active_index = index;
    llm_cfg_copy(llm_model_value,
                 sizeof(llm_model_value),
                 llm_profiles[index].model);
    llm_model_has_override =
        llm_model_value[0] != '\0';

    return llm_prov_save();
}

int llm_prov_delete(const char *name)
{
    int index;
    unsigned int move;

    (void)llm_prov_load();
    index = llm_cfg_find(name);
    if (index < 0) {
        return 0;
    }

    for (move = (unsigned int)index;
         move + 1U < llm_profile_count;
         ++move) {
        llm_profiles[move] = llm_profiles[move + 1U];
    }

    --llm_profile_count;
    (void)memset(&llm_profiles[llm_profile_count], 0,
                 sizeof(llm_profiles[llm_profile_count]));

    if (llm_active_index == index) {
        llm_active_index = llm_profile_count > 0U ? 0 : -1;
    } else if (llm_active_index > index) {
        --llm_active_index;
    }

    if (llm_active_index >= 0) {
        llm_cfg_copy(llm_model_value,
                     sizeof(llm_model_value),
                     llm_profiles[llm_active_index].model);
        llm_model_has_override = llm_model_value[0] != '\0';
    } else {
        llm_model_value[0] = '\0';
        llm_model_has_override = 0;
    }

    return llm_prov_save();
}

const char *llm_model(void)
{
    (void)llm_prov_load();

    if (!llm_model_has_override) {
        return NULL;
    }

    return llm_model_value;
}

int llm_set_model(const char *model)
{
    char clean_model[LLM_MODEL_MAX];

    if (model == NULL || *model == '\0' ||
        strlen(model) >= sizeof(clean_model)) {
        return 0;
    }

    llm_cfg_copy(clean_model, sizeof(clean_model), model);
    llm_cfg_trim(clean_model);
    if (clean_model[0] == '\0') {
        return 0;
    }

    (void)llm_prov_load();
    llm_cfg_copy(llm_model_value,
                 sizeof(llm_model_value), clean_model);
    llm_model_has_override = 1;

    if (llm_active_index >= 0) {
        llm_cfg_copy(llm_profiles[llm_active_index].model,
                     sizeof(llm_profiles[llm_active_index].model),
                     clean_model);
        return llm_prov_save();
    }

    return 1;
}

int llm_model_overridden(void)
{
    return llm_model_has_override;
}

const char *llm_api_url(void)
{
    const llm_provider *provider;

    provider = llm_prov_active();
    if (provider == NULL || provider->url[0] == '\0') {
        return NULL;
    }

    return provider->url;
}

const char *llm_api_key(void)
{
    const llm_provider *provider;

    provider = llm_prov_active();
    if (provider == NULL || provider->api_key[0] == '\0') {
        return NULL;
    }

    return provider->api_key;
}
