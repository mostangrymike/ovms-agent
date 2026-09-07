static int m308_payload_escape(
    const char *line,
    size_t line_length,
    const char *end_marker)
{
    const char *content;
    size_t content_length;
    size_t marker_length;

    if (line == NULL ||
        line_length < 2U ||
        line[0] != '\\') {
        return 0;
    }

    content = line + 1;
    content_length = line_length - 1U;

    while (content_length > 0U &&
           content[0] == '\\') {
        ++content;
        --content_length;
    }

    if (end_marker != NULL) {
        marker_length = strlen(end_marker);
        if (content_length == marker_length &&
            memcmp(content, end_marker, marker_length) == 0) {
            return 1;
        }
    }

    if ((content_length == 15U &&
         memcmp(content, "BEGIN_OPERATION", 15U) == 0) ||
        (content_length == 13U &&
         memcmp(content, "END_OPERATION", 13U) == 0) ||
        (content_length == 14U &&
         (memcmp(content, "BEGIN_OLD_TEXT", 14U) == 0 ||
          memcmp(content, "BEGIN_NEW_TEXT", 14U) == 0)) ||
        (content_length == 12U &&
         (memcmp(content, "END_OLD_TEXT", 12U) == 0 ||
          memcmp(content, "END_NEW_TEXT", 12U) == 0)) ||
        (content_length >= 16U &&
         memcmp(content, "operation_count=", 16U) == 0) ||
        (content_length >= 3U &&
         content[0] == '`' &&
         content[1] == '`' &&
         content[2] == '`')) {
        return 1;
    }

    return 0;
}

static int execute_read_text_block(
    FILE *file,
    const char *end_marker,
    char *destination,
    size_t destination_size,
    int allow_empty)
{
    char block_line[LLM_EXECUTE_LINE_SIZE];
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

        if (m308_payload_escape(
                block_line,
                line_length,
                end_marker)) {
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
