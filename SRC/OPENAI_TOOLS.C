#include "openai_internal.h"
#include "openai_path.h"
#include "openai_cache.h"

char *openai_read_text_file(const char *path)
{
    FILE *file;
    long length;
    size_t actual;
    char *data;

    file = fopen(path, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n",
                     path,
                     strerror(errno));
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)printf("Unable to size %s: %s\n",
                     path,
                     strerror(errno));
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);

    if (length < 0L) {
        (void)printf("Unable to size %s.\n", path);
        (void)fclose(file);
        return NULL;
    }

    if (length > 65536L) {
        (void)printf(
            "File is too large for REVIEW (%ld bytes; limit 65536).\n",
            length
        );
        (void)fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)printf("Unable to rewind %s: %s\n",
                     path,
                     strerror(errno));
        (void)fclose(file);
        return NULL;
    }

    data = malloc((size_t)length + 1U);

    if (data == NULL) {
        (void)puts("Insufficient memory for file review.");
        (void)fclose(file);
        return NULL;
    }

    actual = fread(data, 1U, (size_t)length, file);

    if (ferror(file)) {
        (void)printf("Unable to read %s: %s\n",
                     path,
                     strerror(errno));
        free(data);
        (void)fclose(file);
        return NULL;
    }

    data[actual] = '\0';
    (void)fclose(file);
    return data;
}

char *make_tool_error(const char *message,
                             const char *path)
{
    size_t size;
    char *result;

    size = strlen(message) + 1U;

    if (path != NULL) {
        size += strlen(path) + 2U;
    }

    result = malloc(size);

    if (result == NULL) {
        return NULL;
    }

    (void)strcpy(result, message);

    if (path != NULL) {
        (void)strcat(result, ": ");
        (void)strcat(result, path);
    }

    return result;
}

const char *openai_cache_lookup(
    const openai_file_cache_entry *cache,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        if (cache[index].path != NULL &&
            strcmp(cache[index].path, path) == 0) {
            return cache[index].content;
        }
    }

    return NULL;
}

char *openai_duplicate_text(const char *text)
{
    char *copy;

    copy = malloc(strlen(text) + 1U);

    if (copy != NULL) {
        (void)strcpy(copy, text);
    }

    return copy;
}

int openai_join_path(const char *parent,
                            const char *child,
                            char *output,
                            size_t output_size)
{
    int written;

    if (parent == NULL || child == NULL ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (*parent == '\0' || strcmp(parent, ".") == 0) {
        written = snprintf(output, output_size, "%s", child);
    } else {
        written = snprintf(output, output_size, "%s/%s", parent, child);
    }

    return written >= 0 && (size_t)written < output_size;
}

char *execute_run_build_tool(int *build_status)
{
    char command[256];
    char *output;
    size_t length;
    int status;
    int written;

    if (build_status == NULL) {
        return openai_duplicate_text("Build status pointer was NULL.");
    }

    /* Fixed command only; no user or model text is interpolated. */
    written = snprintf(command,
                       sizeof(command),
                       "@BUILD.COM/OUTPUT=%s",
                       OPENAI_BUILD_LOG_FILE);

    if (written < 0 || (size_t)written >= sizeof(command)) {
        return openai_duplicate_text(
            "Unable to construct fixed BUILD.COM invocation."
        );
    }

    (void)remove(OPENAI_BUILD_LOG_FILE);
    status = system(command);
    *build_status = status;
    openai_last_build_known = 1;
    openai_last_build_status = status;

    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        (status & 1) != 0 ? "build_success" : "build_failure",
        status
    );

    output = read_entire_file(OPENAI_BUILD_LOG_FILE, &length);

    if (output == NULL) {
        char fallback[256];

        (void)snprintf(
            fallback,
            sizeof(fallback),
            "BUILD.COM returned OpenVMS status %d, but the build log "
            "could not be read.",
            status
        );
        return openai_duplicate_text(fallback);
    }

    if (length > OPENAI_BUILD_OUTPUT_LIMIT) {
        output[OPENAI_BUILD_OUTPUT_LIMIT] = '\0';
    }

    {
        char header[256];
        size_t result_size;
        char *result;

        (void)snprintf(
            header,
            sizeof(header),
            "OpenVMS build status: %d (%s)\n\n",
            status,
            (status & 1) != 0 ? "success" : "failure"
        );

        result_size = strlen(header) + strlen(output) + 1U;
        result = malloc(result_size);

        if (result == NULL) {
            free(output);
            return openai_duplicate_text(
                "Insufficient memory to return build output."
            );
        }

        (void)strcpy(result, header);
        (void)strcat(result, output);
        free(output);
        return result;
    }
}

