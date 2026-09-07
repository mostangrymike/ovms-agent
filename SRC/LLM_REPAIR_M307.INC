#define M307_LINE_SIZE 4096U
#define M307_TEXT_SIZE 32768U

static int m307_ci_equal_n(const char *left,
                           const char *right,
                           size_t length)
{
    size_t index;

    if (left == NULL || right == NULL) {
        return 0;
    }

    for (index = 0U; index < length; ++index) {
        if (tolower((unsigned char)left[index]) !=
            tolower((unsigned char)right[index])) {
            return 0;
        }
    }

    return 1;
}

static int m307_ci_equal(const char *left, const char *right)
{
    size_t left_length;
    size_t right_length;

    if (left == NULL || right == NULL) {
        return 0;
    }

    left_length = strlen(left);
    right_length = strlen(right);

    return left_length == right_length &&
           m307_ci_equal_n(left, right, left_length);
}

static int m307_diag_start(const char *text)
{
    const char *first;
    const char *second;

    if (text == NULL || text[0] != '%' || text[1] == '\0') {
        return 0;
    }

    first = strchr(text + 1, '-');
    if (first == NULL || first == text + 1 || first[1] == '\0') {
        return 0;
    }

    second = strchr(first + 1, '-');
    return second != NULL && second > first + 1;
}

static int m307_copy_line(char *destination,
                          size_t destination_size,
                          const char *source)
{
    size_t length;

    if (destination == NULL || destination_size == 0U || source == NULL) {
        return 0;
    }

    length = strlen(source);
    while (length > 0U &&
           (source[length - 1U] == '\n' || source[length - 1U] == '\r')) {
        --length;
    }

    if (length == 0U || length >= destination_size) {
        return 0;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
    return 1;
}

static int m307_root_path(const char *root,
                          const char *filespec,
                          char *output,
                          size_t output_size)
{
    const char *root_open;
    const char *root_close;
    const char *file_open;
    const char *file_close;
    const char *filename;
    const char *suffix;
    const char *position;
    size_t root_device_length;
    size_t file_device_length;
    size_t root_dir_length;
    size_t file_dir_length;
    size_t used;

    if (root == NULL || filespec == NULL || output == NULL ||
        output_size == 0U) {
        return 0;
    }

    root_open = strchr(root, '[');
    root_close = root_open != NULL ? strchr(root_open, ']') : NULL;
    file_open = strchr(filespec, '[');
    file_close = file_open != NULL ? strchr(file_open, ']') : NULL;

    if (root_open == NULL || root_close == NULL ||
        file_open == NULL || file_close == NULL ||
        root_close <= root_open + 1 || file_close <= file_open + 1) {
        return 0;
    }

    root_device_length = (size_t)(root_open - root);
    file_device_length = (size_t)(file_open - filespec);
    if (root_device_length != 0U && file_device_length != 0U) {
        if (root_device_length != file_device_length ||
            !m307_ci_equal_n(root, filespec, root_device_length)) {
            return 0;
        }
    }

    root_dir_length = (size_t)(root_close - root_open - 1);
    file_dir_length = (size_t)(file_close - file_open - 1);
    if (file_dir_length < root_dir_length ||
        !m307_ci_equal_n(root_open + 1, file_open + 1,
                         root_dir_length)) {
        return 0;
    }

    if (file_dir_length > root_dir_length &&
        file_open[1 + root_dir_length] != '.') {
        return 0;
    }

    filename = file_close + 1;
    if (*filename == '\0') {
        return 0;
    }

    suffix = file_open + 1 + root_dir_length;
    if (*suffix == '.') {
        ++suffix;
    }

    used = 0U;
    position = suffix;
    while (position < file_close) {
        char ch;

        ch = *position++;
        if (ch == '.') {
            ch = '/';
        } else if (ch == '[' || ch == ']' || ch == ':' ||
                   ch == ';' || ch == '\\') {
            return 0;
        }

        if (used + 1U >= output_size) {
            return 0;
        }
        output[used++] = (char)tolower((unsigned char)ch);
    }

    if (used != 0U) {
        if (output[used - 1U] != '/') {
            if (used + 1U >= output_size) {
                return 0;
            }
            output[used++] = '/';
        }
    }

    position = filename;
    while (*position != '\0' && *position != ';') {
        char ch;

        ch = *position++;
        if (ch == '/' || ch == '\\' || ch == '[' || ch == ']' ||
            ch == ':') {
            return 0;
        }
        if (used + 1U >= output_size) {
            return 0;
        }
        output[used++] = (char)tolower((unsigned char)ch);
    }

    if (used == 0U || output[used - 1U] == '/' ||
        used >= output_size) {
        return 0;
    }

    output[used] = '\0';
    if (strstr(output, "..") != NULL || output[0] == '/') {
        return 0;
    }

    return 1;
}

int llm_m307_diag_file(const char *build_path,
                       const char *project_root,
                       char *diagnostic,
                       size_t diagnostic_size,
                       char *project_path,
                       size_t project_path_size,
                       int *target_line)
{
    FILE *file;
    char line[M307_LINE_SIZE];
    char pending[M307_LINE_SIZE];
    char filespec[1024];
    int line_number;

    if (build_path == NULL || project_root == NULL ||
        diagnostic == NULL || diagnostic_size == 0U ||
        project_path == NULL || project_path_size == 0U ||
        target_line == NULL) {
        return 0;
    }

    file = fopen(build_path, "r");
    if (file == NULL) {
        return 0;
    }

    pending[0] = '\0';
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text;
        char *location;
        char *file_marker;
        char *value;
        size_t used;

        text = line;
        while (*text == ' ' || *text == '\t') {
            ++text;
        }

        if (m307_diag_start(text)) {
            (void)m307_copy_line(pending, sizeof(pending), text);
            continue;
        }

        if (pending[0] == '\0') {
            continue;
        }

        location = strstr(text, "at line number ");
        if (location == NULL) {
            continue;
        }
        file_marker = strstr(location, " in file ");
        if (file_marker == NULL ||
            sscanf(location, "at line number %d", &line_number) != 1 ||
            line_number < 1) {
            continue;
        }

        value = file_marker + strlen(" in file ");
        while (*value == ' ' || *value == '\t') {
            ++value;
        }

        used = 0U;
        while (value[used] != '\0' && value[used] != '\n' &&
               value[used] != '\r' && used + 1U < sizeof(filespec)) {
            filespec[used] = value[used];
            ++used;
        }
        filespec[used] = '\0';

        if (used == 0U ||
            !m307_root_path(project_root, filespec,
                            project_path, project_path_size) ||
            !m307_copy_line(diagnostic, diagnostic_size, pending)) {
            pending[0] = '\0';
            continue;
        }

        *target_line = line_number;
        (void)fclose(file);
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int m307_read_block(FILE *file,
                           const char *end_marker,
                           char *destination,
                           size_t destination_size,
                           int allow_empty)
{
    char line[M307_LINE_SIZE];
    size_t used;
    size_t line_length;
    size_t marker_length;
    const char *content;

    if (file == NULL || end_marker == NULL || destination == NULL ||
        destination_size == 0U) {
        return 0;
    }

    used = 0U;
    destination[0] = '\0';
    marker_length = strlen(end_marker);

    while (fgets(line, sizeof(line), file) != NULL) {
        line_length = strlen(line);
        while (line_length > 0U &&
               (line[line_length - 1U] == '\n' ||
                line[line_length - 1U] == '\r')) {
            --line_length;
        }

        if (line_length == marker_length &&
            memcmp(line, end_marker, marker_length) == 0) {
            if (used > 0U && destination[used - 1U] == '\n') {
                --used;
            }
            destination[used] = '\0';
            return allow_empty || used > 0U;
        }

        content = line;
        if (line_length == marker_length + 1U && line[0] == '\\' &&
            memcmp(line + 1, end_marker, marker_length) == 0) {
            content = line + 1;
            --line_length;
        }

        if (used + line_length + 1U >= destination_size) {
            return 0;
        }
        if (line_length > 0U) {
            memcpy(destination + used, content, line_length);
            used += line_length;
        }
        destination[used++] = '\n';
        destination[used] = '\0';
    }

    return 0;
}

int llm_m307_failed_op(const char *operations_path,
                       const char *target_path,
                       char *old_text,
                       size_t old_size,
                       char *new_text,
                       size_t new_size)
{
    FILE *file;
    char line[M307_LINE_SIZE];
    char op_path[512];
    int in_operation;
    int saw_type;
    int saw_path;
    int saw_old;
    int saw_new;

    if (operations_path == NULL || target_path == NULL ||
        old_text == NULL || old_size == 0U ||
        new_text == NULL || new_size == 0U) {
        return 0;
    }

    file = fopen(operations_path, "r");
    if (file == NULL) {
        return 0;
    }

    in_operation = 0;
    saw_type = 0;
    saw_path = 0;
    saw_old = 0;
    saw_new = 0;
    op_path[0] = '\0';

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strcmp(line, "BEGIN_OPERATION\n") == 0 ||
            strcmp(line, "BEGIN_OPERATION\r\n") == 0) {
            if (in_operation) {
                (void)fclose(file);
                return 0;
            }
            in_operation = 1;
            saw_type = 0;
            saw_path = 0;
            saw_old = 0;
            saw_new = 0;
            op_path[0] = '\0';
            old_text[0] = '\0';
            new_text[0] = '\0';
            continue;
        }

        if (!in_operation) {
            continue;
        }

        if (strncmp(line, "type=", 5U) == 0) {
            char value[64];
            if (saw_type || !m307_copy_line(value, sizeof(value), line + 5) ||
                strcmp(value, "replace_block") != 0) {
                (void)fclose(file);
                return 0;
            }
            saw_type = 1;
            continue;
        }

        if (strncmp(line, "path=", 5U) == 0) {
            if (saw_path ||
                !m307_copy_line(op_path, sizeof(op_path), line + 5)) {
                (void)fclose(file);
                return 0;
            }
            saw_path = 1;
            continue;
        }

        if (strcmp(line, "BEGIN_OLD_TEXT\n") == 0 ||
            strcmp(line, "BEGIN_OLD_TEXT\r\n") == 0) {
            if (saw_old || !m307_read_block(file, "END_OLD_TEXT",
                                             old_text, old_size, 0)) {
                (void)fclose(file);
                return 0;
            }
            saw_old = 1;
            continue;
        }

        if (strcmp(line, "BEGIN_NEW_TEXT\n") == 0 ||
            strcmp(line, "BEGIN_NEW_TEXT\r\n") == 0) {
            if (saw_new || !m307_read_block(file, "END_NEW_TEXT",
                                             new_text, new_size, 1)) {
                (void)fclose(file);
                return 0;
            }
            saw_new = 1;
            continue;
        }

        if (strcmp(line, "END_OPERATION\n") == 0 ||
            strcmp(line, "END_OPERATION\r\n") == 0) {
            if (!saw_type || !saw_path || !saw_old || !saw_new) {
                (void)fclose(file);
                return 0;
            }
            if (m307_ci_equal(op_path, target_path)) {
                (void)fclose(file);
                return 1;
            }
            in_operation = 0;
            continue;
        }

        if (line[0] != '\n' && line[0] != '\r' && line[0] != '\0') {
            (void)fclose(file);
            return 0;
        }
    }

    (void)fclose(file);
    return 0;
}

void llm_repair_plan(agent_state *state)
{
    char diagnostic[1024];
    char project_path[512];
    char context[8192];
    char target_text[M307_LINE_SIZE];
    char failed_old[M307_TEXT_SIZE];
    char failed_new[M307_TEXT_SIZE];
    char prompt[65536];
    FILE *source;
    char line[M307_LINE_SIZE];
    int target_line;
    int current_line;
    int start_line;
    int end_line;
    int saw_target;
    size_t context_used;
    int written;
    const char *instructions;

    if (state == NULL || state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!llm_m307_diag_file("OVMS_AGENT_FAILED_BUILD.TXT",
                            state->project_root,
                            diagnostic, sizeof(diagnostic),
                            project_path, sizeof(project_path),
                            &target_line)) {
        (void)puts("No actionable in-project native compiler diagnostic was found.");
        return;
    }

    (void)printf("First actionable diagnostic:\n%s\n", diagnostic);
    source = fopen(project_path, "r");
    if (source == NULL) {
        (void)printf("Could not open source file '%s': %s; refusing.\n",
                     project_path, strerror(errno));
        return;
    }

    start_line = target_line - 2;
    if (start_line < 1) {
        start_line = 1;
    }
    end_line = target_line + 2;
    current_line = 1;
    saw_target = 0;
    context_used = 0U;
    context[0] = '\0';
    target_text[0] = '\0';

    (void)printf("Referenced file: %s\n", project_path);
    (void)printf("Referenced line: %d\n", target_line);
    (void)printf("Context (lines %d..%d):\n", start_line, end_line);

    while (fgets(line, sizeof(line), source) != NULL) {
        if (current_line >= start_line && current_line <= end_line) {
            size_t remaining;
            int count;

            (void)printf("%6d: %s", current_line, line);
            if (strchr(line, '\n') == NULL) {
                (void)putchar('\n');
            }

            remaining = sizeof(context) - context_used;
            count = snprintf(context + context_used, remaining,
                             "%6d: %s%s",
                             current_line, line,
                             strchr(line, '\n') == NULL ? "\n" : "");
            if (count < 0 || (size_t)count >= remaining) {
                (void)fclose(source);
                (void)puts("Repair context is too large; refusing.");
                return;
            }
            context_used += (size_t)count;

            if (current_line == target_line) {
                if (!m307_copy_line(target_text, sizeof(target_text), line)) {
                    (void)fclose(source);
                    (void)puts("Referenced source line is empty or too long; refusing.");
                    return;
                }
                saw_target = 1;
            }
        }
        if (current_line > end_line) {
            break;
        }
        ++current_line;
    }
    (void)fclose(source);

    if (!saw_target) {
        (void)printf("Referenced line %d not found in '%s'; refusing.\n",
                     target_line, project_path);
        return;
    }

    if (!llm_m307_failed_op("OVMS_AGENT_FAILED_OPERATIONS.TXT",
                            project_path,
                            failed_old, sizeof(failed_old),
                            failed_new, sizeof(failed_new))) {
        (void)printf("No valid matching structured failed operation for '%s' was found; refusing.\n",
                     project_path);
        return;
    }

    instructions =
        "\n\nINSTRUCTIONS:\n"
        "You MUST output a single JSON object (not free text).\n"
        "The object MUST contain integer operation_count=1 and an operations array containing exactly one operation.\n"
        "The operation MUST be replace_text for the referenced project-relative path.\n"
        "The operation object MUST be encoded on one physical line and contain exactly op, path, old_text, and new_text.\n"
        "old_text MUST exactly equal REFERENCED_SOURCE_LINE below, excluding its terminating newline.\n"
        "new_text MUST be one replacement source line with no embedded newline.\n"
        "The FAILED_OPERATION blocks are exact transaction evidence and may be multiline; use them as context but do not copy the whole block into old_text.\n"
        "Do not replace the whole file and do not include commentary or extra operations.\n";

    written = snprintf(
        prompt, sizeof(prompt),
        "%s\n%s\n%d\n%s\nREFERENCED_SOURCE_LINE:\n%s\n"
        "FAILED_OPERATION_OLD_TEXT:\n%s\n"
        "FAILED_OPERATION_NEW_TEXT:\n%s\n%s",
        diagnostic, project_path, target_line, context, target_text,
        failed_old, failed_new, instructions);
    if (written < 0 || (size_t)written >= sizeof(prompt)) {
        (void)puts("Repair prompt is too large; refusing.");
        return;
    }

    llm_agent_plan(state, prompt);
}
