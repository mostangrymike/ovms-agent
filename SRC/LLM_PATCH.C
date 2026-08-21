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
