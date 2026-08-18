#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "llm_config.h"

#define GH_CFG_FILE "SYS$LOGIN:OVMS_AGENT_GITHUB.DAT"
#define GH_CFG_LINE 1024U

static gh_profile gh_profiles[GH_PROF_MAX];
static unsigned int gh_profile_count = 0U;
static int gh_active_index = -1;
static int gh_profiles_loaded = 0;

static void gh_cfg_copy(char *dest,
                        size_t dest_size,
                        const char *source)
{
    if (dest == NULL || dest_size == 0U) return;
    if (source == NULL) { dest[0] = '\0'; return; }
    (void)strncpy(dest, source, dest_size - 1U);
    dest[dest_size - 1U] = '\0';
}

static void gh_cfg_chomp(char *text)
{
    size_t length;
    if (text == NULL) return;
    length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\n' || text[length - 1U] == '\r')) {
        text[--length] = '\0';
    }
}

static int gh_cfg_find(const char *name)
{
    unsigned int index;
    if (name == NULL || *name == '\0') return -1;
    for (index = 0U; index < gh_profile_count; ++index) {
        if (strcmp(gh_profiles[index].name, name) == 0) return (int)index;
    }
    return -1;
}

static int gh_cfg_slot(const char *name)
{
    int found;
    unsigned int index;
    found = gh_cfg_find(name);
    if (found >= 0) return found;
    if (gh_profile_count >= GH_PROF_MAX) return -1;
    index = gh_profile_count++;
    (void)memset(&gh_profiles[index], 0, sizeof(gh_profiles[index]));
    gh_cfg_copy(gh_profiles[index].name, sizeof(gh_profiles[index].name), name);
    return (int)index;
}

static int gh_cfg_load(void)
{
    FILE *file;
    char line[GH_CFG_LINE];
    char active[GH_PROF_NAME_MAX];
    int section;
    if (gh_profiles_loaded) return 1;
    gh_profile_count = 0U;
    gh_active_index = -1;
    active[0] = '\0';
    section = -1;
    file = fopen(GH_CFG_FILE, "r");
    if (file == NULL) { gh_profiles_loaded = 1; return 1; }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *value;
        size_t length;
        gh_cfg_chomp(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        if (strncmp(line, "active=", 7U) == 0) {
            gh_cfg_copy(active, sizeof(active), line + 7);
            continue;
        }
        if (strncmp(line, "[github ", 8U) == 0) {
            length = strlen(line);
            if (length > 9U && line[length - 1U] == ']') {
                line[length - 1U] = '\0';
                section = gh_cfg_slot(line + 8);
            } else section = -1;
            continue;
        }
        if (section < 0) continue;
        value = strchr(line, '=');
        if (value == NULL) continue;
        *value++ = '\0';
        if (strcmp(line, "repo") == 0) {
            gh_cfg_copy(gh_profiles[section].repo, sizeof(gh_profiles[section].repo), value);
        } else if (strcmp(line, "user") == 0) {
            gh_cfg_copy(gh_profiles[section].user, sizeof(gh_profiles[section].user), value);
        } else if (strcmp(line, "branch") == 0) {
            gh_cfg_copy(gh_profiles[section].branch, sizeof(gh_profiles[section].branch), value);
        } else if (strcmp(line, "token") == 0) {
            gh_cfg_copy(gh_profiles[section].token, sizeof(gh_profiles[section].token), value);
        }
    }
    (void)fclose(file);
    gh_profiles_loaded = 1;
    if (active[0] != '\0') gh_active_index = gh_cfg_find(active);
    return 1;
}

static int gh_cfg_save(void)
{
    FILE *file;
    unsigned int index;
    if (!gh_cfg_load()) return 0;
    file = fopen(GH_CFG_FILE, "w");
    if (file == NULL) return 0;
    (void)fputs("# OVMS Agent GitHub profiles\n", file);
    (void)fputs("# Tokens are intentionally stored here.\n", file);
    if (gh_active_index >= 0) {
        (void)fprintf(file, "active=%s\n\n", gh_profiles[gh_active_index].name);
    } else (void)fputs("active=\n\n", file);
    for (index = 0U; index < gh_profile_count; ++index) {
        (void)fprintf(file, "[github %s]\n", gh_profiles[index].name);
        (void)fprintf(file, "repo=%s\n", gh_profiles[index].repo);
        (void)fprintf(file, "user=%s\n", gh_profiles[index].user);
        (void)fprintf(file, "branch=%s\n", gh_profiles[index].branch);
        (void)fprintf(file, "token=%s\n\n", gh_profiles[index].token);
    }
    if (fclose(file) != 0) return 0;
    if (chmod(GH_CFG_FILE, 0600) != 0) return 0;
    return 1;
}

unsigned int gh_prof_count(void) { (void)gh_cfg_load(); return gh_profile_count; }
const gh_profile *gh_prof_get(unsigned int index)
{
    (void)gh_cfg_load();
    if (index >= gh_profile_count) return NULL;
    return &gh_profiles[index];
}
const gh_profile *gh_prof_active(void)
{
    (void)gh_cfg_load();
    if (gh_active_index < 0) return NULL;
    return &gh_profiles[gh_active_index];
}
int gh_prof_add(const char *name, const char *repo, const char *user,
                const char *branch, const char *token)
{
    int index;
    if (name == NULL || *name == '\0' || repo == NULL || *repo == '\0' ||
        user == NULL || *user == '\0' || branch == NULL || *branch == '\0' ||
        token == NULL || *token == '\0') return 0;
    if (strlen(name) >= GH_PROF_NAME_MAX || strlen(repo) >= GH_PROF_REPO_MAX ||
        strlen(user) >= GH_PROF_USER_MAX || strlen(branch) >= GH_PROF_BRANCH_MAX ||
        strlen(token) >= GH_PROF_TOKEN_MAX) return 0;
    (void)gh_cfg_load();
    index = gh_cfg_slot(name);
    if (index < 0) return 0;
    gh_cfg_copy(gh_profiles[index].repo, sizeof(gh_profiles[index].repo), repo);
    gh_cfg_copy(gh_profiles[index].user, sizeof(gh_profiles[index].user), user);
    gh_cfg_copy(gh_profiles[index].branch, sizeof(gh_profiles[index].branch), branch);
    gh_cfg_copy(gh_profiles[index].token, sizeof(gh_profiles[index].token), token);
    if (gh_active_index < 0) gh_active_index = index;
    return gh_cfg_save();
}
int gh_prof_use(const char *name)
{
    int index;
    (void)gh_cfg_load();
    index = gh_cfg_find(name);
    if (index < 0) return 0;
    gh_active_index = index;
    return gh_cfg_save();
}
int gh_prof_delete(const char *name)
{
    int index;
    unsigned int move;
    (void)gh_cfg_load();
    index = gh_cfg_find(name);
    if (index < 0) return 0;
    for (move = (unsigned int)index; move + 1U < gh_profile_count; ++move)
        gh_profiles[move] = gh_profiles[move + 1U];
    --gh_profile_count;
    (void)memset(&gh_profiles[gh_profile_count], 0, sizeof(gh_profiles[gh_profile_count]));
    if (gh_active_index == index) gh_active_index = gh_profile_count > 0U ? 0 : -1;
    else if (gh_active_index > index) --gh_active_index;
    return gh_cfg_save();
}
