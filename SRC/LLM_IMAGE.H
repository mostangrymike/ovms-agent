#ifndef LLM_IMAGE_H
#define LLM_IMAGE_H

#include <stdio.h>
#include <stddef.h>

#define LLM_IMAGE_PATH_MAX 512U

typedef struct llm_image_meta {
    char path[LLM_IMAGE_PATH_MAX];
    char media_type[24];
    unsigned long size;
} llm_image_meta;

int llm_image_info(const char *path, llm_image_meta *meta);
int llm_image_write_data(FILE *output,
                         const char *path,
                         llm_image_meta *meta);

#endif /* LLM_IMAGE_H */
