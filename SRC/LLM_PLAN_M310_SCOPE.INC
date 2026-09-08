static int llm_m310_scope_add(
    plan_scope_file files[],
    unsigned int *count,
    const char *path,
    int expect_missing)
{
    struct stat file_status;
    int index;
    int status;

    if (files == NULL ||
        count == NULL ||
        path == NULL ||
        !plan_path_safe(path) ||
        strlen(path) >= LLM_PLAN_PATH_SIZE) {
        return 0;
    }

    index = plan_scope_find(files, *count, path);

    if (index >= 0) {
        if (files[index].expect_missing) {
            /* A path created earlier may be modified later in the same plan. */
            return expect_missing ? 0 : 1;
        }

        /* Existing paths may have several ordered replacements, but cannot
         * later become create_file targets in the same plan. */
        return expect_missing ? 0 : 1;
    }

    if (*count >= LLM_PLAN_MAX_FILES) {
        return 0;
    }

    errno = 0;
    status = stat(path, &file_status);

    if (expect_missing) {
        if (status == 0 || errno != ENOENT) {
            return 0;
        }
    } else if (status != 0) {
        return 0;
    }

    (void)strcpy(files[*count].path, path);
    files[*count].expect_missing = expect_missing;
    ++(*count);
    return 1;
}

static int llm_m310_collect_ops(
    const char *plan_text,
    plan_scope_file files[],
    unsigned int *count)
{
    const char *section;
    const char *cursor;
    const char *line_start;
    int in_operation;
    int in_payload;
    char type[32];
    char path[LLM_PLAN_PATH_SIZE];
    char target[LLM_PLAN_PATH_SIZE];

    section = strstr(plan_text, "operation_count=");

    if (section == NULL) {
        return -1;
    }

    cursor = section;
    line_start = section;
    in_operation = 0;
    in_payload = 0;
    type[0] = '\0';
    path[0] = '\0';
    target[0] = '\0';

    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            const char *end;
            size_t length;
            int result;

            end = cursor;
            if (end > line_start && end[-1] == '\r') {
                --end;
            }
            length = (size_t)(end - line_start);

            if (length == 15U &&
                memcmp(line_start, "BEGIN_OPERATION", 15U) == 0) {
                if (in_operation) {
                    return 0;
                }
                in_operation = 1;
                in_payload = 0;
                type[0] = '\0';
                path[0] = '\0';
                target[0] = '\0';
            } else if (length == 13U &&
                       memcmp(line_start, "END_OPERATION", 13U) == 0) {
                int missing;

                if (!in_operation || in_payload ||
                    type[0] == '\0' || path[0] == '\0') {
                    return 0;
                }

                if (strcmp(type, "create_file") == 0) {
                    missing = 1;
                } else if (strcmp(type, "replace_block") == 0 ||
                           strcmp(type, "delete_file") == 0 ||
                           strcmp(type, "rename_file") == 0 ||
                           strcmp(type, "move_file") == 0) {
                    missing = 0;
                } else {
                    return 0;
                }

                if (!llm_m310_scope_add(files, count, path, missing)) {
                    return 0;
                }

                if (target[0] != '\0') {
                    if ((strcmp(type, "rename_file") != 0 &&
                         strcmp(type, "move_file") != 0) ||
                        !llm_m310_scope_add(files, count, target, 1)) {
                        return 0;
                    }
                }

                in_operation = 0;
            } else if (in_operation) {
                if (length == 14U &&
                    (memcmp(line_start, "BEGIN_OLD_TEXT", 14U) == 0 ||
                     memcmp(line_start, "BEGIN_NEW_TEXT", 14U) == 0)) {
                    if (in_payload) {
                        return 0;
                    }
                    in_payload = 1;
                } else if (length == 12U &&
                           (memcmp(line_start, "END_OLD_TEXT", 12U) == 0 ||
                            memcmp(line_start, "END_NEW_TEXT", 12U) == 0)) {
                    if (!in_payload) {
                        return 0;
                    }
                    in_payload = 0;
                } else if (!in_payload) {
                    result = plan_line_value(
                        line_start, end, "type=",
                        type, sizeof(type));
                    if (result < 0) {
                        return 0;
                    }
                    if (result == 0) {
                        result = plan_line_value(
                            line_start, end, "path=",
                            path, sizeof(path));
                    }
                    if (result < 0) {
                        return 0;
                    }
                    if (result == 0) {
                        result = plan_line_value(
                            line_start, end, "target_path=",
                            target, sizeof(target));
                    }
                    if (result < 0) {
                        return 0;
                    }
                }
            }

            if (*cursor == '\0') {
                break;
            }

            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    return !in_operation && !in_payload;
}

static int llm_m310_collect_paths(
    const char *plan_text,
    plan_scope_file files[],
    unsigned int *count_out)
{
    unsigned int count;
    int result;

    if (plan_text == NULL || files == NULL || count_out == NULL) {
        return 0;
    }

    count = 0U;
    result = llm_m310_collect_ops(plan_text, files, &count);

    if (result == 1) {
        *count_out = count;
        return 1;
    }

    if (result == 0) {
        return 0;
    }

    /* Preserve the established narrative-only/advisory scope parser. */
    return plan_collect_paths(plan_text, files, count_out);
}

#define plan_collect_paths llm_m310_collect_paths
