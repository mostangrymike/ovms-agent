/* M279 command-side RMS/version-safe RENAME apply and verify. */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define M279_RENAME_MAX_FILES 32U
#define M279_RENAME_PATH_MAX 512U
#define M279_RENAME_TEXT_MAX 1048576U
#define M279_RENAME_TEMP "OVMS_RENAME_M279.TMP"

typedef struct m279_rename_file {
    char path[M279_RENAME_PATH_MAX];
    char original_spec[M279_RENAME_PATH_MAX];
    char new_spec[M279_RENAME_PATH_MAX];
    unsigned int replacements;
    int written;
} m279_rename_file;

typedef struct m279_rename_txn {
    m279_rename_file files[M279_RENAME_MAX_FILES];
    unsigned int file_count;
    unsigned int total_replacements;
} m279_rename_txn;

extern int fgetname(FILE *, char *);

static int m279_rename_id_char(int ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static int m279_rename_name_valid(const char *name)
{
    const unsigned char *p;

    if (name == NULL || *name == '\0') {
        return 0;
    }

    p = (const unsigned char *)name;
    if (!(isalpha(*p) || *p == '_')) {
        return 0;
    }

    ++p;
    while (*p != '\0') {
        if (!(isalnum(*p) || *p == '_')) {
            return 0;
        }
        ++p;
    }

    return 1;
}

static int m279_rename_source_name(const char *name)
{
    size_t length;
    int final;

    if (name == NULL) {
        return 0;
    }

    length = strlen(name);
    if (length < 2U || name[length - 2U] != '.') {
        return 0;
    }

    final = tolower((unsigned char)name[length - 1U]);
    return final == 'c' || final == 'h';
}

static char *m279_rename_read(const char *path)
{
    FILE *file;
    char chunk[2048];
    char *text;
    size_t used;
    size_t capacity;

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    text = NULL;
    used = 0U;
    capacity = 0U;

    while (fgets(chunk, sizeof(chunk), file) != NULL) {
        size_t length;
        size_t needed;
        char *grown;

        length = strlen(chunk);
        needed = used + length + 1U;
        if (needed > M279_RENAME_TEXT_MAX) {
            free(text);
            (void)fclose(file);
            return NULL;
        }

        if (needed > capacity) {
            size_t next;

            next = capacity == 0U ? 4096U : capacity;
            while (next < needed) {
                if (next > M279_RENAME_TEXT_MAX / 2U) {
                    next = M279_RENAME_TEXT_MAX;
                    break;
                }
                next *= 2U;
            }

            grown = (char *)realloc(text, next);
            if (grown == NULL) {
                free(text);
                (void)fclose(file);
                return NULL;
            }
            text = grown;
            capacity = next;
        }

        (void)memcpy(text + used, chunk, length);
        used += length;
        text[used] = '\0';
    }

    if (ferror(file)) {
        free(text);
        (void)fclose(file);
        return NULL;
    }
    (void)fclose(file);

    if (text == NULL) {
        text = (char *)malloc(1U);
        if (text != NULL) {
            text[0] = '\0';
        }
    }

    return text;
}

static unsigned int m279_rename_count(const char *text,
                                      const char *name)
{
    const char *position;
    size_t length;
    unsigned int count;

    if (text == NULL || name == NULL || *name == '\0') {
        return 0U;
    }

    length = strlen(name);
    position = text;
    count = 0U;

    while ((position = strstr(position, name)) != NULL) {
        int before_ok;
        int after_ok;

        before_ok = position == text ||
            !m279_rename_id_char((unsigned char)position[-1]);
        after_ok = !m279_rename_id_char(
            (unsigned char)position[length]);

        if (before_ok && after_ok) {
            ++count;
        }
        position += length;
    }

    return count;
}

static char *m279_rename_replace(const char *text,
                                 const char *old_name,
                                 const char *new_name,
                                 unsigned int expected)
{
    const char *position;
    const char *match;
    char *output;
    char *destination;
    size_t old_length;
    size_t new_length;
    size_t input_length;
    size_t output_length;
    unsigned int count;

    old_length = strlen(old_name);
    new_length = strlen(new_name);
    input_length = strlen(text);

    if (new_length >= old_length) {
        size_t growth;

        growth = (new_length - old_length) * (size_t)expected;
        if (input_length + growth + 1U > M279_RENAME_TEXT_MAX) {
            return NULL;
        }
        output_length = input_length + growth;
    } else {
        output_length = input_length -
            (old_length - new_length) * (size_t)expected;
    }

    output = (char *)malloc(output_length + 1U);
    if (output == NULL) {
        return NULL;
    }

    position = text;
    destination = output;
    count = 0U;

    while ((match = strstr(position, old_name)) != NULL) {
        int before_ok;
        int after_ok;
        size_t prefix;

        before_ok = match == text ||
            !m279_rename_id_char((unsigned char)match[-1]);
        after_ok = !m279_rename_id_char(
            (unsigned char)match[old_length]);

        if (!before_ok || !after_ok) {
            prefix = (size_t)(match - position) + 1U;
            (void)memcpy(destination, position, prefix);
            destination += prefix;
            position += prefix;
            continue;
        }

        prefix = (size_t)(match - position);
        (void)memcpy(destination, position, prefix);
        destination += prefix;
        (void)memcpy(destination, new_name, new_length);
        destination += new_length;
        position = match + old_length;
        ++count;
    }

    (void)strcpy(destination, position);
    if (count != expected) {
        free(output);
        return NULL;
    }

    return output;
}

static int m279_rename_make_path(const char *directory_name,
                                 const char *name,
                                 char *path,
                                 size_t path_size)
{
    int result;

    if (strcmp(directory_name, ".") == 0) {
        result = snprintf(path, path_size, "%s", name);
    } else {
        result = snprintf(path, path_size, "%s/%s",
                          directory_name, name);
    }

    return result >= 0 && (size_t)result < path_size;
}

static int m279_rename_collect(const char *old_name,
                               const char *new_name,
                               m279_rename_txn *transaction)
{
    const char *directory_name;
    DIR *directory;
    struct dirent *entry;

    (void)memset(transaction, 0, sizeof(*transaction));

    directory_name = "SRC";
    directory = opendir(directory_name);
    if (directory == NULL) {
        directory_name = ".";
        directory = opendir(directory_name);
    }
    if (directory == NULL) {
        return 0;
    }

    while ((entry = readdir(directory)) != NULL) {
        char path[M279_RENAME_PATH_MAX];
        char *text;
        unsigned int old_count;
        unsigned int new_count;

        if (!m279_rename_source_name(entry->d_name)) {
            continue;
        }
        if (!m279_rename_make_path(directory_name, entry->d_name,
                                   path, sizeof(path))) {
            (void)closedir(directory);
            return 0;
        }

        text = m279_rename_read(path);
        if (text == NULL) {
            (void)closedir(directory);
            return 0;
        }

        new_count = m279_rename_count(text, new_name);
        old_count = m279_rename_count(text, old_name);
        free(text);

        if (new_count != 0U) {
            (void)printf("Rename blocked: %s already exists in project source.\n",
                         new_name);
            (void)closedir(directory);
            return -1;
        }

        if (old_count == 0U) {
            continue;
        }

        if (transaction->file_count >= M279_RENAME_MAX_FILES ||
            strlen(path) >= M279_RENAME_PATH_MAX) {
            (void)closedir(directory);
            return 0;
        }

        (void)strcpy(transaction->files[transaction->file_count].path,
                     path);
        transaction->files[transaction->file_count].replacements =
            old_count;
        transaction->total_replacements += old_count;
        ++transaction->file_count;
    }

    (void)closedir(directory);
    return 1;
}

static int m279_rename_confirm(const m279_rename_txn *transaction,
                               const char *old_name,
                               const char *new_name)
{
    char answer[32];
    unsigned int index;

    (void)printf("Rename %s -> %s\n", old_name, new_name);
    (void)puts("========================================");
    (void)printf("Files affected:     %u\n", transaction->file_count);
    (void)printf("Total replacements: %u\n",
                 transaction->total_replacements);

    for (index = 0U; index < transaction->file_count; ++index) {
        (void)printf("  %-32s %u replacement%s\n",
            transaction->files[index].path,
            transaction->files[index].replacements,
            transaction->files[index].replacements == 1U ? "" : "s");
    }

    (void)printf("Apply rename [y/N]? ");
    (void)fflush(stdout);
    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 0;
    }

    return answer[0] == 'y' || answer[0] == 'Y';
}

static int m279_rename_get_spec(const char *path,
                                char *spec,
                                size_t spec_size)
{
    FILE *file;
    int status;

    if (spec_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    status = fgetname(file, spec);
    (void)fclose(file);
    spec[spec_size - 1U] = '\0';
    return status != 0;
}

static int m279_rename_vms_spec(const char *project_path,
                                char *filespec,
                                size_t filespec_size)
{
    const char *slash;
    int result;

    slash = strchr(project_path, '/');
    if (slash == NULL) {
        result = snprintf(filespec, filespec_size, "%s", project_path);
    } else {
        size_t directory_length;

        if (slash == project_path || slash[1] == '\0') {
            return 0;
        }
        directory_length = (size_t)(slash - project_path);
        result = snprintf(filespec, filespec_size, "[.%.*s]%s",
            (int)directory_length, project_path, slash + 1);
    }

    return result >= 0 && (size_t)result < filespec_size;
}

static void m279_rename_remove_temp(void)
{
    while (remove(M279_RENAME_TEMP) == 0) {
        /* Remove every temporary OpenVMS version. */
    }
}

static int m279_rename_copy_text(const char *path,
                                 const char *text,
                                 char *new_spec,
                                 size_t new_spec_size)
{
    FILE *file;
    char filespec[M279_RENAME_PATH_MAX];
    char command[M279_RENAME_PATH_MAX + 96U];
    int status;

    m279_rename_remove_temp();
    file = fopen(M279_RENAME_TEMP, "w");
    if (file == NULL) {
        return 0;
    }
    if (fputs(text, file) == EOF || fclose(file) != 0) {
        (void)remove(M279_RENAME_TEMP);
        return 0;
    }

    if (!m279_rename_vms_spec(path, filespec, sizeof(filespec))) {
        m279_rename_remove_temp();
        return 0;
    }

    if (snprintf(command, sizeof(command),
                 "COPY/NOLOG %s %s;", M279_RENAME_TEMP, filespec) < 0) {
        m279_rename_remove_temp();
        return 0;
    }

    status = system(command);
    m279_rename_remove_temp();
    if (status != 0 && (status & 1) == 0) {
        return 0;
    }

    return m279_rename_get_spec(path, new_spec, new_spec_size);
}

static int m279_rename_rollback(m279_rename_txn *transaction)
{
    unsigned int index;
    int success;

    success = 1;
    index = transaction->file_count;
    while (index > 0U) {
        m279_rename_file *file;

        --index;
        file = &transaction->files[index];
        if (!file->written) {
            continue;
        }

        if (file->new_spec[0] == '\0' ||
            strcmp(file->new_spec, file->original_spec) == 0 ||
            remove(file->new_spec) != 0) {
            success = 0;
        } else {
            file->written = 0;
        }
    }

    return success;
}

static int m279_rename_apply_txn(m279_rename_txn *transaction,
                                 const char *old_name,
                                 const char *new_name)
{
    unsigned int index;

    for (index = 0U; index < transaction->file_count; ++index) {
        m279_rename_file *file;
        char *input;
        char *output;

        file = &transaction->files[index];
        if (!m279_rename_get_spec(file->path, file->original_spec,
                                  sizeof(file->original_spec))) {
            (void)m279_rename_rollback(transaction);
            return 0;
        }

        input = m279_rename_read(file->path);
        if (input == NULL) {
            (void)m279_rename_rollback(transaction);
            return 0;
        }
        output = m279_rename_replace(input, old_name, new_name,
                                     file->replacements);
        free(input);
        if (output == NULL) {
            (void)m279_rename_rollback(transaction);
            return 0;
        }

        file->new_spec[0] = '\0';
        if (!m279_rename_copy_text(file->path, output,
                                   file->new_spec,
                                   sizeof(file->new_spec))) {
            free(output);
            (void)m279_rename_rollback(transaction);
            return 0;
        }
        free(output);

        if (strcmp(file->new_spec, file->original_spec) == 0) {
            (void)m279_rename_rollback(transaction);
            return 0;
        }
        file->written = 1;
    }

    return 1;
}

static int m279_rename_object_exists(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0;
}

static int m279_rename_invalidate(const m279_rename_txn *transaction)
{
    unsigned int index;

    for (index = 0U; index < transaction->file_count; ++index) {
        const char *path;
        const char *extension;
        const char *name;
        size_t name_length;
        char object_name[256];
        char build_path[512];
        char root_path[512];
        char command[640];
        int status;

        path = transaction->files[index].path;
        extension = strrchr(path, '.');
        if (extension == NULL ||
            (tolower((unsigned char)extension[1]) != 'c' ||
             extension[2] != '\0')) {
            continue;
        }

        name = strrchr(path, '/');
        name = name == NULL ? path : name + 1;
        name_length = (size_t)(extension - name);
        if (name_length == 0U || name_length >= sizeof(object_name)) {
            return 0;
        }
        (void)memcpy(object_name, name, name_length);
        object_name[name_length] = '\0';

        if (snprintf(build_path, sizeof(build_path),
                     "BUILD/%s.OBJ", object_name) < 0 ||
            snprintf(root_path, sizeof(root_path),
                     "%s.OBJ", object_name) < 0) {
            return 0;
        }

        if (m279_rename_object_exists(build_path)) {
            (void)snprintf(command, sizeof(command),
                           "DELETE [.BUILD]%s.OBJ;*", object_name);
            status = system(command);
            if (status != 0 && (status & 1) == 0) {
                return 0;
            }
        } else if (m279_rename_object_exists(root_path)) {
            (void)snprintf(command, sizeof(command),
                           "DELETE []%s.OBJ;*", object_name);
            status = system(command);
            if (status != 0 && (status & 1) == 0) {
                return 0;
            }
        }
    }

    return 1;
}

static int m279_rename_build(void)
{
    int status;

    status = system("@BUILD.COM");
    return status == 0 || (status & 1) != 0;
}

static int m279_rename_reindex(agent_state *state)
{
    FILE *file;

    symbol_reindex(state);
    file = fopen(SYMBOL_INDEX_FILE, "r");
    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static void m279_rename_result(int reindex_ok, int build_ok)
{
    (void)puts("");
    (void)puts("Rename verification");
    (void)puts("----------------------------------------");
    (void)printf("Symbol index rebuild: %s\n",
                 reindex_ok ? "PASS" : "FAIL");
    (void)printf("Controlled build:     %s\n",
                 build_ok ? "PASS" : "FAIL");
}

static int m279_rename_prepare(agent_state *state,
                               const char *old_name,
                               const char *new_name,
                               m279_rename_txn *transaction)
{
    int collected;

    if (state == NULL || state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }
    if (!m279_rename_name_valid(old_name) ||
        !m279_rename_name_valid(new_name)) {
        (void)puts("Both names must be valid C identifiers.");
        return 0;
    }
    if (strcmp(old_name, new_name) == 0) {
        (void)puts("Old and new names must be different.");
        return 0;
    }

    collected = m279_rename_collect(old_name, new_name, transaction);
    if (collected <= 0) {
        if (collected == 0) {
            (void)puts("Unable to scan project source for rename.");
        }
        return 0;
    }
    if (transaction->total_replacements == 0U) {
        (void)puts("No exact identifier matches found.");
        return 0;
    }

    return 1;
}

static int m279_rename_apply(agent_state *state,
                             const char *old_name,
                             const char *new_name)
{
    m279_rename_txn transaction;

    if (!m279_rename_prepare(state, old_name, new_name, &transaction)) {
        return 0;
    }
    if (!m279_rename_confirm(&transaction, old_name, new_name)) {
        (void)puts("Rename cancelled.");
        return 0;
    }
    if (!m279_rename_apply_txn(&transaction, old_name, new_name)) {
        (void)puts("Rename failed while writing project files.");
        if (m279_rename_rollback(&transaction)) {
            (void)puts("Any created OpenVMS file versions were removed.");
        } else {
            (void)puts("Rollback was incomplete; inspect file versions.");
        }
        return 0;
    }

    (void)puts("Rename applied. The symbol index is now stale.");
    (void)puts("Run REINDEX, then BUILD or AGENT/VERIFY.");
    return 1;
}

static int m279_rename_verify(agent_state *state,
                              const char *old_name,
                              const char *new_name)
{
    m279_rename_txn transaction;
    int reindex_ok;
    int build_ok;

    if (!m279_rename_prepare(state, old_name, new_name, &transaction)) {
        return 0;
    }
    if (!m279_rename_confirm(&transaction, old_name, new_name)) {
        (void)puts("Rename cancelled.");
        return 0;
    }
    if (!m279_rename_apply_txn(&transaction, old_name, new_name)) {
        (void)puts("Rename failed while writing project files.");
        if (m279_rename_rollback(&transaction)) {
            (void)puts("Any created OpenVMS file versions were removed.");
        } else {
            (void)puts("Rollback was incomplete; inspect file versions.");
        }
        return 0;
    }

    (void)puts("Rename applied. Rebuilding symbol index...");
    reindex_ok = m279_rename_reindex(state);
    (void)puts("Invalidating changed object files...");
    build_ok = reindex_ok && m279_rename_invalidate(&transaction);
    (void)puts("Running controlled build...");
    build_ok = build_ok ? m279_rename_build() : 0;
    m279_rename_result(reindex_ok, build_ok);

    if (reindex_ok && build_ok) {
        (void)puts("Rename completed successfully.");
        return 1;
    }

    (void)puts("Rename verification failed. Rolling back new versions...");
    if (!m279_rename_rollback(&transaction)) {
        (void)puts("Rollback was incomplete; inspect file versions.");
        return 0;
    }
    (void)puts("Previous file versions restored by removing new versions.");
    (void)puts("Rebuilding symbol index after rollback...");
    reindex_ok = m279_rename_reindex(state);
    (void)puts("Invalidating restored object files...");
    build_ok = reindex_ok && m279_rename_invalidate(&transaction);
    (void)puts("Running controlled build after rollback...");
    build_ok = build_ok ? m279_rename_build() : 0;

    (void)puts("");
    (void)puts("Rollback verification");
    (void)puts("----------------------------------------");
    (void)printf("Symbol index rebuild: %s\n",
                 reindex_ok ? "PASS" : "FAIL");
    (void)printf("Controlled build:     %s\n",
                 build_ok ? "PASS" : "FAIL");
    return 0;
}
