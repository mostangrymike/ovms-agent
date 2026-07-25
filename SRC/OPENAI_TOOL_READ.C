#include "openai_internal.h"

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
