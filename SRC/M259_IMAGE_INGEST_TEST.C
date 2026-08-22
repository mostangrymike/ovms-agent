#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_IMAGE.C"

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
}

static int write_bytes(const char *path,
                       const unsigned char *data,
                       size_t size)
{
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fwrite(data, 1U, size, file) != size) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int read_text(const char *path, char *output, size_t output_size)
{
    FILE *file;
    size_t used;
    int ch;

    file = fopen(path, "r");
    if (file == NULL) return 0;
    used = 0U;
    while ((ch = fgetc(file)) != EOF) {
        if (used + 1U >= output_size) {
            (void)fclose(file);
            return 0;
        }
        output[used++] = (char)ch;
    }
    output[used] = '\0';
    return fclose(file) == 0;
}

static int make_oversize(const char *path)
{
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fseek(file, (long)M259_IMAGE_MAX_BYTES, SEEK_SET) != 0 ||
        fputc(0, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int main(void)
{
    static const unsigned char png[] = {
        0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU
    };
    static const unsigned char jpeg[] = {
        0xffU, 0xd8U, 0xffU, 0xe0U
    };
    static const unsigned char gif[] = {
        'G','I','F','8','9','a'
    };
    static const unsigned char webp[] = {
        'R','I','F','F',0U,0U,0U,0U,'W','E','B','P'
    };
    llm_image_meta meta;
    FILE *output;
    char encoded[256];

    remove_all("M259_TINY.PNG");
    remove_all("M259_TINY.JPG");
    remove_all("M259_TINY.GIF");
    remove_all("M259_TINY.WEBP");
    remove_all("M259_BAD.PNG");
    remove_all("M259_BIG.PNG");
    remove_all("M259_DATA.TXT");

    if (!write_bytes("M259_TINY.PNG", png, sizeof(png)) ||
        !write_bytes("M259_TINY.JPG", jpeg, sizeof(jpeg)) ||
        !write_bytes("M259_TINY.GIF", gif, sizeof(gif)) ||
        !write_bytes("M259_TINY.WEBP", webp, sizeof(webp)) ||
        !write_bytes("M259_BAD.PNG", gif, sizeof(gif))) {
        (void)puts("M259 image ingest failed: fixture setup.");
        return 2;
    }

    if (!llm_image_info("M259_TINY.PNG", &meta) ||
        strcmp(meta.media_type, "image/png") != 0 || meta.size != sizeof(png)) {
        (void)puts("M259 image ingest failed: PNG validation.");
        return 2;
    }
    if (!llm_image_info("M259_TINY.JPG", &meta) ||
        strcmp(meta.media_type, "image/jpeg") != 0 ||
        !llm_image_info("M259_TINY.GIF", &meta) ||
        strcmp(meta.media_type, "image/gif") != 0 ||
        !llm_image_info("M259_TINY.WEBP", &meta) ||
        strcmp(meta.media_type, "image/webp") != 0) {
        (void)puts("M259 image ingest failed: supported formats.");
        return 2;
    }

    if (llm_image_info("M259_BAD.PNG", &meta) ||
        llm_image_info("../M259_TINY.PNG", &meta) ||
        llm_image_info("SYS$SYSDEVICE:M259_TINY.PNG", &meta) ||
        llm_image_info("/M259_TINY.PNG", &meta)) {
        (void)puts("M259 image ingest failed: safety/signature refusal.");
        return 2;
    }

    output = fopen("M259_DATA.TXT", "w");
    if (output == NULL ||
        !llm_image_write_data(output, "M259_TINY.PNG", &meta) ||
        fclose(output) != 0 ||
        !read_text("M259_DATA.TXT", encoded, sizeof(encoded)) ||
        strcmp(encoded, "data:image/png;base64,iVBORw0KGgo=") != 0) {
        (void)puts("M259 image ingest failed: base64 data URL.");
        return 2;
    }

    if (!make_oversize("M259_BIG.PNG") ||
        llm_image_info("M259_BIG.PNG", &meta)) {
        (void)puts("M259 image ingest failed: size limit.");
        return 2;
    }

    remove_all("M259_TINY.PNG");
    remove_all("M259_TINY.JPG");
    remove_all("M259_TINY.GIF");
    remove_all("M259_TINY.WEBP");
    remove_all("M259_BAD.PNG");
    remove_all("M259_BIG.PNG");
    remove_all("M259_DATA.TXT");
    (void)puts("M259 guarded image ingestion regression passed.");
    return 1;
}
