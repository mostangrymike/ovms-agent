#include "openai_internal.h"
#include "openai_path.h"

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

void openai_cache_init(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        cache[index].path = NULL;
        cache[index].content = NULL;
    }
}

void openai_cache_free(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        free(cache[index].path);
        free(cache[index].content);
    }
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

int openai_cache_store(openai_file_cache_entry *cache,
                              const char *path,
                              const char *content)
{
    unsigned int index;
    char *path_copy;
    char *content_copy;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        if (cache[index].path == NULL) {
            break;
        }
    }

    if (index == OPENAI_AGENT_CACHE_SIZE) {
        return 0;
    }

    path_copy = malloc(strlen(path) + 1U);
    content_copy = malloc(strlen(content) + 1U);

    if (path_copy == NULL || content_copy == NULL) {
        free(path_copy);
        free(content_copy);
        return 0;
    }

    (void)strcpy(path_copy, path);
    (void)strcpy(content_copy, content);
    cache[index].path = path_copy;
    cache[index].content = content_copy;
    return 1;
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

char *execute_read_file_range_tool(
    const char *arguments,
    char **display_path,
    long *display_start,
    long *display_end)
{
    char *path;
    FILE *file;
    char line[2048];
    char *output;
    size_t used;
    unsigned long line_number;
    long start_line;
    long end_line;

    *display_path = NULL;
    *display_start = 0L;
    *display_end = 0L;

    path = extract_string_argument(arguments, "path");

    if (path == NULL ||
        !extract_integer_argument(
            arguments,
            "start_line",
            &start_line) ||
        !extract_integer_argument(
            arguments,
            "end_line",
            &end_line)) {
        free(path);
        return make_tool_error(
            "read_file_range requires path, start_line, and end_line",
            NULL
        );
    }

    *display_path = openai_duplicate_text(path);
    *display_start = start_line;
    *display_end = end_line;

    if (!openai_path_is_safe(path)) {
        char *error;

        error = make_tool_error(
            "Unsafe or invalid project-relative path",
            path
        );
        free(path);
        return error;
    }

    if (openai_path_is_sensitive(path)) {
        char *error;

        error = make_tool_error(
            "Access denied for sensitive path",
            path
        );
        free(path);
        return error;
    }

    if (start_line < 1L ||
        end_line < start_line ||
        end_line - start_line > 2000L) {
        free(path);
        return make_tool_error(
            "Invalid line range; maximum span is 2001 lines",
            NULL
        );
    }

    file = fopen(path, "r");

    if (file == NULL) {
        char *error;

        error = make_tool_error("Unable to read file", path);
        free(path);
        return error;
    }

    output = malloc(OPENAI_RANGE_OUTPUT_LIMIT);

    if (output == NULL) {
        (void)fclose(file);
        free(path);
        return make_tool_error(
            "Insufficient memory for ranged read",
            NULL
        );
    }

    output[0] = '\0';
    used = 0U;
    line_number = 1UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        int written;

        if ((long)line_number < start_line) {
            ++line_number;
            continue;
        }

        if ((long)line_number > end_line) {
            break;
        }

        written = snprintf(
            output + used,
            OPENAI_RANGE_OUTPUT_LIMIT - used,
            "%6lu  %s",
            line_number,
            line
        );

        if (written < 0 ||
            (size_t)written >=
                OPENAI_RANGE_OUTPUT_LIMIT - used) {
            (void)snprintf(
                output + used,
                OPENAI_RANGE_OUTPUT_LIMIT - used,
                "[ranged read truncated]\n"
            );
            break;
        }

        used += (size_t)written;

        if (used > 0U &&
            output[used - 1U] != '\n' &&
            used + 1U < OPENAI_RANGE_OUTPUT_LIMIT) {
            output[used++] = '\n';
            output[used] = '\0';
        }

        ++line_number;
    }

    (void)fclose(file);
    free(path);

    if (output[0] == '\0') {
        (void)strcpy(
            output,
            "[requested range contains no lines]"
        );
    }

    return output;
}

char *execute_read_file_tool(
    const char *arguments,
    openai_file_cache_entry *cache,
    int *cache_hit,
    char **display_path)
{
    char *path;
    char *output;
    const char *cached;

    *cache_hit = 0;
    *display_path = NULL;
    path = extract_path_argument(arguments);

    if (path == NULL) {
        return make_tool_error(
            "read_file arguments did not contain a valid path",
            NULL
        );
    }

    *display_path = openai_duplicate_text(path);

    if (!openai_path_is_safe(path)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            path
        );
        free(path);
        return output;
    }

    cached = openai_cache_lookup(cache, path);

    if (cached != NULL) {
        *cache_hit = 1;
        output = openai_duplicate_text(cached);
        free(path);
        return output;
    }

    output = openai_read_text_file(path);

    if (output == NULL) {
        output = make_tool_error("Unable to read file", path);
    } else {
        (void)openai_cache_store(cache, path, output);
    }

    free(path);
    return output;
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

char *execute_list_directory_tool(const char *arguments,
                                         char **display_path)
{
    char *path;
    const char *target;
    DIR *directory;
    struct dirent *entry;
    char *output;
    size_t used;
    unsigned long count;

    *display_path = NULL;
    path = extract_string_argument(arguments, "path");

    if (path == NULL) {
        return make_tool_error(
            "list_directory arguments did not contain a valid path",
            NULL
        );
    }

    target = *path == '\0' ? "." : path;
    *display_path = openai_duplicate_text(target);

    if (strcmp(target, ".") != 0 &&
        !openai_path_is_safe(target)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            target
        );
        free(path);
        return output;
    }

    directory = opendir(target);

    if (directory == NULL) {
        output = make_tool_error("Unable to open directory", target);
        free(path);
        return output;
    }

    output = malloc(OPENAI_LIST_OUTPUT_LIMIT);

    if (output == NULL) {
        (void)closedir(directory);
        free(path);
        return make_tool_error(
            "Insufficient memory for directory listing",
            target
        );
    }

    output[0] = '\0';
    used = 0U;
    count = 0UL;

    while ((entry = readdir(directory)) != NULL) {
        char child_path[1024];
        DIR *child_directory;
        int is_directory;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, ".git", 4U) == 0 ||
            openai_listing_entry_hidden(entry->d_name)) {
            continue;
        }

        if (!openai_join_path(target,
                              entry->d_name,
                              child_path,
                              sizeof(child_path))) {
            continue;
        }

        child_directory = opendir(child_path);
        is_directory = child_directory != NULL;

        if (child_directory != NULL) {
            (void)closedir(child_directory);
        }

        written = snprintf(output + used,
                           OPENAI_LIST_OUTPUT_LIMIT - used,
                           "%s%s\n",
                           entry->d_name,
                           is_directory ? "/" : "");

        if (written < 0 ||
            (size_t)written >= OPENAI_LIST_OUTPUT_LIMIT - used) {
            (void)snprintf(output + used,
                           OPENAI_LIST_OUTPUT_LIMIT - used,
                           "[directory listing truncated]\n");
            break;
        }

        used += (size_t)written;
        ++count;
    }

    (void)closedir(directory);
    free(path);

    if (count == 0UL && output[0] == '\0') {
        (void)strcpy(output, "[empty directory]");
    }

    return output;
}

char *execute_search_file_tool(const char *arguments,
                                      char **display_path,
                                      char **display_pattern)
{
    char *path;
    char *pattern;
    FILE *file;
    char line[2048];
    unsigned long line_number;
    unsigned long matches;
    size_t used;
    char *output;

    *display_path = NULL;
    *display_pattern = NULL;

    path = extract_string_argument(arguments, "path");
    pattern = extract_string_argument(arguments, "pattern");

    if (path == NULL || pattern == NULL || *pattern == '\0') {
        free(path);
        free(pattern);
        return make_tool_error(
            "search_file requires valid path and pattern arguments",
            NULL
        );
    }

    *display_path = openai_duplicate_text(path);
    *display_pattern = openai_duplicate_text(pattern);

    if (!openai_path_is_safe(path)) {
        output = make_tool_error(
            "Unsafe or invalid project-relative path",
            path
        );
        free(path);
        free(pattern);
        return output;
    }

    if (openai_path_is_sensitive(path)) {
        output = make_tool_error(
            "Access denied for sensitive path",
            path
        );
        free(path);
        free(pattern);
        return output;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        output = make_tool_error("Unable to search file", path);
        free(path);
        free(pattern);
        return output;
    }

    output = malloc(OPENAI_SEARCH_OUTPUT_LIMIT);

    if (output == NULL) {
        (void)fclose(file);
        free(path);
        free(pattern);
        return make_tool_error("Insufficient memory for search", path);
    }

    output[0] = '\0';
    used = 0U;
    line_number = 1UL;
    matches = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, pattern) != NULL) {
            int written;

            written = snprintf(output + used,
                               OPENAI_SEARCH_OUTPUT_LIMIT - used,
                               "%lu: %s",
                               line_number,
                               line);

            if (written < 0 ||
                (size_t)written >=
                    OPENAI_SEARCH_OUTPUT_LIMIT - used) {
                (void)snprintf(
                    output + used,
                    OPENAI_SEARCH_OUTPUT_LIMIT - used,
                    "\n[search output truncated]\n"
                );
                break;
            }

            used += (size_t)written;

            if (used > 0U &&
                output[used - 1U] != '\n' &&
                used + 1U < OPENAI_SEARCH_OUTPUT_LIMIT) {
                output[used++] = '\n';
                output[used] = '\0';
            }

            ++matches;
        }

        ++line_number;
    }

    (void)fclose(file);

    if (matches == 0UL) {
        (void)strcpy(output, "No matching lines.");
    }

    free(path);
    free(pattern);
    return output;
}

openai_create_result execute_create_file_tool(
    const char *arguments,
    char **display_path)
{
    char *path;
    char *content;
    FILE *existing;
    FILE *destination;
    char answer[32];
    size_t content_length;
    size_t written;

    *display_path = NULL;
    path = extract_string_argument(arguments, "path");
    content = extract_string_argument(arguments, "content");

    if (path == NULL || content == NULL) {
        free(path);
        free(content);
        (void)puts("create_file requires path and content.");
        return OPENAI_CREATE_ERROR;
    }

    *display_path = openai_duplicate_text(path);

    if (!openai_path_is_safe(path)) {
        (void)printf(
            "Unsafe or invalid project-relative path: %s\n",
            path
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    if (openai_path_is_sensitive(path)) {
        (void)printf(
            "Access denied for sensitive path: %s\n",
            path
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    content_length = strlen(content);

    if (content_length > OPENAI_CREATE_MAX_BYTES) {
        (void)printf(
            "Proposed file is too large (%lu bytes; limit %u).\n",
            (unsigned long)content_length,
            (unsigned int)OPENAI_CREATE_MAX_BYTES
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    existing = fopen(path, "r");

    if (existing != NULL) {
        (void)fclose(existing);
        (void)printf(
            "Refusing to create %s because it already exists.\n",
            path
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    (void)printf("Create new file: %s\n", path);
    (void)printf("Size: %lu bytes\n", (unsigned long)content_length);
    (void)puts("");
    (void)puts("Proposed contents:");
    (void)puts("------------------");
    (void)fputs(content, stdout);

    if (content_length == 0U ||
        content[content_length - 1U] != '\n') {
        (void)putchar('\n');
    }

    (void)puts("------------------");
    (void)printf("Create file [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        free(path);
        free(content);
        return OPENAI_CREATE_DECLINED;
    }

    if (answer[0] != 'y' && answer[0] != 'Y') {
        (void)puts("File creation cancelled.");
        free(path);
        free(content);
        return OPENAI_CREATE_DECLINED;
    }

    destination = fopen(path, "w");

    if (destination == NULL) {
        (void)printf(
            "Unable to create %s: %s\n",
            path,
            strerror(errno)
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    written = fwrite(content, 1U, content_length, destination);

    if (written != content_length ||
        fclose(destination) != 0) {
        (void)printf(
            "Unable to write complete contents to %s: %s\n",
            path,
            strerror(errno)
        );
        free(path);
        free(content);
        return OPENAI_CREATE_ERROR;
    }

    (void)puts("File created.");
    free(path);
    free(content);
    return OPENAI_CREATE_CREATED;
}

openai_replace_result execute_replace_lines_tool(
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    char *path;
    char *new_text;
    FILE *file;
    char *content;
    long file_length;
    size_t actual;
    long first_line;
    long last_line;
    long line_number;
    size_t start_offset;
    size_t end_offset;
    size_t position;
    char answer[32];
    int write_ok;

    (void)state;
    *display_path = NULL;
    path = extract_string_argument(arguments, "path");
    new_text = extract_string_argument(arguments, "new_text");

    if (path == NULL || new_text == NULL ||
        !extract_integer_argument(arguments, "first_line", &first_line) ||
        !extract_integer_argument(arguments, "last_line", &last_line)) {
        free(path);
        free(new_text);
        (void)puts(
            "replace_lines requires path, first_line, last_line, and new_text."
        );
        return OPENAI_REPLACE_ERROR;
    }

    *display_path = openai_duplicate_text(path);

    if (!openai_path_is_safe(path)) {
        (void)printf("Unsafe or invalid project-relative path: %s\n", path);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    if (openai_path_is_sensitive(path)) {
        (void)printf("Access denied for sensitive path: %s\n", path);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    if (first_line < 1L || last_line < first_line ||
        last_line - first_line > 2000L) {
        (void)puts("Invalid line range; maximum span is 2001 lines.");
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    if (strlen(new_text) > OPENAI_CREATE_MAX_BYTES) {
        (void)printf(
            "Replacement text is too large (%lu bytes; limit %u).\n",
            (unsigned long)strlen(new_text),
            (unsigned int)OPENAI_CREATE_MAX_BYTES
        );
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n", path, strerror(errno));
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    if (fseek(file, 0L, SEEK_END) != 0 ||
        (file_length = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)printf("Unable to size %s: %s\n", path, strerror(errno));
        (void)fclose(file);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    content = malloc((size_t)file_length + 1U);
    if (content == NULL) {
        (void)puts("Insufficient memory for line-range patch.");
        (void)fclose(file);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    actual = fread(content, 1U, (size_t)file_length, file);
    if (ferror(file)) {
        (void)printf("Unable to read %s: %s\n", path, strerror(errno));
        (void)fclose(file);
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }
    if (fclose(file) != 0) {
        (void)printf("Unable to close %s after reading: %s\n",
                     path, strerror(errno));
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }
    content[actual] = '\0';

    line_number = 1L;
    start_offset = 0U;
    for (position = 0U; position < actual && line_number < first_line; ++position) {
        if (content[position] == '\n') {
            ++line_number;
            start_offset = position + 1U;
        }
    }

    if (line_number != first_line || start_offset > actual) {
        (void)puts("Invalid line range: first_line does not exist.");
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    end_offset = start_offset;
    while (end_offset < actual && line_number <= last_line) {
        if (content[end_offset++] == '\n') {
            if (line_number == last_line) {
                break;
            }
            ++line_number;
        }
    }

    if (line_number < last_line ||
        (line_number == last_line && end_offset == start_offset)) {
        (void)puts("Invalid line range: last_line does not exist.");
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    (void)printf("File: %s\n\n", path);
    (void)puts("Current:");
    if (end_offset > start_offset) {
        (void)fwrite(content + start_offset, 1U,
                     end_offset - start_offset, stdout);
        if (content[end_offset - 1U] != '\n') {
            (void)putchar('\n');
        }
    }
    (void)puts("\nProposed:");
    (void)fputs(new_text, stdout);
    if (*new_text == '\0' || new_text[strlen(new_text) - 1U] != '\n') {
        (void)putchar('\n');
    }
    (void)printf("\nApply patch [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL ||
        (answer[0] != 'y' && answer[0] != 'Y')) {
        (void)puts("Patch cancelled.");
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_DECLINED;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        (void)printf("Unable to write %s: %s\n", path, strerror(errno));
        free(content);
        free(path);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    write_ok = 1;
    if (start_offset > 0U &&
        fwrite(content, 1U, start_offset, file) != start_offset) {
        write_ok = 0;
    }
    if (write_ok && *new_text != '\0' &&
        fwrite(new_text, 1U, strlen(new_text), file) != strlen(new_text)) {
        write_ok = 0;
    }
    if (write_ok && end_offset < actual &&
        fwrite(content + end_offset, 1U, actual - end_offset, file) !=
            actual - end_offset) {
        write_ok = 0;
    }
    if (fclose(file) != 0) {
        write_ok = 0;
    }

    free(content);
    free(path);
    free(new_text);

    if (!write_ok) {
        (void)puts("Unable to write complete line-range patch.");
        return OPENAI_REPLACE_ERROR;
    }

    (void)puts("Patch applied. A new OpenVMS file version was created.");
    return OPENAI_REPLACE_APPLIED;
}

openai_replace_result execute_replace_text_tool(
    agent_state *state,
    const char *arguments,
    char **display_path)
{
    char *path;
    char *old_text;
    char *new_text;
    int patched;

    *display_path = NULL;

    path = extract_string_argument(arguments, "path");
    old_text = extract_string_argument(arguments, "old_text");
    new_text = extract_string_argument(arguments, "new_text");

    if (path == NULL || old_text == NULL || new_text == NULL) {
        free(path);
        free(old_text);
        free(new_text);
        (void)puts(
            "replace_text requires path, old_text, and new_text."
        );
        return OPENAI_REPLACE_ERROR;
    }

    *display_path = openai_duplicate_text(path);

    if (!openai_path_is_safe(path)) {
        (void)printf(
            "Unsafe or invalid project-relative path: %s\n",
            path
        );
        free(path);
        free(old_text);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    if (openai_path_is_sensitive(path)) {
        (void)printf(
            "Access denied for sensitive path: %s\n",
            path
        );
        free(path);
        free(old_text);
        free(new_text);
        return OPENAI_REPLACE_ERROR;
    }

    patched = project_patch(state, path, old_text, new_text);

    free(path);
    free(old_text);
    free(new_text);

    if (patched) {
        return OPENAI_REPLACE_APPLIED;
    }

    return OPENAI_REPLACE_DECLINED;
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

int openai_confirm_restore(const char *path)
{
    char answer[32];

    (void)printf(
        "Build failed after patching %s.\n"
        "Restore the previous OpenVMS file version [y/N]? ",
        path
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        (void)putchar('\n');
        return 0;
    }

    return answer[0] == 'y' || answer[0] == 'Y';
}

int openai_restore_previous_version(const char *path)
{
    char previous_path[1024];
    char destination_path[1024];
    unsigned char buffer[8192];
    FILE *source;
    FILE *destination;
    size_t count;
    int written;

    if (path == NULL || *path == '\0') {
        (void)puts("Unable to restore an empty path.");
        return 0;
    }

    written = snprintf(
        previous_path,
        sizeof(previous_path),
        "%s;-1",
        path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(previous_path)) {
        (void)puts("Previous-version path is too long.");
        return 0;
    }

    /*
     * A trailing semicolon requests a new highest OpenVMS file version.
     * Without it, RMS may try to reuse the source version number.
     */
    written = snprintf(
        destination_path,
        sizeof(destination_path),
        "%s;",
        path
    );

    if (written < 0 ||
        (size_t)written >= sizeof(destination_path)) {
        (void)puts("Restore destination path is too long.");
        return 0;
    }

    source = fopen(previous_path, "rb");

    if (source == NULL) {
        (void)printf(
            "Unable to open previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        return 0;
    }

    destination = fopen(destination_path, "wb");

    if (destination == NULL) {
        (void)printf(
            "Unable to create restored version %s: %s\n",
            destination_path,
            strerror(errno)
        );
        (void)fclose(source);
        return 0;
    }

    while ((count = fread(
                buffer,
                1U,
                sizeof(buffer),
                source)) > 0U) {
        if (fwrite(buffer, 1U, count, destination) != count) {
            (void)printf(
                "Unable to write restored version %s: %s\n",
                destination_path,
                strerror(errno)
            );
            (void)fclose(source);
            (void)fclose(destination);
            return 0;
        }
    }

    if (ferror(source)) {
        (void)printf(
            "Unable to read previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        (void)fclose(source);
        (void)fclose(destination);
        return 0;
    }

    if (fclose(source) != 0) {
        (void)printf(
            "Unable to close previous version %s: %s\n",
            previous_path,
            strerror(errno)
        );
        (void)fclose(destination);
        return 0;
    }

    if (fclose(destination) != 0) {
        (void)printf(
            "Unable to close restored version %s: %s\n",
            destination_path,
            strerror(errno)
        );
        return 0;
    }

    (void)puts(
        "Previous file contents restored as a new OpenVMS version."
    );
    return 1;
}

