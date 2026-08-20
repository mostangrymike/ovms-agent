#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_IMAGE.H"

#define M259_IMAGE_MAX_BYTES (5UL * 1024UL * 1024UL)

static int m259_image_safe_path(const char *path)
{
    if (path == NULL || *path == '\0') return 0;
    if (strstr(path, "..") != NULL) return 0;
    if (strchr(path, ':') != NULL) return 0;
    if (*path == '/') return 0;
    if (strchr(path, '[') != NULL && strncmp(path, "[.", 2U) != 0) return 0;
    return 1;
}

static int m259_image_normalize(const char *input,
                                char *output,
                                size_t output_size)
{
    const char *position;
    size_t used;

    if (!m259_image_safe_path(input) || output == NULL || output_size == 0U)
        return 0;

    position = input;
    used = 0U;
    if (position[0] == '[' && position[1] == '.') {
        position += 2;
        while (*position != '\0' && *position != ']') {
            char ch = *position++;
            if (ch == '.') ch = '/';
            if (used + 1U >= output_size) return 0;
            output[used++] = ch;
        }
        if (*position != ']') return 0;
        ++position;
        if (*position != '\0' && used != 0U && output[used - 1U] != '/') {
            if (used + 1U >= output_size) return 0;
            output[used++] = '/';
        }
        while (*position != '\0') {
            if (used + 1U >= output_size) return 0;
            output[used++] = *position++;
        }
    } else {
        while (*position != '\0') {
            if (used + 1U >= output_size) return 0;
            output[used++] = *position++;
        }
    }
    if (used == 0U) return 0;
    output[used] = '\0';
    return 1;
}

static int m259_image_ext(const char *path,
                          char *extension,
                          size_t extension_size)
{
    const char *dot;
    size_t used;

    dot = strrchr(path, '.');
    if (dot == NULL || dot[1] == '\0') return 0;
    ++dot;
    used = 0U;
    while (*dot != '\0' && *dot != ';') {
        if (used + 1U >= extension_size) return 0;
        extension[used++] = (char)tolower((unsigned char)*dot++);
    }
    extension[used] = '\0';
    return used != 0U;
}

static int m259_image_signature(const unsigned char *head,
                                size_t count,
                                const char *extension,
                                const char **media_type)
{
    if (strcmp(extension, "png") == 0) {
        static const unsigned char png[8] = {
            0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU
        };
        if (count >= 8U && memcmp(head, png, 8U) == 0) {
            *media_type = "image/png";
            return 1;
        }
    } else if (strcmp(extension, "jpg") == 0 ||
               strcmp(extension, "jpeg") == 0) {
        if (count >= 3U && head[0] == 0xffU &&
            head[1] == 0xd8U && head[2] == 0xffU) {
            *media_type = "image/jpeg";
            return 1;
        }
    } else if (strcmp(extension, "gif") == 0) {
        if (count >= 6U &&
            (memcmp(head, "GIF87a", 6U) == 0 ||
             memcmp(head, "GIF89a", 6U) == 0)) {
            *media_type = "image/gif";
            return 1;
        }
    } else if (strcmp(extension, "webp") == 0) {
        if (count >= 12U && memcmp(head, "RIFF", 4U) == 0 &&
            memcmp(head + 8U, "WEBP", 4U) == 0) {
            *media_type = "image/webp";
            return 1;
        }
    }
    return 0;
}

static int m259_image_size(FILE *file, unsigned long *size)
{
    long end;

    if (file == NULL || size == NULL) return 0;
    if (fseek(file, 0L, SEEK_END) != 0) return 0;
    end = ftell(file);
    if (end <= 0L || (unsigned long)end > M259_IMAGE_MAX_BYTES) return 0;
    if (fseek(file, 0L, SEEK_SET) != 0) return 0;
    *size = (unsigned long)end;
    return 1;
}

int llm_image_info(const char *path, llm_image_meta *meta)
{
    FILE *file;
    unsigned char head[16];
    size_t count;
    char normalized[LLM_IMAGE_PATH_MAX];
    char extension[8];
    const char *media_type;
    unsigned long size;

    if (meta == NULL ||
        !m259_image_normalize(path, normalized, sizeof(normalized)) ||
        !m259_image_ext(normalized, extension, sizeof(extension))) return 0;

    file = fopen(normalized, "rb");
    if (file == NULL) return 0;
    if (!m259_image_size(file, &size)) {
        (void)fclose(file);
        return 0;
    }
    count = fread(head, 1U, sizeof(head), file);
    (void)fclose(file);
    media_type = NULL;
    if (!m259_image_signature(head, count, extension, &media_type)) return 0;

    (void)memset(meta, 0, sizeof(*meta));
    (void)strncpy(meta->path, normalized, sizeof(meta->path) - 1U);
    (void)strncpy(meta->media_type, media_type, sizeof(meta->media_type) - 1U);
    meta->size = size;
    return 1;
}

static int m259_b64_emit(FILE *output,
                         const unsigned char *input,
                         size_t count)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int value;
    char encoded[4];

    if (output == NULL || input == NULL || count == 0U || count > 3U) return 0;
    value = ((unsigned int)input[0]) << 16;
    if (count > 1U) value |= ((unsigned int)input[1]) << 8;
    if (count > 2U) value |= (unsigned int)input[2];
    encoded[0] = alphabet[(value >> 18) & 63U];
    encoded[1] = alphabet[(value >> 12) & 63U];
    encoded[2] = count > 1U ? alphabet[(value >> 6) & 63U] : '=';
    encoded[3] = count > 2U ? alphabet[value & 63U] : '=';
    return fwrite(encoded, 1U, 4U, output) == 4U;
}

int llm_image_write_data(FILE *output,
                         const char *path,
                         llm_image_meta *meta)
{
    FILE *file;
    llm_image_meta local;
    unsigned char buffer[768];
    unsigned char carry[3];
    size_t count;
    size_t index;
    size_t carry_count;

    if (output == NULL || !llm_image_info(path, &local)) return 0;
    file = fopen(local.path, "rb");
    if (file == NULL) return 0;

    if (fprintf(output, "data:%s;base64,", local.media_type) < 0) {
        (void)fclose(file);
        return 0;
    }

    carry_count = 0U;
    while ((count = fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
        index = 0U;
        if (carry_count != 0U) {
            while (carry_count < 3U && index < count)
                carry[carry_count++] = buffer[index++];
            if (carry_count == 3U) {
                if (!m259_b64_emit(output, carry, 3U)) {
                    (void)fclose(file);
                    return 0;
                }
                carry_count = 0U;
            }
        }
        while (index + 3U <= count) {
            if (!m259_b64_emit(output, buffer + index, 3U)) {
                (void)fclose(file);
                return 0;
            }
            index += 3U;
        }
        while (index < count) carry[carry_count++] = buffer[index++];
    }
    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }
    if (fclose(file) != 0) return 0;
    if (carry_count != 0U && !m259_b64_emit(output, carry, carry_count)) return 0;
    if (meta != NULL) *meta = local;
    return 1;
}
