#include "llm_internal.h"

#define OPENAI_SESSION_FILE "OVMS_AGENT_SESSIONS.DAT"
#define OPENAI_CURRENT_FILE "OVMS_AGENT_SESSION.CUR"
#define OPENAI_SESSION_MAX 64U
#define OPENAI_SESSION_NAME 64U
#define OPENAI_SESSION_GOAL 512U
#define OPENAI_SESSION_TIME 32U
#define OPENAI_SESSION_LINE 1400U

typedef struct openai_session_rec {
    char id[9];
    int archived;
    char created[OPENAI_SESSION_TIME];
    char updated[OPENAI_SESSION_TIME];
    char parent[9];
    char policy[16];
    char name[OPENAI_SESSION_NAME];
    char original_goal[OPENAI_SESSION_GOAL];
    char current_goal[OPENAI_SESSION_GOAL];
    unsigned long exec_count;
} openai_session_rec;

static char openai_session_path[256] = OPENAI_SESSION_FILE;
static char openai_current_path[256] = OPENAI_CURRENT_FILE;
static unsigned long openai_session_seed = 0UL;

static void openai_session_now(char *output, size_t output_size)
{
    time_t now;
    struct tm *local;

    if (output == NULL || output_size == 0U) {
        return;
    }

    output[0] = '\0';
    now = time(NULL);
    local = localtime(&now);

    if (local == NULL) {
        (void)strncpy(output, "1970-01-01T00:00:00", output_size - 1U);
        output[output_size - 1U] = '\0';
        return;
    }

    if (strftime(output, output_size, "%Y-%m-%dT%H:%M:%S", local) == 0U) {
        (void)strncpy(output, "1970-01-01T00:00:00", output_size - 1U);
        output[output_size - 1U] = '\0';
    }
}

static void openai_session_clean(const char *input,
                                 char *output,
                                 size_t output_size)
{
    size_t used;
    unsigned char ch;

    if (output == NULL || output_size == 0U) {
        return;
    }

    used = 0U;

    if (input == NULL) {
        output[0] = '\0';
        return;
    }

    while (*input != '\0' && used + 1U < output_size) {
        ch = (unsigned char)*input++;

        if (ch == (unsigned char)'|' ||
            ch == (unsigned char)'\r' ||
            ch == (unsigned char)'\n' ||
            ch == (unsigned char)'\t') {
            output[used++] = ' ';
        } else if (ch >= 32U && ch < 127U) {
            output[used++] = (char)ch;
        }
    }

    while (used > 0U && output[used - 1U] == ' ') {
        --used;
    }

    output[used] = '\0';
}

static const char *openai_session_skip(const char *text)
{
    if (text == NULL) {
        return "";
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    return text;
}

static int openai_session_id_ok(const char *id)
{
    unsigned int count;
    unsigned char ch;

    if (id == NULL) {
        return 0;
    }

    count = 0U;

    while (*id != '\0') {
        ch = (unsigned char)*id++;

        if (!((ch >= (unsigned char)'0' && ch <= (unsigned char)'9') ||
              (ch >= (unsigned char)'A' && ch <= (unsigned char)'F') ||
              (ch >= (unsigned char)'a' && ch <= (unsigned char)'f'))) {
            return 0;
        }

        ++count;
    }

    return count == 8U;
}

static void openai_session_upper_id(const char *input, char output[9])
{
    unsigned int index;
    unsigned char ch;

    for (index = 0U; index < 8U; ++index) {
        ch = (unsigned char)input[index];

        if (ch >= (unsigned char)'a' && ch <= (unsigned char)'f') {
            ch = (unsigned char)(ch - (unsigned char)'a' + (unsigned char)'A');
        }

        output[index] = (char)ch;
    }

    output[8] = '\0';
}

static unsigned long openai_session_hash(const char *text,
                                         unsigned long seed)
{
    unsigned long hash;
    unsigned char ch;

    hash = 2166136261UL ^ seed;

    if (text == NULL) {
        return hash;
    }

    while (*text != '\0') {
        ch = (unsigned char)*text++;
        hash ^= (unsigned long)ch;
        hash *= 16777619UL;
    }

    return hash;
}

static int openai_session_parse(char *line, openai_session_rec *record)
{
    char *fields[10];
    char *position;
    unsigned int index;

    if (line == NULL || record == NULL) {
        return 0;
    }

    position = line;

    for (index = 0U; index < 10U; ++index) {
        fields[index] = position;

        if (index == 9U) {
            break;
        }

        position = strchr(position, '|');

        if (position == NULL) {
            return 0;
        }

        *position++ = '\0';
    }

    position = strchr(fields[9], '\n');
    if (position != NULL) {
        *position = '\0';
    }
    position = strchr(fields[9], '\r');
    if (position != NULL) {
        *position = '\0';
    }

    if (!openai_session_id_ok(fields[0])) {
        return 0;
    }

    (void)memset(record, 0, sizeof(*record));
    openai_session_upper_id(fields[0], record->id);
    record->archived = atoi(fields[1]) != 0;

    (void)strncpy(record->created, fields[2], sizeof(record->created) - 1U);
    (void)strncpy(record->updated, fields[3], sizeof(record->updated) - 1U);
    (void)strncpy(record->parent, fields[4], sizeof(record->parent) - 1U);
    (void)strncpy(record->policy, fields[5], sizeof(record->policy) - 1U);
    (void)strncpy(record->name, fields[6], sizeof(record->name) - 1U);
    (void)strncpy(record->original_goal, fields[7],
                  sizeof(record->original_goal) - 1U);
    (void)strncpy(record->current_goal, fields[8],
                  sizeof(record->current_goal) - 1U);
    record->exec_count = strtoul(fields[9], NULL, 10);

    record->created[sizeof(record->created) - 1U] = '\0';
    record->updated[sizeof(record->updated) - 1U] = '\0';
    record->parent[sizeof(record->parent) - 1U] = '\0';
    record->policy[sizeof(record->policy) - 1U] = '\0';
    record->name[sizeof(record->name) - 1U] = '\0';
    record->original_goal[sizeof(record->original_goal) - 1U] = '\0';
    record->current_goal[sizeof(record->current_goal) - 1U] = '\0';

    return 1;
}

static unsigned int openai_session_load(openai_session_rec *records,
                                        unsigned int capacity)
{
    FILE *file;
    char line[OPENAI_SESSION_LINE];
    unsigned int count;

    if (records == NULL || capacity == 0U) {
        return 0U;
    }

    file = fopen(openai_session_path, "r");

    if (file == NULL) {
        return 0U;
    }

    count = 0U;

    while (count < capacity &&
           fgets(line, sizeof(line), file) != NULL) {
        openai_session_rec record;

        if (openai_session_parse(line, &record)) {
            records[count++] = record;
        }
    }

    (void)fclose(file);
    return count;
}

static int openai_session_save(const openai_session_rec *records,
                               unsigned int count)
{
    FILE *file;
    unsigned int index;

    file = fopen(openai_session_path, "w");

    if (file == NULL) {
        return 0;
    }

    for (index = 0U; index < count; ++index) {
        const openai_session_rec *record;

        record = &records[index];

        if (fprintf(
                file,
                "%s|%d|%s|%s|%s|%s|%s|%s|%s|%lu\n",
                record->id,
                record->archived ? 1 : 0,
                record->created,
                record->updated,
                record->parent,
                record->policy,
                record->name,
                record->original_goal,
                record->current_goal,
                record->exec_count) < 0) {
            (void)fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int openai_session_current(char id[9])
{
    FILE *file;
    char line[64];
    char *end;

    if (id == NULL) {
        return 0;
    }

    id[0] = '\0';
    file = fopen(openai_current_path, "r");

    if (file == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        (void)fclose(file);
        return 0;
    }

    (void)fclose(file);

    end = strchr(line, '\n');
    if (end != NULL) {
        *end = '\0';
    }
    end = strchr(line, '\r');
    if (end != NULL) {
        *end = '\0';
    }

    if (!openai_session_id_ok(line)) {
        return 0;
    }

    openai_session_upper_id(line, id);
    return 1;
}

static int openai_session_set_current(const char *id)
{
    FILE *file;

    if (!openai_session_id_ok(id)) {
        return 0;
    }

    file = fopen(openai_current_path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fprintf(file, "%s\n", id) < 0) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static void openai_session_clear_current(void)
{
    while (remove(openai_current_path) == 0) {
    }
}

static int openai_session_find(openai_session_rec *records,
                               unsigned int count,
                               const char *id)
{
    char normalized[9];
    unsigned int index;

    if (!openai_session_id_ok(id)) {
        return -1;
    }

    openai_session_upper_id(id, normalized);

    for (index = 0U; index < count; ++index) {
        if (strcmp(records[index].id, normalized) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int openai_session_new_id(const openai_session_rec *records,
                                 unsigned int count,
                                 const char *name,
                                 char id[9])
{
    char timestamp[OPENAI_SESSION_TIME];
    unsigned long hash;
    unsigned int tries;
    unsigned int index;
    int collision;

    openai_session_now(timestamp, sizeof(timestamp));

    for (tries = 0U; tries < 1024U; ++tries) {
        ++openai_session_seed;
        hash = openai_session_hash(timestamp, openai_session_seed);
        hash ^= openai_session_hash(name, openai_session_seed << 1);

        (void)snprintf(id, 9U, "%08lX", hash);

        collision = 0;

        for (index = 0U; index < count; ++index) {
            if (strcmp(records[index].id, id) == 0) {
                collision = 1;
                break;
            }
        }

        if (!collision) {
            return 1;
        }
    }

    return 0;
}

static int openai_session_arg_id(const char *arguments, char id[9])
{
    const char *value;

    value = openai_session_skip(arguments);

    if (!openai_session_id_ok(value)) {
        return 0;
    }

    openai_session_upper_id(value, id);
    return 1;
}

static int openai_session_split_id(const char *arguments,
                                   char id[9],
                                   char *text,
                                   size_t text_size)
{
    const char *position;
    const char *rest;
    char raw_id[16];
    size_t id_length;

    if (text == NULL || text_size == 0U) {
        return 0;
    }

    position = openai_session_skip(arguments);
    rest = position;

    while (*rest != '\0' && *rest != ' ' && *rest != '\t') {
        ++rest;
    }

    id_length = (size_t)(rest - position);

    if (id_length != 8U || id_length >= sizeof(raw_id)) {
        return 0;
    }

    (void)memcpy(raw_id, position, id_length);
    raw_id[id_length] = '\0';

    if (!openai_session_id_ok(raw_id)) {
        return 0;
    }

    openai_session_upper_id(raw_id, id);

    rest = openai_session_skip(rest);
    openai_session_clean(rest, text, text_size);

    return text[0] != '\0';
}

static int openai_session_write_new(const char *name,
                                    const char *parent,
                                    const char *original_goal,
                                    const char *current_goal,
                                    unsigned long exec_count,
                                    char created_id[9])
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    openai_session_rec *record;
    unsigned int count;
    char clean_name[OPENAI_SESSION_NAME];
    char clean_original[OPENAI_SESSION_GOAL];
    char clean_current[OPENAI_SESSION_GOAL];

    openai_session_clean(name, clean_name, sizeof(clean_name));
    openai_session_clean(original_goal, clean_original, sizeof(clean_original));
    openai_session_clean(current_goal, clean_current, sizeof(clean_current));

    if (clean_name[0] == '\0') {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);

    if (count >= OPENAI_SESSION_MAX) {
        return 0;
    }

    record = &records[count];
    (void)memset(record, 0, sizeof(*record));

    if (!openai_session_new_id(records, count, clean_name, record->id)) {
        return 0;
    }

    openai_session_now(record->created, sizeof(record->created));
    (void)strncpy(record->updated, record->created,
                  sizeof(record->updated) - 1U);

    if (parent != NULL && openai_session_id_ok(parent)) {
        openai_session_upper_id(parent, record->parent);
    } else {
        (void)strcpy(record->parent, "-");
    }

    (void)strncpy(record->policy, openai_approval_name(),
                  sizeof(record->policy) - 1U);
    (void)strncpy(record->name, clean_name, sizeof(record->name) - 1U);
    (void)strncpy(record->original_goal, clean_original,
                  sizeof(record->original_goal) - 1U);
    (void)strncpy(record->current_goal, clean_current,
                  sizeof(record->current_goal) - 1U);
    record->exec_count = exec_count;

    if (!openai_session_save(records, count + 1U)) {
        return 0;
    }

    if (!openai_session_set_current(record->id)) {
        return 0;
    }

    if (created_id != NULL) {
        (void)strcpy(created_id, record->id);
    }

    return 1;
}

int openai_session_new(const char *arguments,
                       char created_id[9])
{
    const char *name;

    name = openai_session_skip(arguments);

    return openai_session_write_new(
        name, NULL, "", "", 0UL, created_id
    );
}

int openai_session_list_text(char *output, size_t output_size)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    unsigned int index;
    char current[9];
    int have_current;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    have_current = openai_session_current(current);

    written = snprintf(
        output, output_size,
        "OVMS Agent sessions\n"
        "-------------------\n"
        "Sessions: %u\n",
        count
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;

    for (index = 0U; index < count; ++index) {
        written = snprintf(
            output + used, output_size - used,
            "%c %s  %-10s  %s  %s\n",
            have_current && strcmp(current, records[index].id) == 0 ? '*' : ' ',
            records[index].id,
            records[index].archived ? "archived" : "active",
            records[index].updated,
            records[index].name
        );

        if (written < 0 || (size_t)written >= output_size - used) {
            return 0;
        }

        used += (size_t)written;
    }

    return 1;
}

int openai_session_show_text(const char *arguments,
                             char *output,
                             size_t output_size)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    int index;
    int written;

    if (output == NULL || output_size == 0U ||
        !openai_session_arg_id(arguments, id)) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent session\n"
        "------------------\n"
        "ID:            %s\n"
        "Name:          %s\n"
        "Created:       %s\n"
        "Updated:       %s\n"
        "Archived:      %s\n"
        "Parent:        %s\n"
        "Policy:        %s\n"
        "Original goal: %s\n"
        "Current goal:  %s\n"
        "Executions:    %lu\n",
        records[index].id,
        records[index].name,
        records[index].created,
        records[index].updated,
        records[index].archived ? "yes" : "no",
        records[index].parent,
        records[index].policy,
        records[index].original_goal[0] != '\0' ?
            records[index].original_goal : "(none)",
        records[index].current_goal[0] != '\0' ?
            records[index].current_goal : "(none)",
        records[index].exec_count
    );

    return written >= 0 && (size_t)written < output_size;
}

int openai_session_resume(const char *arguments)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    int index;

    if (!openai_session_arg_id(arguments, id)) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0 || records[index].archived) {
        return 0;
    }

    return openai_session_set_current(records[index].id);
}

int openai_session_fork(const char *arguments,
                        char created_id[9])
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char parent_id[9];
    char name[OPENAI_SESSION_NAME];
    int index;

    if (!openai_session_split_id(
            arguments, parent_id, name, sizeof(name))) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, parent_id);

    if (index < 0) {
        return 0;
    }

    return openai_session_write_new(
        name,
        records[index].id,
        records[index].original_goal,
        records[index].current_goal,
        records[index].exec_count,
        created_id
    );
}

int openai_session_rename(const char *arguments)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    char name[OPENAI_SESSION_NAME];
    int index;

    if (!openai_session_split_id(arguments, id, name, sizeof(name))) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0) {
        return 0;
    }

    (void)strncpy(records[index].name, name,
                  sizeof(records[index].name) - 1U);
    records[index].name[sizeof(records[index].name) - 1U] = '\0';
    openai_session_now(records[index].updated,
                       sizeof(records[index].updated));

    return openai_session_save(records, count);
}

static int openai_session_archive_id(const char *arguments,
                                     int archived)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    char current[9];
    int index;

    if (!openai_session_arg_id(arguments, id)) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0) {
        return 0;
    }

    records[index].archived = archived ? 1 : 0;
    openai_session_now(records[index].updated,
                       sizeof(records[index].updated));

    if (!openai_session_save(records, count)) {
        return 0;
    }

    if (archived &&
        openai_session_current(current) &&
        strcmp(current, records[index].id) == 0) {
        openai_session_clear_current();
    }

    return 1;
}

int openai_session_archive(const char *arguments)
{
    return openai_session_archive_id(arguments, 1);
}

int openai_session_unarchive(const char *arguments)
{
    return openai_session_archive_id(arguments, 0);
}

int openai_session_delete(const char *arguments)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    unsigned int index;
    unsigned int out;
    char id[9];
    char current[9];
    int found;

    if (!openai_session_arg_id(arguments, id)) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    found = 0;
    out = 0U;

    for (index = 0U; index < count; ++index) {
        if (strcmp(records[index].id, id) == 0) {
            found = 1;
            continue;
        }

        if (out != index) {
            records[out] = records[index];
        }
        ++out;
    }

    if (!found) {
        return 0;
    }

    if (!openai_session_save(records, out)) {
        return 0;
    }

    if (openai_session_current(current) &&
        strcmp(current, id) == 0) {
        openai_session_clear_current();
    }

    return 1;
}

int openai_session_current_id(char id[9])
{
    return openai_session_current(id);
}

int openai_session_parent(const char *arguments,
                          char parent[9])
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    int index;

    if (parent == NULL ||
        !openai_session_arg_id(arguments, id)) {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0) {
        return 0;
    }

    (void)strncpy(parent, records[index].parent, 8U);
    parent[8] = '\0';
    return 1;
}

int openai_session_current_text(char *output,
                                size_t output_size)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    int index;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (!openai_session_current(id)) {
        written = snprintf(
            output, output_size,
            "OVMS Agent current session\n"
            "--------------------------\n"
            "Current session: none\n"
        );
        return written >= 0 && (size_t)written < output_size;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent current session\n"
        "--------------------------\n"
        "ID:       %s\n"
        "Name:     %s\n"
        "Updated:  %s\n"
        "Policy:   %s\n"
        "Goal:     %s\n"
        "Execs:    %lu\n",
        records[index].id,
        records[index].name,
        records[index].updated,
        records[index].policy,
        records[index].current_goal[0] != '\0' ?
            records[index].current_goal : "(none)",
        records[index].exec_count
    );

    return written >= 0 && (size_t)written < output_size;
}

int openai_session_note_goal(const char *goal)
{
    openai_session_rec records[OPENAI_SESSION_MAX];
    unsigned int count;
    char id[9];
    char clean_goal[OPENAI_SESSION_GOAL];
    int index;

    if (!openai_session_current(id)) {
        return 1;
    }

    openai_session_clean(goal, clean_goal, sizeof(clean_goal));

    if (clean_goal[0] == '\0') {
        return 0;
    }

    count = openai_session_load(records, OPENAI_SESSION_MAX);
    index = openai_session_find(records, count, id);

    if (index < 0 || records[index].archived) {
        return 0;
    }

    if (records[index].original_goal[0] == '\0') {
        (void)strncpy(records[index].original_goal, clean_goal,
                      sizeof(records[index].original_goal) - 1U);
    }

    (void)strncpy(records[index].current_goal, clean_goal,
                  sizeof(records[index].current_goal) - 1U);
    (void)strncpy(records[index].policy, openai_approval_name(),
                  sizeof(records[index].policy) - 1U);
    ++records[index].exec_count;
    openai_session_now(records[index].updated,
                       sizeof(records[index].updated));

    return openai_session_save(records, count);
}

void openai_show_session_list(void)
{
    char output[8192];

    if (!openai_session_list_text(output, sizeof(output))) {
        (void)puts("Unable to list sessions.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_session(const char *arguments)
{
    char output[4096];

    if (!openai_session_show_text(arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/SESSION/SHOW <id>");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_session_current(void)
{
    char output[2048];

    if (!openai_session_current_text(output, sizeof(output))) {
        (void)puts("Unable to show current session.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_session_new_cmd(const char *arguments)
{
    char id[9];

    if (!openai_session_new(arguments, id)) {
        (void)puts("Usage: AGENT/SESSION/NEW <name>");
        return;
    }

    (void)printf("Created session %s and made it current.\n", id);
}

void openai_session_resume_cmd(const char *arguments)
{
    if (!openai_session_resume(arguments)) {
        (void)puts("Unable to resume session. It may not exist or may be archived.");
        return;
    }

    (void)puts("Session resumed.");
}

void openai_session_fork_cmd(const char *arguments)
{
    char id[9];

    if (!openai_session_fork(arguments, id)) {
        (void)puts("Usage: AGENT/SESSION/FORK <id> <new-name>");
        return;
    }

    (void)printf("Forked session %s and made it current.\n", id);
}

void openai_session_rename_cmd(const char *arguments)
{
    if (!openai_session_rename(arguments)) {
        (void)puts("Usage: AGENT/SESSION/RENAME <id> <new-name>");
        return;
    }

    (void)puts("Session renamed.");
}

void openai_session_archive_cmd(const char *arguments)
{
    if (!openai_session_archive(arguments)) {
        (void)puts("Usage: AGENT/SESSION/ARCHIVE <id>");
        return;
    }

    (void)puts("Session archived.");
}

void openai_session_unarc_cmd(const char *arguments)
{
    if (!openai_session_unarchive(arguments)) {
        (void)puts("Usage: AGENT/SESSION/UNARCHIVE <id>");
        return;
    }

    (void)puts("Session unarchived.");
}

void openai_session_delete_cmd(const char *arguments)
{
    if (!openai_session_delete(arguments)) {
        (void)puts("Usage: AGENT/SESSION/DELETE <id>");
        return;
    }

    (void)puts("Session deleted.");
}

void openai_test_session_paths(const char *data_path,
                               const char *current_path)
{
    if (data_path == NULL || *data_path == '\0') {
        (void)strcpy(openai_session_path, OPENAI_SESSION_FILE);
    } else {
        (void)strncpy(openai_session_path, data_path,
                      sizeof(openai_session_path) - 1U);
        openai_session_path[sizeof(openai_session_path) - 1U] = '\0';
    }

    if (current_path == NULL || *current_path == '\0') {
        (void)strcpy(openai_current_path, OPENAI_CURRENT_FILE);
    } else {
        (void)strncpy(openai_current_path, current_path,
                      sizeof(openai_current_path) - 1U);
        openai_current_path[sizeof(openai_current_path) - 1U] = '\0';
    }
}
