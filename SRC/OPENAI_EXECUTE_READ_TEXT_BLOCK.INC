static int execute_read_text_block(
    FILE *file,
    const char *end_marker,
    char *destination,
    size_t destination_size,
    int allow_empty)
{
    char block_line[OPENAI_EXECUTE_LINE_SIZE];
    size_t used;
    size_t line_length;
    size_t marker_length;
    const char *content;

    if (file == NULL ||
        end_marker == NULL ||
        destination == NULL ||
        destination_size == 0U) {
        return 0;
    }

    used = 0U;
    destination[0] = '\0';
    marker_length = strlen(end_marker);

    while (fgets(block_line, sizeof(block_line), file) != NULL) {
        line_length = strlen(block_line);

        while (line_length > 0U &&
               (block_line[line_length - 1U] == '\n' ||
                block_line[line_length - 1U] == '\r')) {
            --line_length;
        }

        if (line_length == marker_length &&
            memcmp(block_line, end_marker, marker_length) == 0) {
            if (used > 0U && destination[used - 1U] == '\n') {
                --used;
            }

            destination[used] = '\0';
            return allow_empty || used > 0U;
        }

        content = block_line;

        if (line_length == marker_length + 1U &&
            block_line[0] == '\\' &&
            memcmp(block_line + 1, end_marker, marker_length) == 0) {
            content = block_line + 1;
            --line_length;
        }

        if (used + line_length + 1U >= destination_size) {
            return 0;
        }

        if (line_length > 0U) {
            (void)memcpy(destination + used, content, line_length);
            used += line_length;
        }

        destination[used++] = '\n';
        destination[used] = '\0';
    }

    return 0;
}
