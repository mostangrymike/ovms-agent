#ifndef OPENAI_IMAGE_H
#define OPENAI_IMAGE_H

#include <stdio.h>
#include <stddef.h>

#define OPENAI_IMAGE_PATH_MAX 512U

typedef struct openai_image_meta {
    char path[OPENAI_IMAGE_PATH_MAX];
    char media_type[24];
    unsigned long size;
} openai_image_meta;

int openai_image_info(const char *path, openai_image_meta *meta);
int openai_image_write_data(FILE *output,
                            const char *path,
                            openai_image_meta *meta);

#endif /* OPENAI_IMAGE_H */
