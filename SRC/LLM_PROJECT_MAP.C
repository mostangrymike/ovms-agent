/*
 * M267 provider-neutral PROJECT_MAP transition wrapper.
 *
 * Keep the mature bounded implementation in LLM_PROJECT_MAP_CORE.INC, but
 * route public entry points through a priority-stable recovery layer.  The
 * core still owns the fixed 192-entry VAX budget.  Once that budget fills,
 * a second bounded-depth pass may replace low-value OTHER entries with later
 * source/header/test/build entries so useful repository context does not
 * depend on readdir() order.
 */
#include "LLM_PROJECT_MAP_CORE.INC"

static void llm_map_dec_kind(int kind)
{
    if (kind == LLM_MAP_SOURCE && llm_map_sources > 0U) {
        --llm_map_sources;
    } else if (kind == LLM_MAP_HEADER && llm_map_headers > 0U) {
        --llm_map_headers;
    } else if (kind == LLM_MAP_TEST && llm_map_tests > 0U) {
        --llm_map_tests;
    } else if (kind == LLM_MAP_BUILD && llm_map_builds > 0U) {
        --llm_map_builds;
    } else if (kind == LLM_MAP_OTHER && llm_map_others > 0U) {
        --llm_map_others;
    }
}

static void llm_map_inc_kind(int kind)
{
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

static void llm_map_priority_add(const char *path)
{
    char logical_path[LLM_MAP_PATH];
    unsigned int index;
    unsigned int replace_index;
    int kind;

    llm_map_base_path(path, logical_path, sizeof(logical_path));
    if (logical_path[0] == '\0') return;

    for (index = 0U; index < llm_map_count; ++index) {
        if (llm_map_ci_equal(llm_map_files[index].path,
                                logical_path)) {
            return;
        }
    }

    kind = llm_map_kind(logical_path);
    if (llm_map_count < LLM_MAP_MAX_FILES) {
        llm_map_add(logical_path);
        return;
    }

    llm_map_truncated = 1;
    if (kind == LLM_MAP_OTHER) return;

    replace_index = LLM_MAP_MAX_FILES;
    for (index = 0U; index < llm_map_count; ++index) {
        if (llm_map_files[index].kind == LLM_MAP_OTHER) {
            replace_index = index;
            break;
        }
    }

    if (replace_index >= llm_map_count) return;

    llm_map_dec_kind(llm_map_files[replace_index].kind);
    (void)strncpy(llm_map_files[replace_index].path,
                  logical_path,
                  sizeof(llm_map_files[replace_index].path) - 1U);
    llm_map_files[replace_index].path[
        sizeof(llm_map_files[replace_index].path) - 1U
    ] = '\0';
    llm_map_files[replace_index].kind = kind;
    llm_map_inc_kind(kind);
}

static void llm_map_priority_walk(const char *path,
                                  unsigned int depth)
{
    DIR *directory;
    struct dirent *entry;

    if (depth > LLM_MAP_DEPTH) return;

    directory = opendir(path);
    if (directory == NULL) return;

    while ((entry = readdir(directory)) != NULL) {
        char child[LLM_MAP_PATH];
        DIR *candidate;

        if (llm_map_hide(entry->d_name)) continue;

        if (!llm_map_join(path, entry->d_name,
                             child, sizeof(child))) {
            llm_map_truncated = 1;
            continue;
        }

        candidate = opendir(child);
        if (candidate != NULL) {
            (void)closedir(candidate);
            if (!llm_map_ci_contains(child, "BUILD")) {
                llm_map_priority_walk(child, depth + 1U);
            }
        } else {
            llm_map_priority_add(child);
        }
    }

    (void)closedir(directory);
}

int llm_project_refresh(const agent_state *state)
{
    DIR *probe;

    if (!llm_project_refresh_base(state)) return 0;

    probe = opendir("src");
    if (probe != NULL) {
        (void)closedir(probe);
        llm_map_priority_walk("src", 0U);
    }

    probe = opendir("test");
    if (probe != NULL) {
        (void)closedir(probe);
        llm_map_priority_walk("test", 0U);
    }

    return 1;
}

static int llm_map_ensure_priority(const agent_state *state)
{
    if (!llm_map_loaded) return llm_project_refresh(state);
    return 1;
}

int llm_project_map_text(const agent_state *state,
                         char *output,
                         size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_map_text_base(state, output, output_size);
}

int llm_project_files_text(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_files_text_base(state, output, output_size);
}

int llm_project_src_text(const agent_state *state,
                         char *output,
                         size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_src_text_base(state, output, output_size);
}

int llm_project_tests_text(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_tests_text_base(state, output, output_size);
}

int llm_project_build_text(const agent_state *state,
                           char *output,
                           size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_build_text_base(state, output, output_size);
}

int llm_project_context(const agent_state *state,
                        char *output,
                        size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_context_base(state, output, output_size);
}

int llm_project_compose(const agent_state *state,
                        const char *goal,
                        char *output,
                        size_t output_size)
{
    if (!llm_map_ensure_priority(state)) return 0;
    return llm_project_compose_base(state, goal, output, output_size);
}

void llm_show_project_map(const agent_state *state,
                          int filter)
{
    if (!llm_map_ensure_priority(state)) {
        (void)puts("Unable to build project map.");
        return;
    }
    llm_show_project_map_base(state, filter);
}

void llm_show_project_ctx(const agent_state *state)
{
    if (!llm_map_ensure_priority(state)) {
        (void)puts("Unable to show repository context.");
        return;
    }
    llm_show_project_ctx_base(state);
}

void llm_project_refresh_cmd(const agent_state *state)
{
    if (!llm_project_refresh(state)) {
        (void)puts("Project map refresh failed.");
        return;
    }

    (void)printf("Project map refreshed: %u files%s.\n",
                 llm_map_count,
                 llm_map_truncated ? " (truncated)" : "");
}
