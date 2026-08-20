#ifndef LLM_PATCH_H
#define LLM_PATCH_H

#include <stddef.h>

int llm_patch_apply_json(const char *arguments,
                         char *output,
                         size_t output_size);
int llm_patch_validate(const char *path, char *output, size_t output_size);
int llm_patch_dry(const char *path, char *output, size_t output_size);
int llm_patch_apply(const char *path, char *output, size_t output_size);
int llm_patch_last_text(char *output, size_t output_size);
void llm_patch_validate_cmd(const char *arguments);
void llm_patch_dry_cmd(const char *arguments);
void llm_patch_apply_cmd(const char *arguments);
void llm_patch_last_cmd(void);

#endif /* LLM_PATCH_H */
