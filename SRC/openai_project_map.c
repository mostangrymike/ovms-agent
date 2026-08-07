#include "openai_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#define OPENAI_MAP_MAX_FILES 192U
#define OPENAI_MAP_PATH 256U
#define OPENAI_MAP_TEXT 12288U
#define OPENAI_MAP_DEPTH 4U

#define OPENAI_MAP_SOURCE 1
#define OPENAI_MAP_HEADER 2
#define OPENAI_MAP_TEST 3
#define OPENAI_MAP_BUILD 4
#define OPENAI_MAP_OTHER 5

typedef struct openai_map_entry {
    char path[OPENAI_MAP_PATH];
    int kind;
} openai_map_entry;

static openai_map_entry openai_map_files[OPENAI_MAP_MAX_FILES];
static unsigned int openai_map_count = 0U;
static unsigned int openai_map_sources = 0U;
static unsigned int openai_map_headers = 0U;
static unsigned int openai_map_tests = 0U;
static unsigned int openai_map_builds = 0U;
static unsigned int openai_map_others = 0U;
static int openai_map_loaded = 0;
static int openai_map_truncated = 0;

static int openai_map_ci_equal(const char *left,
                               const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;

        if (a >= (unsigned char)'a' &&
            a <= (unsigned char)'z') {
            a = (unsigned char)(
                a - (unsigned char)'a' +
                (unsigned char)'A'
            );
        }

        if (b >= (unsigned char)'a' &&
            b <= (unsigned char)'z') {
            b = (unsigned char)(
                b - (unsigned char)'a' +
                (unsigned char)'A'
            );
        }

        if (a != b) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int openai_map_ci_contains(const char *text,
                                  const char *needle)
{
    const char *start;

    if (text == NULL || needle == NULL ||
        *needle == '\0') {
        return 0;
    }

    for (start = text; *start != '\0'; ++start) {
        const char *left;
        const char *right;

        left = start;
        right = needle;

        while (*left != '\0' &&
               *right != '\0') {
            unsigned char a;
            unsigned char b;

            a = (unsigned char)*left;
            b = (unsigned char)*right;

            if (a >= (unsigned char)'a' &&
                a <= (unsigned char)'z') {
                a = (unsigned char)(
                    a - (unsigned char)'a' +
                    (unsigned char)'A'
                );
            }

            if (b >= (unsigned char)'a' &&
                b <= (unsigned char)'z') {
                b = (unsigned char)(
                    b - (unsigned char)'a' +
                    (unsigned char)'A'
                );
            }

            if (a != b) {
                break;
            }

            ++left;
            ++right;
        }

        if (*right == '\0') {
            return 1;
        }
    }

    return 0;
}

static int openai_map_old_version(const char *name)
{
    const char *semi;
    const char *pos;

    semi = strrchr(name, ';');

    if (semi == NULL || semi[1] == '\0') {
        return 0;
    }

    pos = semi + 1;

    while (*pos != '\0') {
        if (!isdigit((unsigned char)*pos)) {
            return 0;
        }
        ++pos;
    }

    return strcmp(semi, ";1") != 0;
}

static int openai_map_hide(const char *name)
{
    if (name == NULL ||
        strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 1;
    }

    if (strncmp(name, ".git", 4U) == 0 ||
        strncmp(name, "^.git", 5U) == 0) {
        return 1;
    }

    return 0;
}

static void openai_map_base_path(const char *path,
                                 char *output,
                                 size_t output_size)
{
    const char *semi;
    size_t length;

    if (output == NULL || output_size == 0U) {
        return;
    }

    if (path == NULL) {
        output[0] = '\0';
        return;
    }

    semi = strrchr(path, ';');

    if (semi != NULL && semi[1] != '\0') {
        const char *pos;
        int numeric;

        pos = semi + 1;
        numeric = 1;

        while (*pos != '\0') {
            if (!isdigit((unsigned char)*pos)) {
                numeric = 0;
                break;
            }
            ++pos;
        }

        if (numeric) {
            length = (size_t)(semi - path);

            if (length >= output_size) {
                length = output_size - 1U;
            }

            (void)memcpy(output, path, length);
            output[length] = '\0';
            return;
        }
    }

    (void)strncpy(output, path, output_size - 1U);
    output[output_size - 1U] = '\0';
}

static int openai_map_kind(const char *path)
{
    const char *dot;

    if (path == NULL) {
        return OPENAI_MAP_OTHER;
    }

    if (openai_map_ci_contains(path, "TEST") ||
        openai_map_ci_contains(path, "_TEST.")) {
        return OPENAI_MAP_TEST;
    }

    if (openai_map_ci_contains(path, "BUILD.COM") ||
        openai_map_ci_contains(path, ".OPT") ||
        openai_map_ci_contains(path, "DESCRIP.MMS") ||
        openai_map_ci_contains(path, "MAKEFILE")) {
        return OPENAI_MAP_BUILD;
    }

    dot = strrchr(path, '.');

    if (dot != NULL) {
        char extension[16];
        const char *end;
        size_t length;

        end = strchr(dot, ';');

        if (end == NULL) {
            end = dot + strlen(dot);
        }

        length = (size_t)(end - dot);

        if (length < sizeof(extension)) {
            (void)memcpy(extension, dot, length);
            extension[length] = '\0';

            if (openai_map_ci_equal(extension, ".C")) {
                return OPENAI_MAP_SOURCE;
            }

            if (openai_map_ci_equal(extension, ".H")) {
                return OPENAI_MAP_HEADER;
            }
        }
    }

    return OPENAI_MAP_OTHER;
}

static int openai_map_join(const char *parent,
                           const char *child,
                           char *output,
                           size_t output_size)
{
    int written;

    if (parent == NULL || child == NULL ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (strcmp(parent, ".") == 0) {
        written = snprintf(
            output, output_size, "%s", child
        );
    } else {
        written = snprintf(
            output, output_size, "%s/%s",
            parent, child
        );
    }

    return written >= 0 &&
           (size_t)written < output_size;
}

static void openai_map_add(const char *path)
{
    char logical_path[OPENAI_MAP_PATH];
    unsigned int index;
    int kind;

    openai_map_base_path(
        path, logical_path, sizeof(logical_path)
    );

    if (logical_path[0] == '\0') {
        return;
    }

    for (index = 0U; index < openai_map_count; ++index) {
        if (openai_map_ci_equal(
                openai_map_files[index].path,
                logical_path)) {
            return;
        }
    }

    if (openai_map_count >= OPENAI_MAP_MAX_FILES) {
        openai_map_truncated = 1;
        return;
    }

    kind = openai_map_kind(logical_path);

    (void)strncpy(
        openai_map_files[openai_map_count].path,
        logical_path,
        sizeof(openai_map_files[openai_map_count].path) - 1U
    );
    openai_map_files[openai_map_count].path[
        sizeof(openai_map_files[openai_map_count].path) - 1U
    ] = '\0';
    openai_map_files[openai_map_count].kind = kind;
    ++openai_map_count;

    if (kind == OPENAI_MAP_SOURCE) {
        ++openai_map_sources;
    } else if (kind == OPENAI_MAP_HEADER) {
        ++openai_map_headers;
    } else if (kind == OPENAI_MAP_TEST) {
        ++openai_map_tests;
    } else if (kind == OPENAI_MAP_BUILD) {
        ++openai_map_builds;
    } else {
        ++openai_map_others;
    }
}

static void openai_map_walk(const char *path,
                            unsigned int depth)
{
    DIR *directory;
    struct dirent *entry;

    if (depth > OPENAI_MAP_DEPTH ||
        openai_map_count >= OPENAI_MAP_MAX_FILES) {
        return;
    }

    directory = opendir(path);

    if (directory == NULL) {
        return;
    }

    while ((entry = readdir(directory)) != NULL) {
        char child[OPENAI_MAP_PATH];
        DIR *candidate;

        if (openai_map_hide(entry->d_name)) {
            continue;
        }

        if (!openai_map_join(
                path, entry->d_name,
                child, sizeof(child))) {
            openai_map_truncated = 1;
            continue;
        }

        candidate = opendir(child);

        if (candidate != NULL) {
            (void)closedir(candidate);

            if (!openai_map_ci_contains(child, "BUILD")) {
                openai_map_walk(child, depth + 1U);
            }
        } else {
            openai_map_add(child);
        }

        if (openai_map_count >= OPENAI_MAP_MAX_FILES) {
            openai_map_truncated = 1;
            break;
        }
    }

    (void)closedir(directory);
}

int openai_project_refresh(const agent_state *state)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        return 0;
    }

    openai_map_count = 0U;
    openai_map_sources = 0U;
    openai_map_headers = 0U;
    openai_map_tests = 0U;
    openai_map_builds = 0U;
    openai_map_others = 0U;
    openai_map_truncated = 0;

    /*
     * DEC C directory enumeration is not guaranteed to expose VMS
     * subdirectory files in a form that can be recursively reopened
     * by the same pathname. Scan the project root first, then scan
     * the conventional OVMS Agent source/test directories directly.
     *
     * Duplicate logical filespecs are collapsed by openai_map_add().
     */
    openai_map_walk(".", 0U);

    {
        DIR *probe;

        probe = opendir("src");
        if (probe != NULL) {
            (void)closedir(probe);
            openai_map_walk("src", 0U);
        }

        probe = opendir("test");
        if (probe != NULL) {
            (void)closedir(probe);
            openai_map_walk("test", 0U);
        }
    }

    openai_map_loaded = 1;

    return 1;
}

static int openai_map_ensure(const agent_state *state)
{
    if (!openai_map_loaded) {
        return openai_project_refresh(state);
    }

    return 1;
}

static int openai_map_append(char *output,
                             size_t output_size,
                             size_t *used,
                             const char *text)
{
    size_t available;
    size_t length;

    if (output == NULL || used == NULL ||
        text == NULL || *used >= output_size) {
        return 0;
    }

    available = output_size - *used - 1U;
    length = strlen(text);

    if (length > available) {
        length = available;
    }

    if (length > 0U) {
        (void)memcpy(output + *used, text, length);
        *used += length;
    }

    output[*used] = '\0';
    return 1;
}

static int openai_map_render(const agent_state *state,
                             int filter,
                             char *output,
                             size_t output_size)
{
    unsigned int index;
    unsigned int shown;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_map_ensure(state)) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent project map\n"
        "----------------------\n"
        "Files:   %u%s\n"
        "Sources: %u\n"
        "Headers: %u\n"
        "Tests:   %u\n"
        "Build:   %u\n"
        "Other:   %u\n\n",
        openai_map_count,
        openai_map_truncated ? "+" : "",
        openai_map_sources,
        openai_map_headers,
        openai_map_tests,
        openai_map_builds,
        openai_map_others
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;
    shown = 0U;

    for (index = 0U; index < openai_map_count; ++index) {
        char line[OPENAI_MAP_PATH + 32U];

        if (filter != 0 &&
            openai_map_files[index].kind != filter) {
            continue;
        }

        written = snprintf(
            line, sizeof(line), "%s\n",
            openai_map_files[index].path
        );

        if (written < 0) {
            return 0;
        }

        if (!openai_map_append(
                output, output_size, &used, line)) {
            return 0;
        }

        ++shown;
    }

    if (filter != 0) {
        char tail[64];

        (void)snprintf(
            tail, sizeof(tail),
            "\nEntries shown: %u\n", shown
        );
        (void)openai_map_append(
            output, output_size, &used, tail
        );
    }

    return 1;
}

int openai_project_map_text(const agent_state *state,
                            char *output,
                            size_t output_size)
{
    return openai_map_render(
        state, 0, output, output_size
    );
}

int openai_project_files_text(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return openai_map_render(
        state, 0, output, output_size
    );
}

int openai_project_src_text(const agent_state *state,
                            char *output,
                            size_t output_size)
{
    return openai_map_render(
        state, OPENAI_MAP_SOURCE, output, output_size
    );
}

int openai_project_tests_text(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return openai_map_render(
        state, OPENAI_MAP_TEST, output, output_size
    );
}

int openai_project_build_text(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return openai_map_render(
        state, OPENAI_MAP_BUILD, output, output_size
    );
}

int openai_project_context(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    char map[OPENAI_MAP_TEXT];
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_project_map_text(
            state, map, sizeof(map))) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "Repository context automatically supplied "
        "to OVMS Agent:\n\n%s",
        map
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_project_compose(const agent_state *state,
                           const char *goal,
                           char *output,
                           size_t output_size)
{
    char map[OPENAI_MAP_TEXT];
    int written;

    if (goal == NULL || *goal == '\0' ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (!openai_project_map_text(
            state, map, sizeof(map))) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "REPOSITORY MAP\n"
        "--------------\n"
        "%s\n"
        "MODEL TASK CONTEXT\n"
        "------------------\n"
        "%s",
        map,
        goal
    );

    return written >= 0 &&
           (size_t)written < output_size;
}

void openai_show_project_map(const agent_state *state,
                             int filter)
{
    char output[OPENAI_MAP_TEXT];
    int ok;

    if (filter == OPENAI_MAP_SOURCE) {
        ok = openai_project_src_text(
            state, output, sizeof(output));
    } else if (filter == OPENAI_MAP_TEST) {
        ok = openai_project_tests_text(
            state, output, sizeof(output));
    } else if (filter == OPENAI_MAP_BUILD) {
        ok = openai_project_build_text(
            state, output, sizeof(output));
    } else {
        ok = openai_project_map_text(
            state, output, sizeof(output));
    }

    if (!ok) {
        (void)puts("Unable to build project map.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_project_ctx(const agent_state *state)
{
    char output[OPENAI_MAP_TEXT + 256U];

    if (!openai_project_context(
            state, output, sizeof(output))) {
        (void)puts("Unable to show repository context.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_project_refresh_cmd(const agent_state *state)
{
    if (!openai_project_refresh(state)) {
        (void)puts("Project map refresh failed.");
        return;
    }

    (void)printf(
        "Project map refreshed: %u files%s.\n",
        openai_map_count,
        openai_map_truncated ? " (truncated)" : ""
    );
}
