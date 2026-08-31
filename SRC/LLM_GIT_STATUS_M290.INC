#include "OVMS_STATUS.H"

#define M290_GIT_META_COM    "OVMS_AGENT_GIT_META.TMP"
#define M290_GIT_INDEX_TMP   "OVMS_AGENT_GIT_INDEX.TMP"
#define M290_GIT_CACHED_TMP  "OVMS_AGENT_GIT_CACHED.TMP"
#define M290_GIT_OTHER_TMP   "OVMS_AGENT_GIT_OTHER.TMP"
#define M290_GIT_HASH_TMP    "OVMS_AGENT_GIT_HASH.TMP"
#define M290_GIT_LINE_MAX    1024U
#define M290_GIT_HASH_HEX    40U

typedef unsigned int m290_sha1_word;

typedef struct m290_sha1_context {
    m290_sha1_word state[5];
    m290_sha1_word bit_count_high;
    m290_sha1_word bit_count_low;
    unsigned char block[64];
    unsigned int used;
} m290_sha1_context;

static m290_sha1_word m290_sha1_rotate_left(
    m290_sha1_word value,
    unsigned int count)
{
    return (value << count) | (value >> (32U - count));
}

static void m290_sha1_transform(
    m290_sha1_context *context,
    const unsigned char *block)
{
    m290_sha1_word words[80];
    m290_sha1_word a;
    m290_sha1_word b;
    m290_sha1_word c;
    m290_sha1_word d;
    m290_sha1_word e;
    m290_sha1_word f;
    m290_sha1_word k;
    m290_sha1_word temporary;
    unsigned int index;

    for (index = 0U; index < 16U; ++index) {
        words[index] =
            ((m290_sha1_word)block[index * 4U] << 24) |
            ((m290_sha1_word)block[index * 4U + 1U] << 16) |
            ((m290_sha1_word)block[index * 4U + 2U] << 8) |
            (m290_sha1_word)block[index * 4U + 3U];
    }

    for (index = 16U; index < 80U; ++index) {
        words[index] = m290_sha1_rotate_left(
            words[index - 3U] ^ words[index - 8U] ^
            words[index - 14U] ^ words[index - 16U],
            1U);
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];

    for (index = 0U; index < 80U; ++index) {
        if (index < 20U) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        } else if (index < 40U) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (index < 60U) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }

        temporary = m290_sha1_rotate_left(a, 5U) +
                    f + e + k + words[index];
        e = d;
        d = c;
        c = m290_sha1_rotate_left(b, 30U);
        b = a;
        a = temporary;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}

static void m290_sha1_init(m290_sha1_context *context)
{
    context->state[0] = 0x67452301U;
    context->state[1] = 0xEFCDAB89U;
    context->state[2] = 0x98BADCFEU;
    context->state[3] = 0x10325476U;
    context->state[4] = 0xC3D2E1F0U;
    context->bit_count_high = 0U;
    context->bit_count_low = 0U;
    context->used = 0U;
}

static void m290_sha1_update(
    m290_sha1_context *context,
    const unsigned char *data,
    size_t length)
{
    m290_sha1_word old_low;

    old_low = context->bit_count_low;
    context->bit_count_low += (m290_sha1_word)(length << 3);
    if (context->bit_count_low < old_low) {
        ++context->bit_count_high;
    }
    context->bit_count_high += (m290_sha1_word)(length >> 29);

    while (length > 0U) {
        size_t take;

        take = 64U - context->used;
        if (take > length) {
            take = length;
        }

        (void)memcpy(
            context->block + context->used,
            data,
            take);
        context->used += (unsigned int)take;
        data += take;
        length -= take;

        if (context->used == 64U) {
            m290_sha1_transform(context, context->block);
            context->used = 0U;
        }
    }
}

static void m290_sha1_final(
    m290_sha1_context *context,
    unsigned char digest[20])
{
    unsigned char padding[64];
    unsigned char length_bytes[8];
    m290_sha1_word high;
    m290_sha1_word low;
    size_t padding_length;
    unsigned int index;

    high = context->bit_count_high;
    low = context->bit_count_low;

    (void)memset(padding, 0, sizeof(padding));
    padding[0] = 0x80U;

    for (index = 0U; index < 4U; ++index) {
        length_bytes[index] =
            (unsigned char)(high >> (24U - index * 8U));
        length_bytes[index + 4U] =
            (unsigned char)(low >> (24U - index * 8U));
    }

    padding_length = context->used < 56U ?
                     56U - context->used :
                     120U - context->used;
    m290_sha1_update(context, padding, padding_length);
    m290_sha1_update(context, length_bytes, sizeof(length_bytes));

    for (index = 0U; index < 5U; ++index) {
        digest[index * 4U] =
            (unsigned char)(context->state[index] >> 24);
        digest[index * 4U + 1U] =
            (unsigned char)(context->state[index] >> 16);
        digest[index * 4U + 2U] =
            (unsigned char)(context->state[index] >> 8);
        digest[index * 4U + 3U] =
            (unsigned char)context->state[index];
    }
}

static void m290_sha1_hex(
    const unsigned char digest[20],
    char *hex)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int index;

    for (index = 0U; index < 20U; ++index) {
        hex[index * 2U] = digits[digest[index] >> 4];
        hex[index * 2U + 1U] = digits[digest[index] & 0x0FU];
    }
    hex[M290_GIT_HASH_HEX] = '\0';
}

int llm_git_rms_blob_hash(
    const char *path,
    char *hash,
    size_t hash_size)
{
    FILE *file;
    unsigned char buffer[4096];
    unsigned char digest[20];
    m290_sha1_context context;
    unsigned long file_size;
    size_t got;
    char header[64];
    int written;
    int ok;

    if (sizeof(m290_sha1_word) != 4U ||
        !m263_git_path_ok(path) ||
        hash == NULL || hash_size <= M290_GIT_HASH_HEX) {
        return 0;
    }

    hash[0] = '\0';
    if (!llm_git_rms_copy(path, M290_GIT_HASH_TMP)) {
        return 0;
    }

    file = fopen(M290_GIT_HASH_TMP, "rb");
    if (file == NULL) {
        llm_git_remove(M290_GIT_HASH_TMP);
        return 0;
    }

    file_size = 0UL;
    ok = 1;
    while ((got = fread(buffer, 1U, sizeof(buffer), file)) != 0U) {
        if ((unsigned long)got > 0xFFFFFFFFUL - file_size) {
            ok = 0;
            break;
        }
        file_size += (unsigned long)got;
    }
    if (ferror(file)) {
        ok = 0;
    }

    if (!ok || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        llm_git_remove(M290_GIT_HASH_TMP);
        return 0;
    }

    written = snprintf(
        header,
        sizeof(header),
        "blob %lu",
        file_size);
    if (written < 0 || (size_t)written + 1U > sizeof(header)) {
        (void)fclose(file);
        llm_git_remove(M290_GIT_HASH_TMP);
        return 0;
    }

    m290_sha1_init(&context);
    m290_sha1_update(
        &context,
        (const unsigned char *)header,
        (size_t)written + 1U);

    while ((got = fread(buffer, 1U, sizeof(buffer), file)) != 0U) {
        m290_sha1_update(&context, buffer, got);
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }

    llm_git_remove(M290_GIT_HASH_TMP);
    if (!ok) {
        return 0;
    }

    m290_sha1_final(&context, digest);
    m290_sha1_hex(digest, hash);
    return 1;
}

static int m290_git_ensure_empty_file(const char *path)
{
    FILE *file;

    file = fopen(path, "r");
    if (file != NULL) {
        return fclose(file) == 0;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }
    return fclose(file) == 0;
}

static int m290_git_run_to_file(
    const char *arguments,
    const char *output_file)
{
    FILE *writer;
    FILE *command;
    char full[LLM_GIT_LINE_MAX];
    char dcl[256];
    int status;

    if (arguments == NULL || *arguments == '\0' ||
        output_file == NULL || *output_file == '\0') {
        return 0;
    }

    llm_git_remove(M290_GIT_META_COM);
    llm_git_remove(output_file);

    writer = fopen(output_file, "w");
    if (writer == NULL) {
        return 0;
    }
    if (fgetname(writer, full, 1) == NULL) {
        (void)fclose(writer);
        llm_git_remove(output_file);
        return 0;
    }
    if (fclose(writer) != 0) {
        llm_git_remove(output_file);
        return 0;
    }

    llm_git_remove(full);
    command = fopen(M290_GIT_META_COM, "w");
    if (command == NULL) {
        return 0;
    }

    if (fprintf(
            command,
            "$ SET NOON\n"
            "$ DEFINE/USER/NOLOG SYS$OUTPUT %s\n"
            "$ GIT %s\n"
            "$ M290_GIT_STATUS = $STATUS\n"
            "$ EXIT 'M290_GIT_STATUS'\n",
            full,
            arguments) < 0 ||
        fclose(command) != 0) {
        llm_git_remove(M290_GIT_META_COM);
        llm_git_remove(output_file);
        return 0;
    }

    (void)snprintf(
        dcl,
        sizeof(dcl),
        "@%s",
        M290_GIT_META_COM);
    status = system(dcl);
    llm_git_remove(M290_GIT_META_COM);

    if (!ovms_status_success((unsigned long)status)) {
        llm_git_remove(output_file);
        return 0;
    }

    return m290_git_ensure_empty_file(output_file);
}

static int m290_git_hash_text_ok(const char *hash)
{
    size_t index;

    if (hash == NULL || strlen(hash) != M290_GIT_HASH_HEX) {
        return 0;
    }

    for (index = 0U; index < M290_GIT_HASH_HEX; ++index) {
        char ch;

        ch = hash[index];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'a' && ch <= 'f') ||
              (ch >= 'A' && ch <= 'F'))) {
            return 0;
        }
    }
    return 1;
}

static int m290_git_parse_index_line(
    const char *line,
    char *hash,
    size_t hash_size,
    char *path,
    size_t path_size)
{
    const char *first_space;
    const char *hash_start;
    const char *second_space;
    const char *stage_start;
    const char *tab;
    const char *path_start;
    size_t hash_length;
    size_t path_length;

    if (line == NULL || hash == NULL || hash_size == 0U ||
        path == NULL || path_size == 0U) {
        return 0;
    }

    first_space = strchr(line, ' ');
    if (first_space == NULL) {
        return 0;
    }
    hash_start = first_space + 1;
    second_space = strchr(hash_start, ' ');
    if (second_space == NULL) {
        return 0;
    }

    hash_length = (size_t)(second_space - hash_start);
    if (hash_length + 1U > hash_size ||
        hash_length != M290_GIT_HASH_HEX) {
        return 0;
    }
    (void)memcpy(hash, hash_start, hash_length);
    hash[hash_length] = '\0';
    if (!m290_git_hash_text_ok(hash)) {
        return 0;
    }

    stage_start = second_space + 1;
    tab = strchr(stage_start, '\t');
    if (tab == NULL || tab != stage_start + 1 || *stage_start != '0') {
        return 0;
    }

    path_start = tab + 1;
    path_length = 0U;
    while (path_start[path_length] != '\0' &&
           path_start[path_length] != '\n' &&
           path_start[path_length] != '\r') {
        ++path_length;
    }
    if (path_length == 0U || path_length >= path_size) {
        return 0;
    }

    (void)memcpy(path, path_start, path_length);
    path[path_length] = '\0';
    return m263_git_path_ok(path);
}

static int m290_git_parse_name_status_line(
    const char *line,
    char *kind,
    char *path,
    size_t path_size)
{
    const char *tab;
    const char *path_start;
    size_t path_length;

    if (line == NULL || kind == NULL || path == NULL || path_size == 0U) {
        return 0;
    }

    tab = strchr(line, '\t');
    if (tab == NULL || tab != line + 1 ||
        line[0] < 'A' || line[0] > 'Z') {
        return 0;
    }

    path_start = tab + 1;
    path_length = 0U;
    while (path_start[path_length] != '\0' &&
           path_start[path_length] != '\n' &&
           path_start[path_length] != '\r') {
        ++path_length;
    }
    if (path_length == 0U || path_length >= path_size) {
        return 0;
    }

    (void)memcpy(path, path_start, path_length);
    path[path_length] = '\0';
    if (!m263_git_path_ok(path)) {
        return 0;
    }

    *kind = line[0];
    return 1;
}

static int m290_git_append_status(
    char index_kind,
    char work_kind,
    const char *path)
{
    size_t used;
    int written;

    if (path == NULL || !m263_git_path_ok(path)) {
        return 0;
    }
    if (index_kind == ' ' && work_kind == ' ') {
        return 1;
    }

    used = strlen(llm_git_status);
    written = snprintf(
        llm_git_status + used,
        sizeof(llm_git_status) - used,
        "%s%c%c %s",
        used == 0U ? "" : "\n",
        index_kind,
        work_kind,
        path);
    if (written < 0 ||
        (size_t)written >= sizeof(llm_git_status) - used) {
        llm_git_status[0] = '\0';
        llm_git_truncated = 1;
        return 0;
    }
    return 1;
}

static int m290_git_cached_kind(
    const char *wanted_path,
    char *kind)
{
    FILE *file;
    char line[M290_GIT_LINE_MAX];
    char path[512];
    char current_kind;
    int ok;

    if (wanted_path == NULL || kind == NULL) {
        return 0;
    }

    *kind = ' ';
    file = fopen(M290_GIT_CACHED_TMP, "r");
    if (file == NULL) {
        return 0;
    }

    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (!m290_git_parse_name_status_line(
                line,
                &current_kind,
                path,
                sizeof(path))) {
            ok = 0;
            break;
        }
        if (strcmp(path, wanted_path) == 0) {
            *kind = current_kind;
            break;
        }
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int m290_git_index_contains(
    const char *wanted_path,
    int *found)
{
    FILE *file;
    char line[M290_GIT_LINE_MAX];
    char hash[M290_GIT_HASH_HEX + 1U];
    char path[512];
    int ok;

    if (wanted_path == NULL || found == NULL) {
        return 0;
    }

    *found = 0;
    file = fopen(M290_GIT_INDEX_TMP, "r");
    if (file == NULL) {
        return 0;
    }

    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (!m290_git_parse_index_line(
                line,
                hash,
                sizeof(hash),
                path,
                sizeof(path))) {
            ok = 0;
            break;
        }
        if (strcmp(path, wanted_path) == 0) {
            *found = 1;
            break;
        }
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int m290_git_work_kind(
    const char *path,
    const char *index_hash,
    char *kind)
{
    FILE *check;
    char work_hash[M290_GIT_HASH_HEX + 1U];

    if (path == NULL || index_hash == NULL || kind == NULL) {
        return 0;
    }

    check = fopen(path, "r");
    if (check == NULL) {
        *kind = 'D';
        return 1;
    }
    if (fclose(check) != 0) {
        return 0;
    }

    if (!llm_git_rms_blob_hash(
            path,
            work_hash,
            sizeof(work_hash))) {
        return 0;
    }

    *kind = strcmp(work_hash, index_hash) == 0 ? ' ' : 'M';
    return 1;
}

static int m290_git_status_from_index(void)
{
    FILE *file;
    char line[M290_GIT_LINE_MAX];
    char index_hash[M290_GIT_HASH_HEX + 1U];
    char path[512];
    char index_kind;
    char work_kind;
    int ok;

    file = fopen(M290_GIT_INDEX_TMP, "r");
    if (file == NULL) {
        return 0;
    }

    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (!m290_git_parse_index_line(
                line,
                index_hash,
                sizeof(index_hash),
                path,
                sizeof(path)) ||
            !m290_git_cached_kind(path, &index_kind) ||
            !m290_git_work_kind(path, index_hash, &work_kind) ||
            !m290_git_append_status(index_kind, work_kind, path)) {
            ok = 0;
            break;
        }
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int m290_git_status_missing_index(void)
{
    FILE *file;
    char line[M290_GIT_LINE_MAX];
    char path[512];
    char kind;
    int found;
    int ok;

    file = fopen(M290_GIT_CACHED_TMP, "r");
    if (file == NULL) {
        return 0;
    }

    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (!m290_git_parse_name_status_line(
                line,
                &kind,
                path,
                sizeof(path)) ||
            !m290_git_index_contains(path, &found)) {
            ok = 0;
            break;
        }
        if (!found && !m290_git_append_status(kind, ' ', path)) {
            ok = 0;
            break;
        }
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int m290_git_status_untracked(void)
{
    FILE *file;
    char line[M290_GIT_LINE_MAX];
    char path[512];
    size_t length;
    int ok;

    file = fopen(M290_GIT_OTHER_TMP, "r");
    if (file == NULL) {
        return 0;
    }

    ok = 1;
    while (fgets(line, sizeof(line), file) != NULL) {
        length = strlen(line);
        while (length > 0U &&
               (line[length - 1U] == '\n' ||
                line[length - 1U] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0U || length >= sizeof(path)) {
            ok = 0;
            break;
        }

        (void)memcpy(path, line, length + 1U);
        if (!m263_git_path_ok(path) ||
            !m290_git_append_status('?', '?', path)) {
            ok = 0;
            break;
        }
    }
    if (ferror(file)) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static void m290_git_status_cleanup(void)
{
    llm_git_remove(M290_GIT_META_COM);
    llm_git_remove(M290_GIT_INDEX_TMP);
    llm_git_remove(M290_GIT_CACHED_TMP);
    llm_git_remove(M290_GIT_OTHER_TMP);
    llm_git_remove(M290_GIT_HASH_TMP);
}

static int m290_git_rms_status(void)
{
    int ok;

    m290_git_status_cleanup();
    llm_git_status[0] = '\0';

    ok = m290_git_run_to_file(
             "\"ls-files\" \"--stage\"",
             M290_GIT_INDEX_TMP) &&
         m290_git_run_to_file(
             "\"diff\" \"--cached\" \"--name-status\" "
             "\"--no-renames\" \"--\"",
             M290_GIT_CACHED_TMP) &&
         m290_git_run_to_file(
             "\"ls-files\" \"--others\" \"--exclude-standard\"",
             M290_GIT_OTHER_TMP) &&
         m290_git_status_from_index() &&
         m290_git_status_missing_index() &&
         m290_git_status_untracked();

    m290_git_status_cleanup();
    if (!ok) {
        llm_git_status[0] = '\0';
        return 0;
    }
    return 1;
}
