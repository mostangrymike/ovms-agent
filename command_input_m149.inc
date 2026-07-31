#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "agent.h"

/*
 * Return the number of consecutive backslashes at the end of text[0..length).
 */
static size_t command_m149_trailing_backslashes(
    const char *text,
    size_t length)
{
    size_t count;

    count = 0U;

    while (length > count &&
           text[length - count - 1U] == '\\') {
        ++count;
    }

    return count;
}

/*
 * Analyze one complete physical record already held in memory.
 *
 * A trailing odd number of backslashes means continuation.  One backslash is
 * removed as the continuation marker; any remaining paired backslashes stay
 * literal.  A trailing even number means the record is complete and all
 * backslashes remain literal.
 */
static int command_m149_analyze_record(
    const char *line,
    size_t length,
    size_t *segment_length)
{
    size_t end;
    size_t slash_count;

    if (line == NULL ||
        segment_length == NULL) {
        return 0;
    }

    end = length;

    if (end > 0U &&
        line[end - 1U] == '\n') {
        --end;
    }

    slash_count =
        command_m149_trailing_backslashes(line, end);

    if ((slash_count % 2U) != 0U) {
        *segment_length = end - 1U;
        return 1;
    }

    *segment_length = end;
    return 0;
}

/*
 * Drain the unread remainder of an overlong physical record.
 *
 * initial_slash_count is the consecutive trailing-backslash count from the
 * portion already returned by fgets().  The return value is nonzero when the
 * complete physical record ends with an odd backslash count and therefore
 * continues onto another physical record.
 */
static int command_m149_drain_record_remainder(
    FILE *stream,
    size_t initial_slash_count)
{
    size_t slash_count;
    int character;

    if (stream == NULL) {
        return 0;
    }

    slash_count = initial_slash_count;

    while ((character = fgetc(stream)) != '\n' &&
           character != EOF) {
        if (character == '\\') {
            ++slash_count;
        } else {
            slash_count = 0U;
        }
    }

    return (slash_count % 2U) != 0U;
}

/*
 * Drain continuation records belonging to a rejected logical command.
 * Stop immediately after the first complete non-continuation record, leaving
 * the following independent command available for the next read.
 */
static void command_m149_drain_logical_tail(FILE *stream)
{
    char line[OVMS_AGENT_INPUT_SIZE];
    size_t length;
    size_t segment_length;
    size_t slash_count;
    int continued;

    if (stream == NULL) {
        return;
    }

    for (;;) {
        if (fgets(line, sizeof(line), stream) == NULL) {
            return;
        }

        length = strlen(line);

        if (length == 0U) {
            return;
        }

        if (line[length - 1U] != '\n' &&
            !feof(stream)) {
            slash_count =
                command_m149_trailing_backslashes(
                    line,
                    length);

            continued =
                command_m149_drain_record_remainder(
                    stream,
                    slash_count);
        } else {
            continued =
                command_m149_analyze_record(
                    line,
                    length,
                    &segment_length);
        }

        if (!continued) {
            return;
        }
    }
}

int command_line_complete(
    const char *input,
    size_t input_size,
    int reached_eof)
{
    size_t length;
    size_t segment_length;

    if (input == NULL ||
        input_size == 0U) {
        return 0;
    }

    length = strlen(input);

    if (length == 0U) {
        return 0;
    }

    if (command_m149_analyze_record(
            input,
            length,
            &segment_length)) {
        return 0;
    }

    if (input[length - 1U] == '\n') {
        return 1;
    }

    return reached_eof != 0;
}

int command_read_stream(
    FILE *stream,
    char *input,
    size_t input_size)
{
    char line[OVMS_AGENT_INPUT_SIZE];
    size_t assembled_length;
    size_t line_length;
    size_t segment_length;
    size_t separator_length;
    size_t slash_count;
    int continued;
    int pending_continuation;

    if (stream == NULL ||
        input == NULL ||
        input_size < 2U) {
        return 0;
    }

    input[0] = '\0';
    assembled_length = 0U;
    pending_continuation = 0;

    for (;;) {
        if (fgets(line, sizeof(line), stream) == NULL) {
            input[0] = '\0';

            if (pending_continuation) {
                return -1;
            }

            return 0;
        }

        line_length = strlen(line);

        if (line_length == 0U) {
            input[0] = '\0';

            if (pending_continuation) {
                return -1;
            }

            return 0;
        }

        /*
         * No newline and not EOF means fgets() returned only a prefix of an
         * overlong physical record.  Reject the whole logical command.
         */
        if (line[line_length - 1U] != '\n' &&
            !feof(stream)) {
            slash_count =
                command_m149_trailing_backslashes(
                    line,
                    line_length);

            continued =
                command_m149_drain_record_remainder(
                    stream,
                    slash_count);

            if (continued) {
                command_m149_drain_logical_tail(stream);
            }

            input[0] = '\0';
            return -1;
        }

        continued =
            command_m149_analyze_record(
                line,
                line_length,
                &segment_length);

        separator_length = 0U;

        if (assembled_length > 0U &&
            segment_length > 0U &&
            !isspace(
                (unsigned char)
                input[assembled_length - 1U]) &&
            !isspace((unsigned char)line[0])) {
            separator_length = 1U;
        }

        if (assembled_length +
                separator_length +
                segment_length +
                1U >
            input_size) {
            if (continued) {
                command_m149_drain_logical_tail(stream);
            }

            input[0] = '\0';
            return -1;
        }

        if (separator_length != 0U) {
            input[assembled_length] = ' ';
            ++assembled_length;
        }

        if (segment_length > 0U) {
            memcpy(
                input + assembled_length,
                line,
                segment_length);
            assembled_length += segment_length;
        }

        input[assembled_length] = '\0';

        if (!continued) {
            return 1;
        }

        pending_continuation = 1;
    }
}
