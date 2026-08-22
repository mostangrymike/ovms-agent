/*
 * M257 Phase 6 scoped project-instruction wrapper.
 *
 * Preserve the proven M232 root-instruction implementation and add bounded,
 * project-relative directory scopes discovered from paths in the current goal.
 * Scoped files are applied from broad to specific; later scopes override
 * conflicting earlier guidance.
 */
#include "LLM_INSTRUCTIONS.C"

#define M257_SCOPE_MAX 8U
#define M257_SCOPE_PATH 256U
#define M257_SCOPE_DATA 4096U
#define M257_SCOPE_FILE "OVMS_AGENT_INSTRUCTIONS.TXT"

typedef struct m257_scope_rec {
    char directory[M257_SCOPE_PATH];
    unsigned int depth;
} m257_scope_rec;

static char m257_active_files[2048];
static char m257_active_data[M257_SCOPE_DATA];
static unsigned int m257_active_count = 0U;
static int m257_active_truncated = 0;

static int m257_path_char(int ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-' || ch == '.' || ch == '/';
}

static int m257_dir_safe(const char *directory)
{
    const char *position;

    if (directory == NULL || *directory == '\0' ||
        *directory == '/' || strstr(directory, "..") != NULL ||
        strchr(directory, ':') != NULL ||
        strchr(directory, '\\') != NULL) {
        return 0;
    }

    position = directory;
    while (*position != '\0') {
        if (!m257_path_char((unsigned char)*position)) return 0;
        ++position;
    }
    return 1;
}

static unsigned int m257_depth(const char *directory)
{
    unsigned int depth;
    const char *position;

    depth = 1U;
    position = directory;
    while (*position != '\0') {
        if (*position == '/') ++depth;
        ++position;
    }
    return depth;
}

static int m257_scope_exists(const m257_scope_rec *scopes,
                             unsigned int count,
                             const char *directory)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(scopes[index].directory, directory) == 0) return 1;
    }
    return 0;
}

static void m257_add_prefixes(m257_scope_rec *scopes,
                              unsigned int *count,
                              const char *path)
{
    char directory[M257_SCOPE_PATH];
    const char *slash;
    const char *position;
    size_t length;

    if (scopes == NULL || count == NULL || path == NULL) return;
    slash = strrchr(path, '/');
    if (slash == NULL || slash == path) return;

    length = (size_t)(slash - path);
    if (length == 0U || length >= sizeof(directory)) return;
    (void)memcpy(directory, path, length);
    directory[length] = '\0';
    if (!m257_dir_safe(directory)) return;

    position = directory;
    for (;;) {
        const char *next;
        size_t prefix_length;
        char prefix[M257_SCOPE_PATH];

        next = strchr(position, '/');
        if (next != NULL) {
            prefix_length = (size_t)(next - directory);
        } else {
            prefix_length = strlen(directory);
        }

        if (prefix_length > 0U && prefix_length < sizeof(prefix)) {
            (void)memcpy(prefix, directory, prefix_length);
            prefix[prefix_length] = '\0';
            if (!m257_scope_exists(scopes, *count, prefix) &&
                *count < M257_SCOPE_MAX) {
                (void)strcpy(scopes[*count].directory, prefix);
                scopes[*count].depth = m257_depth(prefix);
                ++(*count);
            }
        }

        if (next == NULL) break;
        position = next + 1;
    }
}

static unsigned int m257_collect_scopes(const char *goal,
                                        m257_scope_rec *scopes)
{
    const char *position;
    unsigned int count;

    if (goal == NULL || scopes == NULL) return 0U;
    position = goal;
    count = 0U;

    while (*position != '\0') {
        const char *start;
        size_t length;
        char token[M257_SCOPE_PATH];

        while (*position != '\0' &&
               !m257_path_char((unsigned char)*position)) ++position;
        start = position;
        while (*position != '\0' &&
               m257_path_char((unsigned char)*position)) ++position;
        length = (size_t)(position - start);

        if (length > 0U && length < sizeof(token)) {
            (void)memcpy(token, start, length);
            token[length] = '\0';
            if (strchr(token, '/') != NULL) {
                m257_add_prefixes(scopes, &count, token);
            }
        }
    }

    return count;
}

static void m257_sort_scopes(m257_scope_rec *scopes,
                             unsigned int count)
{
    unsigned int i;
    unsigned int j;

    for (i = 0U; i < count; ++i) {
        for (j = i + 1U; j < count; ++j) {
            if (scopes[j].depth < scopes[i].depth) {
                m257_scope_rec temp;
                temp = scopes[i];
                scopes[i] = scopes[j];
                scopes[j] = temp;
            }
        }
    }
}

static int m257_scope_path(const agent_state *state,
                           const char *directory,
                           char *output,
                           size_t output_size)
{
    const char *root;
    int written;

    if (directory == NULL || output == NULL || output_size == 0U) return 0;
    root = state != NULL ? state->project_root : NULL;

    if (root == NULL || *root == '\0' || strcmp(root, ".") == 0) {
        written = snprintf(output, output_size, "%s/%s",
                           directory, M257_SCOPE_FILE);
    } else {
        written = snprintf(output, output_size, "%s/%s/%s",
                           root, directory, M257_SCOPE_FILE);
    }

    return written >= 0 && (size_t)written < output_size;
}

static int m257_append(char *output,
                       size_t output_size,
                       const char *text)
{
    size_t used;
    size_t length;

    if (output == NULL || text == NULL) return 0;
    used = strlen(output);
    length = strlen(text);
    if (used + length + 1U > output_size) return 0;
    (void)memcpy(output + used, text, length + 1U);
    return 1;
}

static int m257_load_scopes(const agent_state *state,
                            const char *goal)
{
    m257_scope_rec scopes[M257_SCOPE_MAX];
    unsigned int count;
    unsigned int index;

    m257_active_files[0] = '\0';
    m257_active_data[0] = '\0';
    m257_active_count = 0U;
    m257_active_truncated = 0;

    count = m257_collect_scopes(goal, scopes);
    m257_sort_scopes(scopes, count);

    for (index = 0U; index < count; ++index) {
        char path[M257_SCOPE_PATH * 2U];
        char data[M257_SCOPE_DATA];
        char header[512];
        FILE *file;
        size_t used;
        int ch;
        int written;

        if (!m257_scope_path(state, scopes[index].directory,
                             path, sizeof(path))) continue;
        file = fopen(path, "r");
        if (file == NULL) continue;

        used = 0U;
        while ((ch = fgetc(file)) != EOF) {
            if (ch == '\r') continue;
            if (used + 1U < sizeof(data)) {
                data[used++] = (char)ch;
            } else {
                m257_active_truncated = 1;
            }
        }
        if (ferror(file)) {
            (void)fclose(file);
            return 0;
        }
        (void)fclose(file);
        while (used > 0U &&
               (data[used - 1U] == '\n' || data[used - 1U] == ' ' ||
                data[used - 1U] == '\t')) --used;
        data[used] = '\0';
        if (used == 0U) continue;

        written = snprintf(header, sizeof(header),
            "\n\nDIRECTORY-SCOPED PROJECT INSTRUCTIONS [%s]\n"
            "------------------------------------------------\n",
            scopes[index].directory);
        if (written < 0 || (size_t)written >= sizeof(header) ||
            !m257_append(m257_active_data, sizeof(m257_active_data), header) ||
            !m257_append(m257_active_data, sizeof(m257_active_data), data) ||
            !m257_append(m257_active_files, sizeof(m257_active_files), path) ||
            !m257_append(m257_active_files, sizeof(m257_active_files), "\n")) {
            m257_active_truncated = 1;
            break;
        }
        ++m257_active_count;
    }

    return 1;
}

int llm_instr_reload(const agent_state *state)
{
    m257_active_files[0] = '\0';
    m257_active_data[0] = '\0';
    m257_active_count = 0U;
    m257_active_truncated = 0;
    return llm_instr_reload_base(state);
}

int llm_instr_compose(const agent_state *state,
                      const char *goal,
                      char *output,
                      size_t output_size)
{
    char base[LLM_INSTR_PROMPT];
    int written;

    if (!llm_instr_compose_base(state, goal, base, sizeof(base)) ||
        !m257_load_scopes(state, goal)) return 0;

    if (m257_active_count == 0U) {
        written = snprintf(output, output_size, "%s", base);
    } else {
        written = snprintf(output, output_size,
            "%s\n\n"
            "INSTRUCTION PRECEDENCE\n"
            "----------------------\n"
            "Directory-scoped instructions are ordered from broad to specific; "
            "later, deeper scopes override conflicting earlier guidance.%s",
            base, m257_active_data);
    }

    return written >= 0 && (size_t)written < output_size;
}

int llm_instr_status_text(const agent_state *state,
                          char *output,
                          size_t output_size)
{
    char base[2048];
    int written;

    if (!llm_instr_status_text_base(state, base, sizeof(base))) return 0;
    written = snprintf(output, output_size,
        "%sScoped active files: %u\n"
        "Scoped truncated:   %s\n%s",
        base, m257_active_count,
        m257_active_truncated ? "yes" : "no",
        m257_active_count > 0U ? m257_active_files : "");
    return written >= 0 && (size_t)written < output_size;
}

int llm_instr_show_text(const agent_state *state,
                        char *output,
                        size_t output_size)
{
    char base[LLM_INSTR_MAX + 256U];
    int written;

    if (!llm_instr_show_text_base(state, base, sizeof(base))) return 0;
    written = snprintf(output, output_size, "%s%s",
                       base, m257_active_data);
    return written >= 0 && (size_t)written < output_size;
}

void llm_show_instr_status(const agent_state *state)
{
    char output[4096];
    if (!llm_instr_status_text(state, output, sizeof(output))) {
        (void)puts("Unable to show project instruction status.");
        return;
    }
    (void)fputs(output, stdout);
}

void llm_show_instr(const agent_state *state)
{
    char output[LLM_INSTR_MAX + M257_SCOPE_DATA + 512U];
    if (!llm_instr_show_text(state, output, sizeof(output))) {
        (void)puts("Unable to show project instructions.");
        return;
    }
    (void)fputs(output, stdout);
}

void llm_instr_reload_cmd(const agent_state *state)
{
    if (!llm_instr_reload(state)) {
        (void)puts("Project instruction reload failed.");
        return;
    }
    llm_show_instr_status(state);
}

void llm_test_instr_path(const char *path)
{
    m257_active_files[0] = '\0';
    m257_active_data[0] = '\0';
    m257_active_count = 0U;
    m257_active_truncated = 0;
    llm_test_instr_path_base(path);
}
