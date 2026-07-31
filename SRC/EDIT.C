#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit.h"

#define EDIT_LINE_SIZE 1024
#define EDIT_MAX_LINES 8192
#define EDIT_HISTORY_DEPTH 16

typedef struct edit_buffer {
    char **lines;
    size_t count;
    size_t capacity;
    int modified;
} edit_buffer;

typedef struct edit_clipboard {
    char **lines;
    size_t count;
} edit_clipboard;

typedef struct edit_history {
    edit_buffer items[EDIT_HISTORY_DEPTH];
    size_t count;
} edit_history;

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

static void edit_clipboard_init(edit_clipboard *clipboard)
{
    clipboard->lines = NULL;
    clipboard->count = 0U;
}

static void edit_clipboard_free(edit_clipboard *clipboard)
{
    size_t index;

    if (clipboard == NULL) {
        return;
    }

    for (index = 0U; index < clipboard->count; ++index) {
        free(clipboard->lines[index]);
    }

    free(clipboard->lines);
    edit_clipboard_init(clipboard);
}

static void edit_history_init(edit_history *history)
{
    size_t index;

    history->count = 0U;

    for (index = 0U; index < EDIT_HISTORY_DEPTH; ++index) {
        edit_buffer_init(&history->items[index]);
    }
}

static void edit_history_clear(edit_history *history)
{
    size_t index;

    for (index = 0U; index < history->count; ++index) {
        edit_buffer_free(&history->items[index]);
    }

    history->count = 0U;
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

static char *edit_copy_line(const char *text)
{
    size_t length;
    char *copy;

    length = strlen(text);
    copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    (void)memcpy(copy, text, length + 1U);
    return copy;
}

static int edit_buffer_clone(const edit_buffer *source,
                             edit_buffer *destination)
{
    size_t index;

    edit_buffer_init(destination);

    if (!edit_buffer_reserve(destination, source->count)) {
        return 0;
    }

    for (index = 0U; index < source->count; ++index) {
        char *copy;

        copy = edit_copy_line(source->lines[index]);

        if (copy == NULL) {
            edit_buffer_free(destination);
            return 0;
        }

        destination->lines[destination->count++] = copy;
    }

    destination->modified = source->modified;
    return 1;
}

static int edit_history_push(edit_history *history,
                             const edit_buffer *buffer)
{
    edit_buffer snapshot;
    size_t index;

    if (!edit_buffer_clone(buffer, &snapshot)) {
        (void)puts("Unable to save history state.");
        return 0;
    }

    if (history->count == EDIT_HISTORY_DEPTH) {
        edit_buffer_free(&history->items[0]);

        for (index = 1U; index < history->count; ++index) {
            history->items[index - 1U] = history->items[index];
        }

        edit_buffer_init(&history->items[history->count - 1U]);
        --history->count;
    }

    history->items[history->count++] = snapshot;
    return 1;
}

static int edit_history_pop(edit_history *history,
                            edit_buffer *buffer)
{
    edit_buffer replacement;

    if (history->count == 0U) {
        return 0;
    }

    replacement = history->items[history->count - 1U];
    edit_buffer_init(&history->items[history->count - 1U]);
    --history->count;

    edit_buffer_free(buffer);
    *buffer = replacement;
    return 1;
}

static int edit_buffer_append(edit_buffer *buffer, const char *text)
{
    char *copy;

    if (buffer->count >= EDIT_MAX_LINES) {
        return 0;
    }

    if (!edit_buffer_reserve(buffer, buffer->count + 1U)) {
        return 0;
    }

    copy = edit_copy_line(text);

    if (copy == NULL) {
        return 0;
    }

    buffer->lines[buffer->count++] = copy;
    return 1;
}

static int edit_buffer_insert(edit_buffer *buffer,
                              size_t before_line,
                              const char *text)
{
    char *copy;
    size_t index;

    if (before_line < 1U || before_line > buffer->count + 1U) {
        return 0;
    }

    if (buffer->count >= EDIT_MAX_LINES) {
        return 0;
    }

    if (!edit_buffer_reserve(buffer, buffer->count + 1U)) {
        return 0;
    }

    copy = edit_copy_line(text);

    if (copy == NULL) {
        return 0;
    }

    index = before_line - 1U;

    if (index < buffer->count) {
        (void)memmove(&buffer->lines[index + 1U],
                      &buffer->lines[index],
                      (buffer->count - index) *
                          sizeof(*buffer->lines));
    }

    buffer->lines[index] = copy;
    ++buffer->count;
    return 1;
}

static int edit_buffer_delete(edit_buffer *buffer,
                              size_t first,
                              size_t last)
{
    size_t index;
    size_t remove_count;

    if (first < 1U ||
        last < first ||
        last > buffer->count) {
        return 0;
    }

    for (index = first - 1U; index < last; ++index) {
        free(buffer->lines[index]);
    }

    remove_count = last - first + 1U;

    if (last < buffer->count) {
        (void)memmove(&buffer->lines[first - 1U],
                      &buffer->lines[last],
                      (buffer->count - last) *
                          sizeof(*buffer->lines));
    }

    buffer->count -= remove_count;
    return 1;
}

static int edit_buffer_replace(edit_buffer *buffer,
                               size_t line_number,
                               const char *text)
{
    char *copy;

    if (line_number < 1U ||
        line_number > buffer->count) {
        return 0;
    }

    copy = edit_copy_line(text);

    if (copy == NULL) {
        return 0;
    }

    free(buffer->lines[line_number - 1U]);
    buffer->lines[line_number - 1U] = copy;
    return 1;
}

static int edit_clipboard_copy(const edit_buffer *buffer,
                               edit_clipboard *clipboard,
                               size_t first,
                               size_t last)
{
    size_t index;
    size_t count;
    char **lines;

    if (first < 1U ||
        last < first ||
        last > buffer->count) {
        return 0;
    }

    count = last - first + 1U;
    lines = calloc(count, sizeof(*lines));

    if (lines == NULL) {
        return 0;
    }

    for (index = 0U; index < count; ++index) {
        lines[index] =
            edit_copy_line(buffer->lines[first - 1U + index]);

        if (lines[index] == NULL) {
            size_t cleanup;

            for (cleanup = 0U; cleanup < index; ++cleanup) {
                free(lines[cleanup]);
            }

            free(lines);
            return 0;
        }
    }

    edit_clipboard_free(clipboard);
    clipboard->lines = lines;
    clipboard->count = count;
    return 1;
}

static int edit_clipboard_paste(edit_buffer *buffer,
                                const edit_clipboard *clipboard,
                                size_t before_line)
{
    size_t index;

    if (clipboard->count == 0U) {
        return 0;
    }

    if (before_line < 1U ||
        before_line > buffer->count + 1U) {
        return 0;
    }

    if (buffer->count + clipboard->count > EDIT_MAX_LINES) {
        return 0;
    }

    for (index = 0U; index < clipboard->count; ++index) {
        if (!edit_buffer_insert(buffer,
                                before_line + index,
                                clipboard->lines[index])) {
            return 0;
        }
    }

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
    (void)puts("  g <line>          Print five lines around line");
    (void)puts("  a                 Append lines; end with a single .");
    (void)puts("  i <line>          Insert before line; end with a single .");
    (void)puts("  d <line>          Delete one line");
    (void)puts("  d <first> <last>  Delete a line range");
    (void)puts("  r <line>          Replace one line");
    (void)puts("  f text            Find next matching line");
    (void)puts("  s/old/new/        Replace first match in every line");
    (void)puts("  c <first> <last>  Copy a range");
    (void)puts("  x <first> <last>  Cut a range");
    (void)puts("  v <line>          Paste before line");
    (void)puts("  u                 Undo");
    (void)puts("  y                 Redo");
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

static int edit_insert_mode(edit_buffer *buffer, size_t before_line)
{
    char line[EDIT_LINE_SIZE];
    size_t current_line;

    if (before_line < 1U ||
        before_line > buffer->count + 1U) {
        (void)puts("Invalid insertion line.");
        return 0;
    }

    current_line = before_line;
    (void)puts("Enter text. A single period ends insert mode.");

    for (;;) {
        (void)fputs("INSERT> ", stdout);
        (void)fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 0;
        }

        if (strcmp(line, ".\n") == 0 ||
            strcmp(line, ".\r\n") == 0 ||
            strcmp(line, ".") == 0) {
            return 1;
        }

        if (!edit_buffer_insert(buffer, current_line, line)) {
            (void)puts("Unable to insert line.");
            return 0;
        }

        ++current_line;
        buffer->modified = 1;
    }
}

static int edit_replace_mode(edit_buffer *buffer, size_t line_number)
{
    char line[EDIT_LINE_SIZE];

    if (line_number < 1U ||
        line_number > buffer->count) {
        (void)puts("Invalid replacement line.");
        return 0;
    }

    (void)printf("Replace line %lu:\n",
                 (unsigned long)line_number);
    (void)fputs("REPLACE> ", stdout);
    (void)fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
        return 0;
    }

    if (!edit_buffer_replace(buffer, line_number, line)) {
        (void)puts("Unable to replace line.");
        return 0;
    }

    buffer->modified = 1;
    return 1;
}

static size_t edit_find(const edit_buffer *buffer,
                        const char *text,
                        size_t start_line)
{
    size_t index;

    if (text == NULL || *text == '\0' || buffer->count == 0U) {
        return 0U;
    }

    if (start_line < 1U || start_line > buffer->count) {
        start_line = 1U;
    }

    for (index = start_line - 1U; index < buffer->count; ++index) {
        if (strstr(buffer->lines[index], text) != NULL) {
            return index + 1U;
        }
    }

    for (index = 0U; index + 1U < start_line; ++index) {
        if (strstr(buffer->lines[index], text) != NULL) {
            return index + 1U;
        }
    }

    return 0U;
}

static int edit_substitute(edit_buffer *buffer,
                           const char *old_text,
                           const char *new_text,
                           unsigned long *changed)
{
    size_t index;
    size_t old_length;
    size_t new_length;

    *changed = 0UL;
    old_length = strlen(old_text);
    new_length = strlen(new_text);

    if (old_length == 0U) {
        return 0;
    }

    for (index = 0U; index < buffer->count; ++index) {
        char *match;

        match = strstr(buffer->lines[index], old_text);

        if (match != NULL) {
            size_t prefix_length;
            size_t suffix_length;
            char *replacement;

            prefix_length =
                (size_t)(match - buffer->lines[index]);
            suffix_length = strlen(match + old_length);

            replacement = malloc(prefix_length +
                                 new_length +
                                 suffix_length +
                                 1U);

            if (replacement == NULL) {
                return 0;
            }

            (void)memcpy(replacement,
                         buffer->lines[index],
                         prefix_length);
            (void)memcpy(replacement + prefix_length,
                         new_text,
                         new_length);
            (void)memcpy(replacement +
                             prefix_length +
                             new_length,
                         match + old_length,
                         suffix_length + 1U);

            free(buffer->lines[index]);
            buffer->lines[index] = replacement;
            ++(*changed);
        }
    }

    if (*changed != 0UL) {
        buffer->modified = 1;
    }

    return 1;
}

static int edit_parse_substitute(char *command,
                                 char **old_text,
                                 char **new_text)
{
    char delimiter;
    char *first;
    char *second;
    char *third;

    delimiter = command[1];

    if (delimiter == '\0' ||
        delimiter == '\n' ||
        delimiter == '\r') {
        return 0;
    }

    first = command + 2;
    second = strchr(first, delimiter);

    if (second == NULL) {
        return 0;
    }

    *second = '\0';
    ++second;
    third = strchr(second, delimiter);

    if (third == NULL) {
        return 0;
    }

    *third = '\0';
    *old_text = first;
    *new_text = second;
    return **old_text != '\0';
}

void edit_run(agent_state *state, const char *path)
{
    edit_buffer buffer;
    edit_clipboard clipboard;
    edit_history undo;
    edit_history redo;
    size_t find_cursor;
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
    edit_clipboard_init(&clipboard);
    edit_history_init(&undo);
    edit_history_init(&redo);
    find_cursor = 1U;

    if (!edit_load(path, &buffer)) {
        edit_buffer_free(&buffer);
        edit_clipboard_free(&clipboard);
        edit_history_clear(&undo);
        edit_history_clear(&redo);
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

        if (op == 'g' || op == 'G') {
            size_t start;
            size_t end;

            first = 0UL;
            fields = sscanf(command + 1, "%lu", &first);

            if (fields != 1 ||
                first < 1UL ||
                first > (unsigned long)buffer.count) {
                (void)puts("Usage: G line");
                continue;
            }

            start = first > 2UL ? (size_t)first - 2U : 1U;
            end = (size_t)first + 2U;

            if (end > buffer.count) {
                end = buffer.count;
            }

            edit_print(&buffer, start, end);
            continue;
        }

        if (op == 'a' || op == 'A') {
            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);
            (void)edit_append_mode(&buffer);
            continue;
        }

        if (op == 'i' || op == 'I') {
            first = 0UL;
            fields = sscanf(command + 1, "%lu", &first);

            if (fields != 1) {
                (void)puts("Usage: I line");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);
            (void)edit_insert_mode(&buffer, (size_t)first);
            continue;
        }

        if (op == 'd' || op == 'D') {
            first = 0UL;
            last = 0UL;
            fields = sscanf(command + 1, "%lu %lu", &first, &last);

            if (fields == 1) {
                last = first;
            } else if (fields != 2) {
                (void)puts("Usage: D line | D first last");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);

            if (!edit_buffer_delete(&buffer,
                                    (size_t)first,
                                    (size_t)last)) {
                (void)puts("Invalid delete range.");
                (void)edit_history_pop(&undo, &buffer);
                continue;
            }

            buffer.modified = 1;
            continue;
        }

        if (op == 'r' || op == 'R') {
            first = 0UL;
            fields = sscanf(command + 1, "%lu", &first);

            if (fields != 1) {
                (void)puts("Usage: R line");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);
            (void)edit_replace_mode(&buffer, (size_t)first);
            continue;
        }

        if (op == 'f' || op == 'F') {
            char *text;
            size_t found;

            text = command + 1;

            while (*text == ' ' || *text == '\t') {
                ++text;
            }

            text[strcspn(text, "\r\n")] = '\0';

            if (*text == '\0') {
                (void)puts("Usage: F text");
                continue;
            }

            found = edit_find(&buffer, text, find_cursor);

            if (found == 0U) {
                (void)puts("Text not found.");
            } else {
                edit_print(&buffer, found, found);
                find_cursor = found + 1U;

                if (find_cursor > buffer.count) {
                    find_cursor = 1U;
                }
            }

            continue;
        }

        if (op == 's' || op == 'S') {
            char *old_text;
            char *new_text;
            unsigned long changed;

            if (!edit_parse_substitute(command,
                                       &old_text,
                                       &new_text)) {
                (void)puts("Usage: S/old/new/");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);

            if (!edit_substitute(&buffer,
                                 old_text,
                                 new_text,
                                 &changed)) {
                (void)puts("Substitution failed.");
                (void)edit_history_pop(&undo, &buffer);
                continue;
            }

            if (changed == 0UL) {
                (void)puts("No matches.");
                (void)edit_history_pop(&undo, &buffer);
            } else {
                (void)printf("%lu line%s changed.\n",
                             changed,
                             changed == 1UL ? "" : "s");
            }

            continue;
        }

        if (op == 'c' || op == 'C') {
            first = 0UL;
            last = 0UL;
            fields = sscanf(command + 1, "%lu %lu", &first, &last);

            if (fields == 1) {
                last = first;
            } else if (fields != 2) {
                (void)puts("Usage: C line | C first last");
                continue;
            }

            if (!edit_clipboard_copy(&buffer,
                                     &clipboard,
                                     (size_t)first,
                                     (size_t)last)) {
                (void)puts("Unable to copy range.");
                continue;
            }

            (void)printf("%lu line%s copied.\n",
                         (unsigned long)clipboard.count,
                         clipboard.count == 1U ? "" : "s");
            continue;
        }

        if (op == 'x' || op == 'X') {
            first = 0UL;
            last = 0UL;
            fields = sscanf(command + 1, "%lu %lu", &first, &last);

            if (fields == 1) {
                last = first;
            } else if (fields != 2) {
                (void)puts("Usage: X line | X first last");
                continue;
            }

            if (!edit_clipboard_copy(&buffer,
                                     &clipboard,
                                     (size_t)first,
                                     (size_t)last)) {
                (void)puts("Unable to copy cut range.");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);

            if (!edit_buffer_delete(&buffer,
                                    (size_t)first,
                                    (size_t)last)) {
                (void)puts("Unable to cut range.");
                (void)edit_history_pop(&undo, &buffer);
                continue;
            }

            buffer.modified = 1;
            continue;
        }

        if (op == 'v' || op == 'V') {
            first = 0UL;
            fields = sscanf(command + 1, "%lu", &first);

            if (fields != 1) {
                (void)puts("Usage: V line");
                continue;
            }

            if (clipboard.count == 0U) {
                (void)puts("Clipboard is empty.");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            edit_history_clear(&redo);

            if (!edit_clipboard_paste(&buffer,
                                      &clipboard,
                                      (size_t)first)) {
                (void)puts("Unable to paste clipboard.");
                (void)edit_history_pop(&undo, &buffer);
                continue;
            }

            buffer.modified = 1;
            continue;
        }

        if (op == 'u' || op == 'U') {
            if (undo.count == 0U) {
                (void)puts("Nothing to undo.");
                continue;
            }

            if (!edit_history_push(&redo, &buffer)) {
                continue;
            }

            (void)edit_history_pop(&undo, &buffer);
            (void)puts("Undo complete.");
            continue;
        }

        if (op == 'y' || op == 'Y') {
            if (redo.count == 0U) {
                (void)puts("Nothing to redo.");
                continue;
            }

            if (!edit_history_push(&undo, &buffer)) {
                continue;
            }

            (void)edit_history_pop(&redo, &buffer);
            (void)puts("Redo complete.");
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
    edit_clipboard_free(&clipboard);
    edit_history_clear(&undo);
    edit_history_clear(&redo);
}
