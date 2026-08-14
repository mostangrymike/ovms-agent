/*
 * M251.5b true multiline prompt input.
 *
 * This is intentionally separate from command_input_m149.inc.  M149's
 * trailing-backslash command continuation semantics remain unchanged.
 *
 * A multiline body ends only when a physical record contains ".END" by
 * itself.  Embedded newlines in all preceding records are preserved.
 *
 * Return values:
 *   1  complete multiline body read
 *   0  invalid arguments
 *  -1  body exceeded the supplied buffer; input cleared and body drained
 *  -2  EOF before .END; input cleared
 */

static int command_m251_is_end(const char *line)
{
    size_t length;

    if (line == NULL) {
        return 0;
    }

    length = strlen(line);

    while (length > 0U &&
           (line[length - 1U] == '\n' ||
            line[length - 1U] == '\r')) {
        --length;
    }

    return length == 4U &&
           strncmp(line, ".END", 4U) == 0;
}

static void command_m251_drain_record(FILE *stream)
{
    int character;

    if (stream == NULL) {
        return;
    }

    while ((character = fgetc(stream)) != '\n' &&
           character != EOF) {
    }
}

static void command_m251_drain_body(FILE *stream)
{
    char line[OVMS_AGENT_INPUT_SIZE];
    size_t length;

    if (stream == NULL) {
        return;
    }

    for (;;) {
        if (fgets(line, sizeof(line), stream) == NULL) {
            return;
        }

        length = strlen(line);

        if (length > 0U &&
            line[length - 1U] != '\n' &&
            !feof(stream)) {
            command_m251_drain_record(stream);
            continue;
        }

        if (command_m251_is_end(line)) {
            return;
        }
    }
}

static int command_read_multiline(
    FILE *stream,
    char *input,
    size_t input_size)
{
    char line[OVMS_AGENT_INPUT_SIZE];
    size_t used;
    size_t length;

    if (stream == NULL ||
        input == NULL ||
        input_size < 2U) {
        return 0;
    }

    input[0] = '\0';
    used = 0U;

    for (;;) {
        if (fgets(line, sizeof(line), stream) == NULL) {
            input[0] = '\0';
            return -2;
        }

        length = strlen(line);

        if (length > 0U &&
            line[length - 1U] != '\n' &&
            !feof(stream)) {
            command_m251_drain_record(stream);
            command_m251_drain_body(stream);
            input[0] = '\0';
            return -1;
        }

        if (command_m251_is_end(line)) {
            return 1;
        }

        if (used + length + 1U > input_size) {
            command_m251_drain_body(stream);
            input[0] = '\0';
            return -1;
        }

        if (length > 0U) {
            memcpy(input + used, line, length);
            used += length;
        }

        input[used] = '\0';
    }
}
