#include "llm_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#define LLM_MAP_MAX_FILES 192U
#define LLM_MAP_PATH 256U
#define LLM_MAP_TEXT 12288U
#define LLM_MAP_DEPTH 4U

#define LLM_MAP_SOURCE 1
#define LLM_MAP_HEADER 2
#define LLM_MAP_TEST 3
#define LLM_MAP_BUILD 4
#define LLM_MAP_OTHER 5

typedef struct llm_map_entry {
    char path[LLM_MAP_PATH];
    int kind;
} llm_map_entry;

static llm_map_entry llm_map_files[LLM_MAP_MAX_FILES];
static unsigned int llm_map_count = 0U;
static unsigned int llm_map_sources = 0U;
static unsigned int llm_map_headers = 0U;
static unsigned int llm_map_tests = 0U;
static unsigned int llm_map_builds = 0U;
static unsigned int llm_map_others = 0U;
static int llm_map_loaded = 0;
static int llm_map_truncated = 0;

static int llm_map_ci_equal(const char *left,
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

static int llm_map_ci_contains(const char *text,
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

static int llm_map_old_version(const char *name)
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

static int llm_map_hide(const char *name)
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

static void llm_map_base_path(const char *path,
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

static int llm_map_kind(const char *path)
{
    const char *dot;

    if (path == NULL) {
        return LLM_MAP_OTHER;
    }

    if (llm_map_ci_contains(path, "TEST") ||
        llm_map_ci_contains(path, "_TEST.")) {
        return LLM_MAP_TEST;
    }

    if (llm_map_ci_contains(path, "BUILD.COM") ||
        llm_map_ci_contains(path, ".OPT") ||
        llm_map_ci_contains(path, "DESCRIP.MMS") ||
        llm_map_ci_contains(path, "MAKEFILE")) {
        return LLM_MAP_BUILD;
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

            if (llm_map_ci_equal(extension, ".C") ||
                llm_map_ci_equal(extension, ".COB") ||
                llm_map_ci_equal(extension, ".COBOL") ||
                llm_map_ci_equal(extension, ".CBL")) {
                return LLM_MAP_SOURCE;
            }

            if (llm_map_ci_equal(extension, ".H") ||
                llm_map_ci_equal(extension, ".CPY")) {
                return LLM_MAP_HEADER;
            }
        }
    }

    return LLM_MAP_OTHER;
}

static int llm_map_join(const char *parent,
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

static void llm_map_add(const char *path)
{
    char logical_path[LLM_MAP_PATH];
    unsigned int index;
    int kind;

    llm_map_base_path(
        path, logical_path, sizeof(logical_path)
    );

    if (logical_path[0] == '\0') {
        return;
    }

    for (index = 0U; index < llm_map_count; ++index) {
        if (llm_map_ci_equal(
                llm_map_files[index].path,
                logical_path)) {
            return;
        }
    }

    if (llm_map_count >= LLM_MAP_MAX_FILES) {
        llm_map_truncated = 1;
        return;
    }

    kind = llm_map_kind(logical_path);

    (void)strncpy(
        llm_map_files[llm_map_count].path,
        logical_path,
        sizeof(llm_map_files[llm_map_count].path) - 1U
    );
    llm_map_files[llm_map_count].path[
        sizeof(llm_map_files[llm_map_count].path) - 1U
    ] = '\0';
    llm_map_files[llm_map_count].kind = kind;
    ++llm_map_count;

    if (kind == LLM_MAP_SOURCE) {
        ++llm_map_sources;
    } else if (kind == LLM_MAP_HEADER) {
        ++llm_map_headers;
    } else if (kind == LLM_MAP_TEST) {
        ++llm_map_tests;
    } else if (kind == LLM_MAP_BUILD) {
        ++llm_map_builds;
    } else {
        ++llm_map_others;
    }
}

static void llm_map_walk(const char *path,
                            unsigned int depth)
{
    DIR *directory;
    struct dirent *entry;

    if (depth > LLM_MAP_DEPTH ||
        llm_map_count >= LLM_MAP_MAX_FILES) {
        return;
    }

    directory = opendir(path);

    if (directory == NULL) {
        return;
    }

    while ((entry = readdir(directory)) != NULL) {
        char child[LLM_MAP_PATH];
        DIR *candidate;

        if (llm_map_hide(entry->d_name)) {
            continue;
        }

        if (!llm_map_join(
                path, entry->d_name,
                child, sizeof(child))) {
            llm_map_truncated = 1;
            continue;
        }

        candidate = opendir(child);

        if (candidate != NULL) {
            (void)closedir(candidate);

            if (!llm_map_ci_contains(child, "BUILD")) {
                llm_map_walk(child, depth + 1U);
            }
        } else {
            llm_map_add(child);
        }

        if (llm_map_count >= LLM_MAP_MAX_FILES) {
            llm_map_truncated = 1;
            break;
        }
    }

    (void)closedir(directory);
}

static void llm_map_build_root(const char *path)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir(path);
    if (directory == NULL) {
        return;
    }

    while ((entry = readdir(directory)) != NULL) {
        char child[LLM_MAP_PATH];
        DIR *candidate;

        if (llm_map_hide(entry->d_name)) {
            continue;
        }

        if (!llm_map_join(
                path, entry->d_name,
                child, sizeof(child))) {
            llm_map_truncated = 1;
            continue;
        }

        candidate = opendir(child);
        if (candidate != NULL) {
            (void)closedir(candidate);
            continue;
        }

        if (llm_map_kind(child) == LLM_MAP_BUILD) {
            llm_map_add(child);
        }
    }

    (void)closedir(directory);
}

int llm_project_refresh_base(const agent_state *state)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        return 0;
    }

    llm_map_count = 0U;
    llm_map_sources = 0U;
    llm_map_headers = 0U;
    llm_map_tests = 0U;
    llm_map_builds = 0U;
    llm_map_others = 0U;
    llm_map_truncated = 0;

    /*
     * Seed authoritative root build definitions before bounded
     * repository traversal, then prioritize the conventional live
     * source/test areas. The final root walk fills remaining capacity.
     * Duplicate logical filespecs are collapsed by llm_map_add().
     */
    llm_map_build_root(".");

    {
        DIR *probe;

        probe = opendir("src");
        if (probe != NULL) {
            (void)closedir(probe);
            llm_map_walk("src", 0U);
        }

        probe = opendir("test");
        if (probe != NULL) {
            (void)closedir(probe);
            llm_map_walk("test", 0U);
        }
    }

    llm_map_walk(".", 0U);

    llm_map_loaded = 1;

    return 1;
}

static int llm_map_ensure(const agent_state *state)
{
    if (!llm_map_loaded) {
        return llm_project_refresh_base(state);
    }

    return 1;
}

static int llm_map_append(char *output,
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

static int llm_map_render(const agent_state *state,
                             int filter,
                             char *output,
                             size_t output_size)
{
    unsigned int index;
    unsigned int shown;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U ||
        !llm_map_ensure(state)) {
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
        llm_map_count,
        llm_map_truncated ? "+" : "",
        llm_map_sources,
        llm_map_headers,
        llm_map_tests,
        llm_map_builds,
        llm_map_others
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;
    shown = 0U;

    for (index = 0U; index < llm_map_count; ++index) {
        char line[LLM_MAP_PATH + 32U];

        if (filter != 0 &&
            llm_map_files[index].kind != filter) {
            continue;
        }

        written = snprintf(
            line, sizeof(line), "%s\n",
            llm_map_files[index].path
        );

        if (written < 0) {
            return 0;
        }

        if (!llm_map_append(
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
        (void)llm_map_append(
            output, output_size, &used, tail
        );
    }

    return 1;
}

int llm_project_map_text_base(const agent_state *state,
                            char *output,
                            size_t output_size)
{
    return llm_map_render(
        state, 0, output, output_size
    );
}

int llm_project_files_text_base(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return llm_map_render(
        state, 0, output, output_size
    );
}

int llm_project_src_text_base(const agent_state *state,
                            char *output,
                            size_t output_size)
{
    return llm_map_render(
        state, LLM_MAP_SOURCE, output, output_size
    );
}

int llm_project_tests_text_base(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return llm_map_render(
        state, LLM_MAP_TEST, output, output_size
    );
}

int llm_project_build_text_base(const agent_state *state,
                              char *output,
                              size_t output_size)
{
    return llm_map_render(
        state, LLM_MAP_BUILD, output, output_size
    );
}

int llm_project_context_base(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    char map[LLM_MAP_TEXT];
    int written;

    if (output == NULL || output_size == 0U ||
        !llm_project_map_text_base(
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

int llm_project_compose_base(const agent_state *state,
                           const char *goal,
                           char *output,
                           size_t output_size)
{
    char map[LLM_MAP_TEXT];
    int written;

    if (goal == NULL || *goal == '\0' ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    if (!llm_project_map_text_base(
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

void llm_show_project_map_base(const agent_state *state,
                             int filter)
{
    char output[LLM_MAP_TEXT];
    int ok;

    if (filter == LLM_MAP_SOURCE) {
        ok = llm_project_src_text_base(
            state, output, sizeof(output));
    } else if (filter == LLM_MAP_TEST) {
        ok = llm_project_tests_text_base(
            state, output, sizeof(output));
    } else if (filter == LLM_MAP_BUILD) {
        ok = llm_project_build_text_base(
            state, output, sizeof(output));
    } else {
        ok = llm_project_map_text_base(
            state, output, sizeof(output));
    }

    if (!ok) {
        (void)puts("Unable to build project map.");
        return;
    }

    (void)fputs(output, stdout);
}

void llm_show_project_ctx_base(const agent_state *state)
{
    char output[LLM_MAP_TEXT + 256U];

    if (!llm_project_context_base(
            state, output, sizeof(output))) {
        (void)puts("Unable to show repository context.");
        return;
    }

    (void)fputs(output, stdout);
}

void llm_project_refresh_cmd_base(const agent_state *state)
{
    if (!llm_project_refresh_base(state)) {
        (void)puts("Project map refresh failed.");
        return;
    }

    (void)printf(
        "Project map refreshed: %u files%s.\n",
        llm_map_count,
        llm_map_truncated ? " (truncated)" : ""
    );
}
