#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit.h"

#define EDIT_LINE_SIZE 1024
#define EDIT_MAX_LINES 8192

typedef struct edit_buffer {
    char **lines;
    size_t count;
    size_t capacity;
    int modified;
} edit_buffer;

static void edit_buffer_init(edit_buffer *buffer)
{
    buffer->lines = NULL;
    buffer->count = 0U;
    buffer->capacity = 0U;
    buffer->modified = 0;
}

static void edit_buffer_free(edit_buffer *buffer)
{
    size_t index;

    if (buffer == NULL) {
        return;
    }

    for (index = 0U; index < buffer->count; ++index) {
        free(buffer->lines[index]);
    }

    free(buffer->lines);
    edit_buffer_init(buffer);
}

static int edit_buffer_reserve(edit_buffer *buffer, size_t needed)
{
    char **new_lines;
    size_t new_capacity;

    if (needed <= buffer->capacity) {
        return 1;
    }

    new_capacity = buffer->capacity == 0U ? 64U : buffer->capacity * 2U;

    while (new_capacity < needed) {
        new_capacity *= 2U;
    }

    if (new_capacity > EDIT_MAX_LINES) {
        new_capacity = EDIT_MAX_LINES;
    }

    if (new_capacity < needed) {
        return 0;
    }

    new_lines = realloc(buffer->lines,
                        new_capacity * sizeof(*new_lines));

    if (new_lines == NULL) {
        return 0;
    }

    buffer->lines = new_lines;
    buffer->capacity = new_capacity;
    return 1;
}

static int edit_buffer_append(edit_buffer *buffer, const char *text)
{
    size_t length;
    char *copy;

    if (buffer->count >= EDIT_MAX_LINES) {
        return 0;
    }

    if (!edit_buffer_reserve(buffer, buffer->count + 1U)) {
        return 0;
    }

    length = strlen(text);
    copy = malloc(length + 1U);

    if (copy == NULL) {
        return 0;
    }

    (void)memcpy(copy, text, length + 1U);
    buffer->lines[buffer->count++] = copy;
    return 1;
}

static int edit_load(const char *path, edit_buffer *buffer)
{
    FILE *file;
    char line[EDIT_LINE_SIZE];

    file = fopen(path, "r");

    if (file == NULL) {
        if (errno == ENOENT) {
            return 1;
        }

        (void)printf("Unable to open %s: %s\n",
                     path,
                     strerror(errno));
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (!edit_buffer_append(buffer, line)) {
            (void)puts("Editor buffer limit reached.");
            (void)fclose(file);
            return 0;
        }
    }

    if (ferror(file)) {
        (void)printf("Unable to read %s: %s\n",
                     path,
                     strerror(errno));
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        (void)printf("Unable to close %s: %s\n",
                     path,
                     strerror(errno));
        return 0;
    }

    return 1;
}

static void edit_print(const edit_buffer *buffer,
                       size_t first,
                       size_t last)
{
    size_t index;

    if (buffer->count == 0U) {
        (void)puts("[empty]");
        return;
    }

    if (first < 1U) {
        first = 1U;
    }

    if (last == 0U || last > buffer->count) {
        last = buffer->count;
    }

    if (first > last) {
        (void)puts("Invalid line range.");
        return;
    }

    for (index = first - 1U; index < last; ++index) {
        (void)printf("%6lu  %s",
                     (unsigned long)(index + 1U),
                     buffer->lines[index]);

        if (strchr(buffer->lines[index], '\n') == NULL) {
            (void)putchar('\n');
        }
    }
}

static int edit_write(const char *path, edit_buffer *buffer)
{
    FILE *file;
    size_t index;

    file = fopen(path, "w");

    if (file == NULL) {
        (void)printf("Unable to write %s: %s\n",
                     path,
                     strerror(errno));
        return 0;
    }

    for (index = 0U; index < buffer->count; ++index) {
        if (fputs(buffer->lines[index], file) == EOF) {
            (void)printf("Write failed for %s: %s\n",
                         path,
                         strerror(errno));
            (void)fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        (void)printf("Unable to close %s: %s\n",
                     path,
                     strerror(errno));
        return 0;
    }

    buffer->modified = 0;
    (void)printf("%lu line%s written to %s.\n",
                 (unsigned long)buffer->count,
                 buffer->count == 1U ? "" : "s",
                 path);
    return 1;
}

static void edit_help(void)
{
    (void)puts("Editor commands:");
    (void)puts("  p [first [last]]  Print lines");
    (void)puts("  a                 Append lines; end with a single .");
    (void)puts("  w                 Write a new OpenVMS file version");
    (void)puts("  q                 Quit; refuses if modified");
    (void)puts("  q!                Quit and discard changes");
    (void)puts("  h                 Display this help");
}

static int edit_append_mode(edit_buffer *buffer)
{
    char line[EDIT_LINE_SIZE];

    (void)puts("Enter text. A single period ends append mode.");

    for (;;) {
        (void)fputs("APPEND> ", stdout);
        (void)fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 0;
        }

        if (strcmp(line, ".\n") == 0 ||
            strcmp(line, ".\r\n") == 0 ||
            strcmp(line, ".") == 0) {
            return 1;
        }

        if (!edit_buffer_append(buffer, line)) {
            (void)puts("Unable to append line.");
            return 0;
        }

        buffer->modified = 1;
    }
}

void edit_run(agent_state *state, const char *path)
{
    edit_buffer buffer;
    char command[EDIT_LINE_SIZE];

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (path == NULL || *path == '\0') {
        (void)puts("Usage: EDIT project-relative-file");
        return;
    }

    if (strstr(path, "..") != NULL ||
        strchr(path, ':') != NULL ||
        *path == '/') {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    edit_buffer_init(&buffer);

    if (!edit_load(path, &buffer)) {
        edit_buffer_free(&buffer);
        return;
    }

    (void)printf("Editing %s (%lu line%s).\n",
                 path,
                 (unsigned long)buffer.count,
                 buffer.count == 1U ? "" : "s");
    edit_help();

    for (;;) {
        char op;
        unsigned long first;
        unsigned long last;
        int fields;

        (void)fputs("EDIT> ", stdout);
        (void)fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            (void)putchar('\n');
            break;
        }

        op = command[0];

        if (op == 'h' || op == 'H' || op == '?') {
            edit_help();
            continue;
        }

        if (op == 'p' || op == 'P') {
            first = 1UL;
            last = 0UL;
            fields = sscanf(command + 1, "%lu %lu", &first, &last);

            if (fields == EOF || fields == 0) {
                first = 1UL;
                last = 0UL;
            } else if (fields == 1) {
                last = first;
            }

            edit_print(&buffer, (size_t)first, (size_t)last);
            continue;
        }

        if (op == 'a' || op == 'A') {
            (void)edit_append_mode(&buffer);
            continue;
        }

        if (op == 'w' || op == 'W') {
            (void)edit_write(path, &buffer);
            continue;
        }

        if ((op == 'q' || op == 'Q') && command[1] == '!') {
            break;
        }

        if (op == 'q' || op == 'Q') {
            if (buffer.modified) {
                (void)puts("Buffer modified; use W or Q!.");
                continue;
            }

            break;
        }

        (void)puts("Unknown editor command. Enter H for help.");
    }

    edit_buffer_free(&buffer);
}

