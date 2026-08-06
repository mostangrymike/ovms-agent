#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "project.h"

#define READ_BUFFER_SIZE 1024
#define NORMALIZED_PATH_SIZE 1024
#define TREE_MAX_DEPTH 16
#define GREP_MAX_DEPTH 16
#define GREP_MAX_MATCHES 100UL
#define GREP_CONTEXT_MAX 20U

static int path_is_safe(const char *path);
static int normalize_project_path(const char *input,
                                  char *output,
                                  size_t output_size)
{
    const char *position;
    size_t used;

    if (input == NULL ||
        output == NULL ||
        output_size == 0U) {
        return 0;
    }

    if (*input == '\0') {
        if (output_size < 2U) {
            return 0;
        }

        output[0] = '.';
        output[1] = '\0';
        return 1;
    }

    if (!path_is_safe(input)) {
        return 0;
    }

    position = input;
    used = 0U;

    /*
     * Convert:
     *
     *   [.SRC]MAIN.C
     *
     * to:
     *
     *   SRC/MAIN.C
     */
    if (position[0] == '[' &&
        position[1] == '.') {
        position += 2;

        while (*position != '\0' &&
               *position != ']') {
            char ch;

            ch = *position++;

            if (ch == '.') {
                ch = '/';
            }

            if (used + 1U >= output_size) {
                return 0;
            }

            output[used++] = ch;
        }

        if (*position != ']') {
            return 0;
        }

        ++position;

        if (*position != '\0') {
            if (used != 0U &&
                output[used - 1U] != '/') {
                if (used + 1U >= output_size) {
                    return 0;
                }

                output[used++] = '/';
            }

            while (*position != '\0') {
                if (used + 1U >= output_size) {
                    return 0;
                }

                output[used++] = *position++;
            }
        }
    } else {
        while (*position != '\0') {
            if (used + 1U >= output_size) {
                return 0;
            }

            output[used++] = *position++;
        }
    }

    if (used == 0U) {
        if (output_size < 2U) {
            return 0;
        }

        output[0] = '.';
        output[1] = '\0';
        return 1;
    }

    output[used] = '\0';
    return 1;
}

static void tree_indent(unsigned int depth)
{
    unsigned int index;

    for (index = 0U; index < depth; ++index) {
        (void)fputs("  ", stdout);
    }
}

static int tree_join_path(const char *parent,
                          const char *child,
                          char *output,
                          size_t output_size)
{
    int written;

    if (parent == NULL ||
        child == NULL ||
        output == NULL ||
        output_size == 0U) {
        return 0;
    }

    if (strcmp(parent, ".") == 0) {
        written = snprintf(output,
                           output_size,
                           "%s",
                           child);
    } else {
        written = snprintf(output,
                           output_size,
                           "%s/%s",
                           parent,
                           child);
    }

    return written >= 0 &&
           (size_t)written < output_size;
}

static void project_tree_walk(const char *path,
                              unsigned int depth)
{
    DIR *directory;
    struct dirent *entry;

    if (depth > TREE_MAX_DEPTH) {
        tree_indent(depth);
        (void)puts("[maximum depth reached]");
        return;
    }

    directory = opendir(path);

    if (directory == NULL) {
        tree_indent(depth);
        (void)printf("[unable to open %s: %s]\n",
                     path,
                     strerror(errno));
        return;
    }

    while ((entry = readdir(directory)) != NULL) {
        char child_path[NORMALIZED_PATH_SIZE];
        DIR *child_directory;
        int is_directory;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, ".git", 4U) == 0) {
            continue;
        }

        if (!tree_join_path(path,
                            entry->d_name,
                            child_path,
                            sizeof(child_path))) {
            tree_indent(depth);
            (void)puts("[path too long]");
            continue;
        }

        child_directory = opendir(child_path);
        is_directory = child_directory != NULL;

        if (child_directory != NULL) {
            (void)closedir(child_directory);
        }

        tree_indent(depth);
        (void)printf("%s%s\n",
                     entry->d_name,
                     is_directory ? "/" : "");

        if (is_directory) {
            project_tree_walk(child_path, depth + 1U);
        }
    }

    if (closedir(directory) != 0) {
        tree_indent(depth);
        (void)printf("[unable to close %s: %s]\n",
                     path,
                     strerror(errno));
    }
}

static int path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (strstr(path, "..") != NULL) {
        return 0;
    }

    if (strchr(path, ':') != NULL) {
        return 0;
    }

    if (*path == '/') {
        return 0;
    }

    if (strchr(path, '[') != NULL &&
        strncmp(path, "[.", 2U) != 0) {
        return 0;
    }

    return 1;
}

static int is_old_version(const char *name)
{
    const char *semicolon;
    const char *position;

    semicolon = strrchr(name, ';');

    if (semicolon == NULL || semicolon[1] == '\0') {
        return 0;
    }

    position = semicolon + 1;

    while (*position != '\0') {
        if (!isdigit((unsigned char)*position)) {
            return 0;
        }
        ++position;
    }

    return strcmp(semicolon, ";1") != 0;
}

static int hide_default_entry(const char *name)
{
    if (strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 1;
    }

    if (strncmp(name, "^.git", 5U) == 0) {
        return 1;
    }

    if (is_old_version(name)) {
        return 1;
    }

    return 0;
}

static int text_contains_ignore_case(const char *text,
                                     const char *pattern)
{
    const unsigned char *start;

    if (text == NULL ||
        pattern == NULL ||
        *pattern == '\0') {
        return 0;
    }

    for (start = (const unsigned char *)text;
         *start != (unsigned char)'\0';
         ++start) {
        const unsigned char *left;
        const unsigned char *right;

        left = start;
        right = (const unsigned char *)pattern;

        while (*left != (unsigned char)'\0' &&
               *right != (unsigned char)'\0' &&
               tolower((int)*left) == tolower((int)*right)) {
            ++left;
            ++right;
        }

        if (*right == (unsigned char)'\0') {
            return 1;
        }
    }

    return 0;
}

static int grep_file_type(const char *name)
{
    const char *dot;
    char extension[8];
    size_t index;

    if (name == NULL) {
        return 0;
    }

    dot = strrchr(name, '.');
    if (dot == NULL || dot[1] == '\0') {
        return 0;
    }

    ++dot;
    index = 0U;

    while (dot[index] != '\0' &&
           dot[index] != ';' &&
           index + 1U < sizeof(extension)) {
        extension[index] =
            (char)toupper((unsigned char)dot[index]);
        ++index;
    }

    extension[index] = '\0';

    return strcmp(extension, "C") == 0 ||
           strcmp(extension, "H") == 0 ||
           strcmp(extension, "COM") == 0 ||
           strcmp(extension, "OPT") == 0 ||
           strcmp(extension, "MD") == 0 ||
           strcmp(extension, "TXT") == 0;
}

static void grep_print_line(const char *path,
                            unsigned long line_number,
                            const char *text,
                            int is_match)
{
    (void)printf("%s%c%lu%c%s",
                 path,
                 is_match ? ':' : '-',
                 line_number,
                 is_match ? ':' : '-',
                 text);

    if (strchr(text, '\n') == NULL) {
        (void)putchar('\n');
    }
}

static void grep_one_file(const char *path,
                          const char *pattern,
                          unsigned int context,
                          unsigned long match_limit,
                          int case_sensitive,
                          int count_only,
                          unsigned long *matches)
{
    FILE *file;
    char buffer[READ_BUFFER_SIZE];
    char previous[GREP_CONTEXT_MAX][READ_BUFFER_SIZE];
    unsigned long previous_line[GREP_CONTEXT_MAX];
    unsigned int previous_used;
    unsigned int previous_next;
    unsigned int after_remaining;
    unsigned long line_number;
    unsigned long last_printed;
    unsigned int index;
    unsigned int slot;

    if (path == NULL ||
        pattern == NULL ||
        matches == NULL ||
        context > GREP_CONTEXT_MAX ||
        match_limit == 0UL ||
        match_limit > GREP_MAX_MATCHES ||
        *matches >= match_limit) {
        return;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    previous_used = 0U;
    previous_next = 0U;
    after_remaining = 0U;
    line_number = 1UL;
    last_printed = 0UL;

    while (*matches < match_limit &&   
           fgets(buffer, sizeof(buffer), file) != NULL) {
        int is_match;

        if (case_sensitive) {
            is_match = strstr(buffer, pattern) != NULL;
        } else {
            is_match = text_contains_ignore_case(
                buffer,
                pattern
            );
        }

        if (is_match) {
            /*
             * Print buffered lines preceding this match. Lines that
             * were already printed as trailing context are skipped.
             */
            for (index = 0U;
                 index < previous_used;
                 ++index) {
                if (previous_used < context) {
                    slot = index;
                } else {
                    slot = (previous_next + index) % context;
                }

                if (!count_only &&
                    previous_line[slot] > last_printed) {
                    grep_print_line(path,
                                    previous_line[slot],
                                    previous[slot],
                                    0);
                    last_printed = previous_line[slot];
                }
            }

            if (!count_only &&
                line_number > last_printed) {
                grep_print_line(path,
                                line_number,
                                buffer,
                                1);
                last_printed = line_number;
            }

            ++*matches;
            after_remaining = context;
        } else if (after_remaining != 0U) {
            if (!count_only &&
                line_number > last_printed) {
                grep_print_line(path,
                                line_number,
                                buffer,
                                0);
                last_printed = line_number;
            }

            --after_remaining;
        }

        if (context != 0U) {
            (void)strcpy(previous[previous_next],
                         buffer);
            previous_line[previous_next] =
                line_number;

            previous_next =
                (previous_next + 1U) % context;

            if (previous_used < context) {
                ++previous_used;
            }
        }

        ++line_number;
    }

    (void)fclose(file);
}

static void project_grep_walk(const char *path,
                              const char *pattern,
                              unsigned int depth,
                              unsigned int context,
                              unsigned long match_limit,
                              int case_sensitive,
                              int count_only,
                              unsigned long *matches)
{
    DIR *directory;
    struct dirent *entry;

    if (path == NULL ||
        pattern == NULL ||
        matches == NULL ||
        context > GREP_CONTEXT_MAX ||
        match_limit == 0UL ||
        match_limit > GREP_MAX_MATCHES ||
        depth > GREP_MAX_DEPTH ||
        *matches >= match_limit) {
        return;
    }

    directory = opendir(path);
    if (directory == NULL) {
        return;
    }

    while (*matches < match_limit &&
           (entry = readdir(directory)) != NULL) {
        char child_path[NORMALIZED_PATH_SIZE];
        DIR *child_directory;

        if (hide_default_entry(entry->d_name) ||
            strncmp(entry->d_name, ".git", 4U) == 0 ||
            strncmp(entry->d_name, "^.git", 5U) == 0 ||
            strcmp(entry->d_name, "build") == 0 ||
            strcmp(entry->d_name, "backup_modular") == 0 ||
            strcmp(entry->d_name, "m150a_backup") == 0 ||
            strcmp(entry->d_name, "m150a_stage") == 0) {
            continue;
        }

        if (!tree_join_path(path,
                            entry->d_name,
                            child_path,
                            sizeof(child_path))) {
            continue;
        }

        child_directory = opendir(child_path);

        if (child_directory != NULL) {
            (void)closedir(child_directory);
            project_grep_walk(child_path,
                              pattern,
                              depth + 1U,
                              context,
                              match_limit,
                              case_sensitive,
                              count_only,
                              matches);            
        } else if (grep_file_type(entry->d_name)) {
            grep_one_file(child_path,
                          pattern,
                          context,
                          match_limit,
                          case_sensitive,
                          count_only,
                          matches);
        }
    }

    (void)closedir(directory);
}

void project_grep(const agent_state *state,
                  const char *pattern,
                  const char *path,
                  unsigned int context,
                  unsigned long match_limit,
                  int case_sensitive,
                  int count_only)
{
    unsigned long matches;
    char normalized[NORMALIZED_PATH_SIZE];
    DIR *directory;
    FILE *file;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts(
            "OVMS_AGENT_ROOT is not defined."
        );
        return;
    }

    if (pattern == NULL || *pattern == '\0') {
        (void)puts("GREP text cannot be empty.");
        return;
    }

    if (context > GREP_CONTEXT_MAX) {
        (void)puts(
            "GREP context is out of range."
        );
        return;
    }

    if (match_limit == 0UL ||
        match_limit > GREP_MAX_MATCHES) {
        (void)puts(
            "GREP match limit is out of range."
        );
        return;
    }

    matches = 0UL;

    if (path == NULL || *path == '\0') {
        project_grep_walk("SRC",
                          pattern,
                          0U,
                          context,
                          match_limit,
                          case_sensitive,
                          count_only,
                          &matches);

        if (matches < match_limit) {
            project_grep_walk("DOC",
                              pattern,
                              0U,
                              context,
                              match_limit,
                              case_sensitive,
                              count_only,
                              &matches);
        }

        if (matches < match_limit) {
            project_grep_walk("TEST",
                              pattern,
                              0U,
                              context,
                              match_limit,
                              case_sensitive,
                              count_only,
                              &matches);
        }
    } else {
        if (!normalize_project_path(
                path,
                normalized,
                sizeof(normalized))) {
            (void)puts(
                "Unsafe or invalid "
                "project-relative path."
            );
            return;
        }

        directory = opendir(normalized);

        if (directory != NULL) {
            (void)closedir(directory);

            project_grep_walk(normalized,
                              pattern,
                              0U,
                              context,
                              match_limit,
                              case_sensitive,
                              count_only,
                              &matches);
        } else {
            file = fopen(normalized, "r");

            if (file == NULL) {
                (void)printf(
                    "Unable to search %s: %s\n",
                    path,
                    strerror(errno)
                );
                return;
            }

            (void)fclose(file);

            if (!grep_file_type(normalized)) {
                (void)printf(
                    "Unsupported GREP file type: %s\n",
                    path
                );
                return;
            }

            grep_one_file(normalized,
                          pattern,
                          context,
                          match_limit,
                          case_sensitive,
                          count_only,
                          &matches);
        }
    }

    if (matches >= match_limit) {
        (void)printf(
            "%lu match%s shown; "
            "result limit reached.\n",
            matches,
            matches == 1UL ? "" : "es"
        );
    } else {
        (void)printf(
            "%lu match%s found.\n",
            matches,
            matches == 1UL ? "" : "es"
        );
    }
}
                  
void project_show_root(const agent_state *state)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)printf("Project root: %s\n", state->project_root);
}

void project_show_status(const agent_state *state)
{
    if (state == NULL) {
        (void)puts("Agent state is unavailable.");
        return;
    }

    (void)printf("Version:       %s\n", OVMS_AGENT_VERSION);
    (void)printf("Project root:  %s\n",
        state->project_root != NULL &&
        *state->project_root != '\0'
            ? state->project_root
            : "not defined");
    (void)printf("API key:       %s\n",
        state->api_key_defined ? "defined" : "not defined");
    (void)printf("Write access:  %s\n",
        state->write_enabled ? "enabled" : "disabled");
    (void)printf("DCL execution: %s\n",
        state->dcl_enabled ? "enabled" : "disabled");
}

void project_search(const agent_state *state,
                    const char *path,
                    const char *pattern)
{
    FILE *file;
    char buffer[1024];
    unsigned long line_number;
char normalized[NORMALIZED_PATH_SIZE];
    unsigned long matches;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!normalize_project_path(path,
                            normalized,
                            sizeof(normalized))) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

file = fopen(normalized, "r");

    if (file == NULL) {
        (void)printf("Unable to search %s: %s\n",
                     path,
                     strerror(errno));
        return;
    }

    line_number = 1UL;
    matches = 0UL;

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (text_contains_ignore_case(buffer, pattern)) {
            (void)printf("%6lu  %s", line_number, buffer);

            if (strchr(buffer, '\n') == NULL) {
                (void)putchar('\n');
            }

            ++matches;
        }

        ++line_number;
    }

    if (ferror(file)) {
        (void)printf("Search failed: %s\n", strerror(errno));
    }

    if (fclose(file) != 0) {
        (void)printf("Unable to close %s: %s\n",
                     path,
                     strerror(errno));
    }

    (void)printf("%lu matching line%s.\n",
                 matches,
                 matches == 1UL ? "" : "s");
}

void project_tree(const agent_state *state,
                  const char *path)
{
    char normalized[NORMALIZED_PATH_SIZE];

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!normalize_project_path(path,
                                normalized,
                                sizeof(normalized))) {
        (void)puts("Unsafe or invalid project-relative path.");
        return;
    }

    (void)printf("%s/\n", normalized);
    project_tree_walk(normalized, 1U);
}

void project_list(const agent_state *state, const char *path, int show_all)
{
    DIR *directory;
    struct dirent *entry;
    const char *target;
    char normalized[NORMALIZED_PATH_SIZE];
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

   if (!normalize_project_path(path,
                            normalized,
                            sizeof(normalized))) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

target = normalized;

directory = opendir(target);


    if (directory == NULL) {
        (void)printf("Unable to open directory %s: %s\n",
                     target,
                     strerror(errno));
        return;
    }

    (void)printf("Directory: %s\n\n", target);

    while ((entry = readdir(directory)) != NULL) {
        if (show_all || !hide_default_entry(entry->d_name)) {
            (void)printf("%s\n", entry->d_name);
        }
    }

    if (closedir(directory) != 0) {
        (void)printf("Warning: unable to close directory: %s\n",
                     strerror(errno));
    }
}



void project_git_status(const agent_state *state)
{
    int status;

    (void)state;

    puts("Git status:");
    puts("");

    status = system("git status --short");

    if ((status & 1) == 0) {
        (void)printf("Git failed with OpenVMS status %d\n", status);
    }
}

void project_git_diff(const agent_state *state)
{
    int status;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)puts("Git diff:");
    (void)puts("");

    status = system("git diff --");

    if ((status & 1) == 0) {
        (void)printf(
            "Git diff failed with OpenVMS status %d.\n",
            status
        );
    }
}

void project_build(const agent_state *state)
{
    int status;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)puts("Building project...");
    (void)puts("");

    status = system("@BUILD.COM");

    (void)puts("");

    if ((status & 1) != 0) {
        (void)puts("Build completed successfully.");
    } else {
        (void)printf(
            "Build failed with OpenVMS status %d.\n",
            status
        );
    }
}

void project_read(const agent_state *state,
                  const char *path,
                  unsigned long start_line,
                  unsigned long line_count)
{
    FILE *file;
    char buffer[READ_BUFFER_SIZE];
    unsigned long line_number;
    char normalized[NORMALIZED_PATH_SIZE];
    unsigned long printed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!normalize_project_path(path,
                            normalized,
                            sizeof(normalized))) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

file = fopen(normalized, "r");


    if (file == NULL) {
        (void)printf("Unable to read %s\n", normalized);
        (void)printf("Reason: %s\n", strerror(errno));
        return;
    }

if (start_line == 0UL) {
    start_line = 1UL;
}

line_number = 1UL;
printed = 0UL;

while (fgets(buffer, sizeof(buffer), file) != NULL) {
    if (line_number >= start_line) {
        (void)printf("%6lu  %s", line_number, buffer);

        if (strchr(buffer, '\n') == NULL) {
            (void)putchar('\n');
        }

        ++printed;

        if (line_count != 0UL &&
            printed >= line_count) {
            break;
        }
    }

    ++line_number;
}











    if (ferror(file)) {
        (void)printf("Read error: %s\n", strerror(errno));
    }

    if (fclose(file) != 0) {
        (void)printf("Warning: unable to close file: %s\n",
                     strerror(errno));
    }
}
int project_patch(const agent_state *state,
                  const char *path,
                  const char *old_text,
                  const char *new_text)
                 
{
    FILE *file;
    char *original;
    char *updated;
    char answer[32];
    long length;
    size_t old_length;
char normalized[NORMALIZED_PATH_SIZE];
    size_t new_length;
char *line_start;
char *line_end;
size_t before_length;
size_t after_length;
    size_t prefix_length;
    size_t updated_length;
    char *match;
    char *second_match;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!normalize_project_path(path,
                            normalized,
                            sizeof(normalized))) {
    (void)puts("Unsafe or invalid project-relative path.");
    return 0;
}

    if (old_text == NULL || *old_text == '\0') {
        (void)puts("Original text cannot be empty.");
        return 0;
    }

    if (new_text == NULL) {
        (void)puts("Replacement text is invalid.");
        return 0;
    }

    file = fopen(normalized, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n",
                     normalized,
                     strerror(errno));
        return 0;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)puts("Unable to determine file size.");
        (void)fclose(file);
        return 0;
    }

    length = ftell(file);

    if (length < 0L) {
        (void)puts("Unable to determine file size.");
        (void)fclose(file);
        return 0;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)puts("Unable to rewind file.");
        (void)fclose(file);
        return 0;
    }

    original = malloc((size_t)length + 1U);

    if (original == NULL) {
        (void)puts("Insufficient memory.");
        (void)fclose(file);
        return 0;
    }

    length = (long)fread(original, 1U, (size_t)length, file);
    original[length] = '\0';
    (void)fclose(file);

    match = strstr(original, old_text);

    if (match == NULL) {
        (void)printf("Text not found in %s.\n", path);
        free(original);
        return 0;
    }

    second_match = strstr(match + strlen(old_text), old_text);

    if (second_match != NULL) {
        (void)puts("Patch refused: original text occurs more than once.");
        free(original);
        return 0;
    }

    old_length = strlen(old_text);
    new_length = strlen(new_text);
    prefix_length = (size_t)(match - original);
    updated_length =
        strlen(original) - old_length + new_length;

    updated = malloc(updated_length + 1U);

    if (updated == NULL) {
        (void)puts("Insufficient memory.");
        free(original);
        return 0;
    }

    (void)memcpy(updated, original, prefix_length);
    (void)memcpy(updated + prefix_length,
                 new_text,
                 new_length);
    (void)strcpy(updated + prefix_length + new_length,
                 match + old_length);

    line_start = match;

while (line_start > original &&
       line_start[-1] != '\n') {
    --line_start;
}

line_end = match + old_length;

while (*line_end != '\0' &&
       *line_end != '\n') {
    ++line_end;
}

before_length = (size_t)(match - line_start);
after_length =
    (size_t)(line_end - (match + old_length));

(void)printf("File: %s\n", normalized);
(void)puts("");
(void)printf("Current:  %.*s%s%.*s\n",
             (int)before_length,
             line_start,
             old_text,
             (int)after_length,
             match + old_length);

(void)printf("Proposed: %.*s%s%.*s\n",
             (int)before_length,
             line_start,
             new_text,
             (int)after_length,
             match + old_length);

(void)puts("");
(void)fputs("Apply patch [y/N]? ", stdout);

    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL ||
        (answer[0] != 'Y' && answer[0] != 'y')) {
        (void)puts("Patch cancelled.");
        free(updated);
        free(original);
        return 0;
    }

    file = fopen(normalized, "w");

    if (file == NULL) {
        (void)printf("Unable to write %s: %s\n",
                     normalized,
                     strerror(errno));
        free(updated);
        free(original);
        return 0;
    }

    if (fwrite(updated, 1U, updated_length, file) != updated_length) {
        (void)puts("Unable to write complete replacement file.");
        (void)fclose(file);
        free(updated);
        free(original);
        return 0;
    }

    if (fclose(file) != 0) {
        (void)printf("Unable to close replacement file: %s\n",
                     strerror(errno));
        free(updated);
        free(original);
        return 0;
    }

    free(updated);
    free(original);

    (void)puts("Patch applied. A new OpenVMS file version was created.");
    return 1;
}
