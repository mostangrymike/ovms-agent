#include <string.h>


int command_line_complete(
    const char *input,
    size_t input_size,
    int reached_eof)
{
    size_t length;

    if (input == NULL ||
        input_size == 0U) {
        return 0;
    }

    length = strlen(input);

    if (length > 0U &&
        input[length - 1U] == '\n') {
        return 1;
    }

    return reached_eof != 0;
}

int command_read_stream(
    FILE *stream,
    char *input,
    size_t input_size)
{
    size_t length;
    int character;

    if (stream == NULL ||
        input == NULL ||
        input_size < 2U) {
        return 0;
    }

    input[0] = '\0';

    if (fgets(input, input_size, stream) == NULL) {
        return 0;
    }

    length = strlen(input);

    if (command_line_complete(
            input,
            input_size,
            feof(stream))) {
        return 1;
    }

    while ((character = fgetc(stream)) != '\n' &&
           character != EOF) {
    }

    input[0] = '\0';
    return -1;
}
