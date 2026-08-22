static int execute_write_text_block(
    FILE *file,
    const char *begin_marker,
    const char *end_marker,
    const char *text)
{
    const char *line_start;
    const char *line_end;
    size_t line_length;

    if (file == NULL ||
        begin_marker == NULL ||
        end_marker == NULL ||
        text == NULL) {
        return 0;
    }

    if (fprintf(file, "%s\n", begin_marker) < 0) {
        return 0;
    }

    line_start = text;

    if (*line_start != '\0') {
        for (;;) {
            line_end = strchr(line_start, '\n');

            if (line_end != NULL) {
                line_length = (size_t)(line_end - line_start);
            } else {
                line_length = strlen(line_start);
            }

            if (line_length == strlen(end_marker) &&
                memcmp(line_start, end_marker, line_length) == 0) {
                if (fputc('\\', file) == EOF) {
                    return 0;
                }
            }

            if (line_length > 0U &&
                fwrite(line_start, 1U, line_length, file) != line_length) {
                return 0;
            }

            if (fputc('\n', file) == EOF) {
                return 0;
            }

            if (line_end == NULL) {
                break;
            }

            line_start = line_end + 1;

            if (*line_start == '\0') {
                if (fputc('\n', file) == EOF) {
                    return 0;
                }

                break;
            }
        }
    }

    if (fprintf(file, "%s\n", end_marker) < 0) {
        return 0;
    }

    return 1;
}
