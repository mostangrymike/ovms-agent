#ifndef LLM_CONFIG_H
#define LLM_CONFIG_H

#define LLM_PROV_NAME_MAX 64U
#define LLM_PROV_URL_MAX 512U
#define LLM_PROV_KEY_MAX 512U
#define LLM_PROV_MAX 64U

#define GH_PROF_NAME_MAX 64U
#define GH_PROF_REPO_MAX 256U
#define GH_PROF_USER_MAX 128U
#define GH_PROF_BRANCH_MAX 128U
#define GH_PROF_TOKEN_MAX 512U
#define GH_PROF_MAX 64U

typedef struct llm_provider {
    char name[LLM_PROV_NAME_MAX];
    char url[LLM_PROV_URL_MAX];
    char model[128];
    char api_key[LLM_PROV_KEY_MAX];
} llm_provider;

typedef struct gh_profile {
    char name[GH_PROF_NAME_MAX];
    char repo[GH_PROF_REPO_MAX];
    char user[GH_PROF_USER_MAX];
    char branch[GH_PROF_BRANCH_MAX];
    char token[GH_PROF_TOKEN_MAX];
} gh_profile;

const char *llm_model(void);
int llm_set_model(const char *model);
int llm_model_overridden(void);

int llm_prov_load(void);
int llm_prov_save(void);
unsigned int llm_prov_count(void);
const llm_provider *llm_prov_get(unsigned int index);
const llm_provider *llm_prov_active(void);
int llm_prov_add(const char *name,
                 const char *url,
                 const char *model,
                 const char *api_key);
int llm_prov_use(const char *name);
int llm_prov_delete(const char *name);
const char *llm_api_url(void);
const char *llm_api_key(void);

unsigned int gh_prof_count(void);
const gh_profile *gh_prof_get(unsigned int index);
const gh_profile *gh_prof_active(void);
int gh_prof_add(const char *name,
                const char *repo,
                const char *user,
                const char *branch,
                const char *token);
int gh_prof_use(const char *name);
int gh_prof_delete(const char *name);

#endif /* LLM_CONFIG_H */
