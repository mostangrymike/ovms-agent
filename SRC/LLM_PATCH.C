#include "LLM_PATCH.H"

#define openai_patch_apply_json llm_patch_apply_json
#define openai_patch_validate llm_patch_validate
#define openai_patch_dry llm_patch_dry
#define openai_patch_apply llm_patch_apply
#define openai_patch_last_text llm_patch_last_text
#define openai_patch_validate_cmd llm_patch_validate_cmd
#define openai_patch_dry_cmd llm_patch_dry_cmd
#define openai_patch_apply_cmd llm_patch_apply_cmd
#define openai_patch_last_cmd llm_patch_last_cmd

#include "LLM_PATCH_CORE.INC"

#undef openai_patch_last_cmd
#undef openai_patch_apply_cmd
#undef openai_patch_dry_cmd
#undef openai_patch_validate_cmd
#undef openai_patch_last_text
#undef openai_patch_apply
#undef openai_patch_dry
#undef openai_patch_validate
#undef openai_patch_apply_json

/* Temporary compatibility wrappers for broad legacy consumers. */
int openai_patch_apply_json(const char *a, char *o, size_t n)
{ return llm_patch_apply_json(a, o, n); }
int openai_patch_validate(const char *p, char *o, size_t n)
{ return llm_patch_validate(p, o, n); }
int openai_patch_dry(const char *p, char *o, size_t n)
{ return llm_patch_dry(p, o, n); }
int openai_patch_apply(const char *p, char *o, size_t n)
{ return llm_patch_apply(p, o, n); }
int openai_patch_last_text(char *o, size_t n)
{ return llm_patch_last_text(o, n); }
void openai_patch_validate_cmd(const char *a) { llm_patch_validate_cmd(a); }
void openai_patch_dry_cmd(const char *a) { llm_patch_dry_cmd(a); }
void openai_patch_apply_cmd(const char *a) { llm_patch_apply_cmd(a); }
void openai_patch_last_cmd(void) { llm_patch_last_cmd(); }
