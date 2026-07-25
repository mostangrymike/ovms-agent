#include "openai_internal.h"

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
