#ifndef LLM_CONFIG_H
#define LLM_CONFIG_H

#define LLM_PROV_NAME_MAX 64U
#define LLM_PROV_URL_MAX 512U
#define LLM_PROV_KEY_MAX 512U
#define LLM_PROV_MAX 64U

typedef struct llm_provider {
    char name[LLM_PROV_NAME_MAX];
    char url[LLM_PROV_URL_MAX];
    char model[128];
    char api_key[LLM_PROV_KEY_MAX];
} llm_provider;

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

#endif /* LLM_CONFIG_H */
