#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "M289_BUILD_PROFILE.H"

#define M289_LINE_MAX 1024U

#define M289_KEY_LANGUAGE   0x0001UL
#define M289_KEY_EXTENSIONS 0x0002UL
#define M289_KEY_KIND       0x0004UL
#define M289_KEY_COMPILE    0x0008UL
#define M289_KEY_COPTS      0x0010UL
#define M289_KEY_LINK       0x0020UL
#define M289_KEY_LOPTS      0x0040UL
#define M289_KEY_OBJEXT     0x0080UL
#define M289_KEY_EXEEXT     0x0100UL
#define M289_KEY_RUN        0x0200UL
#define M289_KEYS_COMPILED  0x01ffUL
#define M289_KEYS_COMMON    0x0007UL
#define M289_KEYS_COMPILED_ONLY 0x01f8UL
#define M289_KEYS_INTERPRETED (M289_KEYS_COMMON | M289_KEY_RUN)

static void m289_error(char *error, size_t error_size, const char *text)
{
    if (error == NULL || error_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "Unknown build-profile error.";
    }
    (void)strncpy(error, text, error_size - 1U);
    error[error_size - 1U] = '\0';
}

static int m289_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)toupper((unsigned char)*left++);
        b = (unsigned char)toupper((unsigned char)*right++);
        if (a != b) {
            return 0;
        }
    }
    return *left == '\0' && *right == '\0';
}

static char *m289_ltrim(char *text)
{
    if (text == NULL) {
        return NULL;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    return text;
}

static void m289_rtrim(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }
    length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == ' ' || text[length - 1U] == '\t' ||
            text[length - 1U] == '\r' || text[length - 1U] == '\n')) {
        text[--length] = '\0';
    }
}

static int m289_copy(char *target, size_t target_size, const char *value)
{
    size_t length;

    if (target == NULL || target_size == 0U || value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length >= target_size) {
        return 0;
    }
    (void)memcpy(target, value, length + 1U);
    return 1;
}

static int m289_key_store(m289_build_profile *profile,
                          const char *key,
                          const char *value,
                          unsigned long *seen,
                          char *error,
                          size_t error_size)
{
    unsigned long bit;
    char *target;
    size_t target_size;

    bit = 0UL;
    target = NULL;
    target_size = 0U;

    if (strcmp(key, "language") == 0) {
        bit = M289_KEY_LANGUAGE;
        target = profile->language;
        target_size = sizeof(profile->language);
    } else if (strcmp(key, "extensions") == 0) {
        bit = M289_KEY_EXTENSIONS;
        target = profile->extensions;
        target_size = sizeof(profile->extensions);
    } else if (strcmp(key, "kind") == 0) {
        bit = M289_KEY_KIND;
        target = profile->kind;
        target_size = sizeof(profile->kind);
    } else if (strcmp(key, "run_command") == 0) {
        bit = M289_KEY_RUN;
        target = profile->run_command;
        target_size = sizeof(profile->run_command);
    } else if (strcmp(key, "compile_command") == 0) {
        bit = M289_KEY_COMPILE;
        target = profile->compile_command;
        target_size = sizeof(profile->compile_command);
    } else if (strcmp(key, "compile_options") == 0) {
        bit = M289_KEY_COPTS;
        target = profile->compile_options;
        target_size = sizeof(profile->compile_options);
    } else if (strcmp(key, "link_command") == 0) {
        bit = M289_KEY_LINK;
        target = profile->link_command;
        target_size = sizeof(profile->link_command);
    } else if (strcmp(key, "link_options") == 0) {
        bit = M289_KEY_LOPTS;
        target = profile->link_options;
        target_size = sizeof(profile->link_options);
    } else if (strcmp(key, "object_extension") == 0) {
        bit = M289_KEY_OBJEXT;
        target = profile->object_extension;
        target_size = sizeof(profile->object_extension);
    } else if (strcmp(key, "executable_extension") == 0) {
        bit = M289_KEY_EXEEXT;
        target = profile->executable_extension;
        target_size = sizeof(profile->executable_extension);
    } else {
        m289_error(error, error_size, "Unknown build-profile key.");
        return 0;
    }

    if ((*seen & bit) != 0UL) {
        m289_error(error, error_size, "Duplicate build-profile key.");
        return 0;
    }
    if (!m289_copy(target, target_size, value)) {
        m289_error(error, error_size, "Build-profile value is too long.");
        return 0;
    }

    *seen |= bit;
    return 1;
}

static int m289_placeholder_ok(const char *name)
{
    return strcmp(name, "source") == 0 ||
           strcmp(name, "source_name") == 0 ||
           strcmp(name, "object") == 0 ||
           strcmp(name, "executable") == 0 ||
           strcmp(name, "compile_options") == 0 ||
           strcmp(name, "link_options") == 0;
}

static int m289_run_placeholder_ok(const char *name)
{
    return strcmp(name, "source") == 0 ||
           strcmp(name, "source_name") == 0;
}

static int m289_template_ok(const char *text, int run_template)
{
    const char *open;
    const char *close;
    char name[64];
    size_t length;
    int valid;

    if (text == NULL || *text == '\0') {
        return 0;
    }

    open = text;
    while (*open != '\0') {
        if (*open == '}') {
            return 0;
        }
        if (*open != '{') {
            ++open;
            continue;
        }
        close = strchr(open + 1, '}');
        if (close == NULL) {
            return 0;
        }
        length = (size_t)(close - (open + 1));
        if (length == 0U || length >= sizeof(name)) {
            return 0;
        }
        (void)memcpy(name, open + 1, length);
        name[length] = '\0';
        valid = run_template ? m289_run_placeholder_ok(name) :
                               m289_placeholder_ok(name);
        if (!valid) {
            return 0;
        }
        open = close + 1;
    }
    return 1;
}

int m289_profile_load(const char *path,
                      m289_build_profile *profile,
                      char *error,
                      size_t error_size)
{
    FILE *file;
    char line[M289_LINE_MAX];
    char *text;
    char *equals;
    char *key;
    char *value;
    size_t length;
    unsigned long seen;
    int ok;

    if (error != NULL && error_size > 0U) {
        error[0] = '\0';
    }
    if (path == NULL || profile == NULL) {
        m289_error(error, error_size, "Invalid build-profile arguments.");
        return 0;
    }

    (void)memset(profile, 0, sizeof(*profile));
    file = fopen(path, "r");
    if (file == NULL) {
        m289_error(error, error_size, "Unable to open build profile.");
        return 0;
    }

    seen = 0UL;
    ok = 1;
    while (ok && fgets(line, sizeof(line), file) != NULL) {
        length = strlen(line);
        if (length > 0U && line[length - 1U] != '\n' && !feof(file)) {
            m289_error(error, error_size, "Build-profile line is too long.");
            ok = 0;
            break;
        }

        m289_rtrim(line);
        text = m289_ltrim(line);
        if (*text == '\0' || *text == '#') {
            continue;
        }

        equals = strchr(text, '=');
        if (equals == NULL || equals == text) {
            m289_error(error, error_size, "Malformed build-profile line.");
            ok = 0;
            break;
        }
        *equals = '\0';
        key = m289_ltrim(text);
        m289_rtrim(key);
        value = m289_ltrim(equals + 1);
        m289_rtrim(value);

        if (*key == '\0' || !m289_key_store(profile, key, value,
                                             &seen, error, error_size)) {
            ok = 0;
            break;
        }
    }

    if (ferror(file)) {
        m289_error(error, error_size, "Unable to read build profile.");
        ok = 0;
    }
    if (fclose(file) != 0) {
        m289_error(error, error_size, "Unable to close build profile.");
        ok = 0;
    }
    if (!ok) {
        return 0;
    }

    if (m289_equal_ci(profile->kind, "compiled")) {
        if ((seen & M289_KEYS_COMPILED) != M289_KEYS_COMPILED) {
            m289_error(error, error_size, "Missing required build-profile key.");
            return 0;
        }
        if ((seen & M289_KEY_RUN) != 0UL) {
            m289_error(error, error_size,
                       "Build-profile key is not allowed for this kind.");
            return 0;
        }
        if (profile->language[0] == '\0' || profile->extensions[0] == '\0' ||
            profile->compile_command[0] == '\0' ||
            profile->link_command[0] == '\0' ||
            profile->object_extension[0] == '\0' ||
            profile->executable_extension[0] == '\0') {
            m289_error(error, error_size,
                       "Required build-profile value is empty.");
            return 0;
        }
        if (!m289_template_ok(profile->compile_command, 0) ||
            !m289_template_ok(profile->link_command, 0)) {
            m289_error(error, error_size,
                       "Invalid build-profile placeholder.");
            return 0;
        }
        return 1;
    }

    if (m289_equal_ci(profile->kind, "interpreted")) {
        if ((seen & M289_KEYS_INTERPRETED) != M289_KEYS_INTERPRETED) {
            m289_error(error, error_size, "Missing required build-profile key.");
            return 0;
        }
        if ((seen & M289_KEYS_COMPILED_ONLY) != 0UL) {
            m289_error(error, error_size,
                       "Build-profile key is not allowed for this kind.");
            return 0;
        }
        if (profile->language[0] == '\0' || profile->extensions[0] == '\0' ||
            profile->run_command[0] == '\0') {
            m289_error(error, error_size,
                       "Required build-profile value is empty.");
            return 0;
        }
        if (!m289_template_ok(profile->run_command, 1)) {
            m289_error(error, error_size,
                       "Invalid build-profile placeholder.");
            return 0;
        }
        return 1;
    }

    m289_error(error, error_size, "Unsupported build-profile kind.");
    return 0;
}

static int m289_suffix_ci(const char *text, const char *suffix)
{
    size_t text_length;
    size_t suffix_length;
    size_t index;
    unsigned char a;
    unsigned char b;

    text_length = strlen(text);
    suffix_length = strlen(suffix);
    if (suffix_length == 0U || suffix_length > text_length) {
        return 0;
    }
    text += text_length - suffix_length;
    for (index = 0U; index < suffix_length; ++index) {
        a = (unsigned char)toupper((unsigned char)text[index]);
        b = (unsigned char)toupper((unsigned char)suffix[index]);
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int m289_source_allowed(const m289_build_profile *profile,
                               const char *source)
{
    char work[M289_PROFILE_EXTS_MAX];
    char *token;
    char *next;

    if (!m289_copy(work, sizeof(work), profile->extensions)) {
        return 0;
    }

    token = work;
    while (*token != '\0') {
        while (*token == ' ' || *token == '\t' || *token == ',') {
            ++token;
        }
        if (*token == '\0') {
            break;
        }
        next = token;
        while (*next != '\0' && *next != ',') {
            ++next;
        }
        if (*next != '\0') {
            *next++ = '\0';
        }
        m289_rtrim(token);
        if (*token != '\0' && m289_suffix_ci(source, token)) {
            return 1;
        }
        token = next;
    }
    return 0;
}

static const char *m289_last_component(const char *path)
{
    const char *scan;
    const char *base;

    base = path;
    for (scan = path; *scan != '\0'; ++scan) {
        if (*scan == '/' || *scan == '\\' || *scan == ']' ||
            *scan == '>' || *scan == ':') {
            base = scan + 1;
        }
    }
    return base;
}

static int m289_source_name(const char *source,
                            char *source_name,
                            size_t source_name_size)
{
    const char *base;
    const char *dot;
    const char *scan;
    size_t length;

    base = m289_last_component(source);
    dot = NULL;
    for (scan = base; *scan != '\0'; ++scan) {
        if (*scan == '.') {
            dot = scan;
        }
    }
    if (dot == NULL || dot == base) {
        return 0;
    }
    length = (size_t)(dot - base);
    if (length + 1U > source_name_size) {
        return 0;
    }
    (void)memcpy(source_name, base, length);
    source_name[length] = '\0';
    return 1;
}

static int m289_derive_names(const char *source,
                             const char *object_extension,
                             const char *executable_extension,
                             char *source_name,
                             size_t source_name_size,
                             char *object,
                             size_t object_size,
                             char *executable,
                             size_t executable_size)
{
    const char *base;
    const char *dot;
    const char *scan;
    size_t stem_length;
    size_t base_stem_length;

    base = m289_last_component(source);
    dot = NULL;
    for (scan = base; *scan != '\0'; ++scan) {
        if (*scan == '.') {
            dot = scan;
        }
    }
    if (dot == NULL || dot == base) {
        return 0;
    }

    stem_length = (size_t)(dot - source);
    base_stem_length = (size_t)(dot - base);
    if (base_stem_length + 1U > source_name_size ||
        stem_length + strlen(object_extension) + 1U > object_size ||
        stem_length + strlen(executable_extension) + 1U > executable_size) {
        return 0;
    }

    (void)memcpy(source_name, base, base_stem_length);
    source_name[base_stem_length] = '\0';
    (void)memcpy(object, source, stem_length);
    object[stem_length] = '\0';
    (void)strcat(object, object_extension);
    (void)memcpy(executable, source, stem_length);
    executable[stem_length] = '\0';
    (void)strcat(executable, executable_extension);
    return 1;
}

static int m289_append(char *output,
                       size_t output_size,
                       size_t *used,
                       const char *text,
                       size_t length)
{
    if (*used + length >= output_size) {
        return 0;
    }
    if (length > 0U) {
        (void)memcpy(output + *used, text, length);
        *used += length;
    }
    output[*used] = '\0';
    return 1;
}

static void m289_compact_spaces(char *text)
{
    char *readp;
    char *writep;
    int quoted;
    int pending_space;

    readp = text;
    writep = text;
    quoted = 0;
    pending_space = 0;

    while (*readp != '\0') {
        if (*readp == '"') {
            if (pending_space && writep != text) {
                *writep++ = ' ';
            }
            pending_space = 0;
            *writep++ = *readp++;
            quoted = !quoted;
            continue;
        }
        if (!quoted && (*readp == ' ' || *readp == '\t')) {
            pending_space = 1;
            ++readp;
            continue;
        }
        if (pending_space && writep != text) {
            *writep++ = ' ';
        }
        pending_space = 0;
        *writep++ = *readp++;
    }
    *writep = '\0';
}

static const char *m289_replacement(const char *name,
                                    const char *source,
                                    const char *source_name,
                                    const char *object,
                                    const char *executable,
                                    const m289_build_profile *profile)
{
    if (strcmp(name, "source") == 0) return source;
    if (strcmp(name, "source_name") == 0) return source_name;
    if (strcmp(name, "object") == 0) return object;
    if (strcmp(name, "executable") == 0) return executable;
    if (strcmp(name, "compile_options") == 0) return profile->compile_options;
    if (strcmp(name, "link_options") == 0) return profile->link_options;
    return NULL;
}

static int m289_expand(const char *template_text,
                       const char *source,
                       const char *source_name,
                       const char *object,
                       const char *executable,
                       const m289_build_profile *profile,
                       char *output,
                       size_t output_size)
{
    const char *scan;
    const char *close;
    const char *replacement;
    char name[64];
    size_t used;
    size_t length;

    if (output == NULL || output_size == 0U) {
        return 0;
    }
    output[0] = '\0';
    used = 0U;
    scan = template_text;

    while (*scan != '\0') {
        if (*scan != '{') {
            if (!m289_append(output, output_size, &used, scan, 1U)) {
                return 0;
            }
            ++scan;
            continue;
        }
        close = strchr(scan + 1, '}');
        if (close == NULL) {
            return 0;
        }
        length = (size_t)(close - (scan + 1));
        if (length == 0U || length >= sizeof(name)) {
            return 0;
        }
        (void)memcpy(name, scan + 1, length);
        name[length] = '\0';
        replacement = m289_replacement(name, source, source_name,
                                       object, executable, profile);
        if (replacement == NULL ||
            !m289_append(output, output_size, &used,
                         replacement, strlen(replacement))) {
            return 0;
        }
        scan = close + 1;
    }

    m289_compact_spaces(output);
    return output[0] != '\0';
}

int m289_profile_commands(const m289_build_profile *profile,
                          const char *source,
                          char *compile_command,
                          size_t compile_size,
                          char *link_command,
                          size_t link_size,
                          char *error,
                          size_t error_size)
{
    char source_name[M289_PROFILE_PATH_MAX];
    char object[M289_PROFILE_PATH_MAX];
    char executable[M289_PROFILE_PATH_MAX];

    if (error != NULL && error_size > 0U) {
        error[0] = '\0';
    }
    if (profile == NULL || source == NULL || *source == '\0' ||
        compile_command == NULL || link_command == NULL) {
        m289_error(error, error_size, "Invalid command-resolution arguments.");
        return 0;
    }
    if (!m289_equal_ci(profile->kind, "compiled")) {
        m289_error(error, error_size, "Profile is not a compiled toolchain.");
        return 0;
    }
    if (!m289_source_allowed(profile, source)) {
        m289_error(error, error_size,
                   "Source extension is not allowed by profile.");
        return 0;
    }
    if (!m289_derive_names(source,
                           profile->object_extension,
                           profile->executable_extension,
                           source_name, sizeof(source_name),
                           object, sizeof(object),
                           executable, sizeof(executable))) {
        m289_error(error, error_size, "Unable to derive build output names.");
        return 0;
    }
    if (!m289_expand(profile->compile_command, source, source_name,
                     object, executable, profile,
                     compile_command, compile_size) ||
        !m289_expand(profile->link_command, source, source_name,
                     object, executable, profile,
                     link_command, link_size)) {
        m289_error(error, error_size, "Resolved build command is too long.");
        return 0;
    }
    return 1;
}

int m289_profile_run_command(const m289_build_profile *profile,
                             const char *source,
                             char *run_command,
                             size_t run_size,
                             char *error,
                             size_t error_size)
{
    char source_name[M289_PROFILE_PATH_MAX];

    if (error != NULL && error_size > 0U) {
        error[0] = '\0';
    }
    if (profile == NULL || source == NULL || *source == '\0' ||
        run_command == NULL || run_size == 0U) {
        m289_error(error, error_size, "Invalid command-resolution arguments.");
        return 0;
    }
    if (!m289_equal_ci(profile->kind, "interpreted")) {
        m289_error(error, error_size, "Profile is not an interpreted toolchain.");
        return 0;
    }
    if (!m289_source_allowed(profile, source)) {
        m289_error(error, error_size,
                   "Source extension is not allowed by profile.");
        return 0;
    }
    if (!m289_source_name(source, source_name, sizeof(source_name))) {
        m289_error(error, error_size, "Unable to derive source name.");
        return 0;
    }
    if (!m289_expand(profile->run_command, source, source_name,
                     "", "", profile, run_command, run_size)) {
        m289_error(error, error_size, "Resolved run command is too long.");
        return 0;
    }
    return 1;
}
