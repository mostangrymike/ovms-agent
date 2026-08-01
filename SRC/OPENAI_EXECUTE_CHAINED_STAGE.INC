static char *execute_apply_replacement_to_content(
    const char *content,
    const openai_saved_operation *operation)
{
    const char *match;
    const char *second_match;
    char *replacement;
    size_t prefix_length;
    size_t old_length;
    size_t new_length;
    size_t content_length;
    size_t result_length;

    if (content == NULL ||
        operation == NULL ||
        operation->old_text[0] == '\0') {
        return NULL;
    }

    match = strstr(content, operation->old_text);

    if (match == NULL) {
        (void)printf(
            "Saved operation old_text was not found in %s.\n",
            operation->path
        );
        return NULL;
    }

    old_length = strlen(operation->old_text);
    second_match = strstr(match + old_length, operation->old_text);

    if (second_match != NULL) {
        (void)printf(
            "Saved operation old_text is not unique in %s.\n",
            operation->path
        );
        return NULL;
    }

    prefix_length = (size_t)(match - content);
    new_length = strlen(operation->new_text);
    content_length = strlen(content);
    result_length =
        prefix_length +
        new_length +
        content_length -
        prefix_length -
        old_length;

    replacement = (char *)malloc(result_length + 1U);

    if (replacement == NULL) {
        return NULL;
    }

    if (prefix_length > 0U) {
        (void)memcpy(
            replacement,
            content,
            prefix_length
        );
    }

    if (new_length > 0U) {
        (void)memcpy(
            replacement + prefix_length,
            operation->new_text,
            new_length
        );
    }

    (void)memcpy(
        replacement + prefix_length + new_length,
        match + old_length,
        content_length -
            prefix_length -
            old_length
    );

    replacement[result_length] = '\0';
    return replacement;
}

static int execute_target_exists(
    const char *path)
{
    FILE *file;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}

static int execute_create_precheck(
    const openai_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int other_index;

    for (index = 0U;
         index < operation_count;
         ++index) {
        if (!operations[index].is_create) {
            continue;
        }

        if (!openai_path_is_safe(operations[index].path) ||
            openai_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan create_file refused for path: %s\n",
                operations[index].path
            );
            return 0;
        }

        if (execute_target_exists(
                operations[index].path)) {
            (void)printf(
                "Saved plan create_file target already exists: %s\n",
                operations[index].path
            );
            return 0;
        }

        for (other_index = 0U;
             other_index < operation_count;
             ++other_index) {
            if (other_index != index &&
                strcmp(
                    operations[index].path,
                    operations[other_index].path) == 0) {
                (void)printf(
                    "Saved plan create_file path is reused: %s\n",
                    operations[index].path
                );
                return 0;
            }
        }
    }

    return 1;
}

static int execute_stage_operations_chained(
    edit_txn *transaction,
    const openai_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int prior_index;
    unsigned int apply_index;

    if (transaction == NULL ||
        operations == NULL ||
        operation_count == 0U) {
        return 0;
    }

    if (!execute_create_precheck(
            operations,
            operation_count)) {
        return 0;
    }

    for (index = 0U;
         index < operation_count;
         ++index) {
        char *working_content;
        int already_processed;

        if (operations[index].is_create) {
            if (!edit_txn_add(
                    transaction,
                    operations[index].path,
                    operations[index].new_text)) {
                (void)printf(
                    "Unable to stage create_file operation for %s.\n",
                    operations[index].path
                );
                return 0;
            }

            continue;
        }

        already_processed = 0;

        for (prior_index = 0U;
             prior_index < index;
             ++prior_index) {
            if (strcmp(
                    operations[prior_index].path,
                    operations[index].path) == 0) {
                already_processed = 1;
                break;
            }
        }

        if (already_processed) {
            continue;
        }

        if (!openai_path_is_safe(operations[index].path) ||
            openai_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan execution refused for path: %s\n",
                operations[index].path
            );
            return 0;
        }

        working_content =
            openai_read_text_file(operations[index].path);

        if (working_content == NULL) {
            (void)printf(
                "Unable to read saved operation target: %s\n",
                operations[index].path
            );
            return 0;
        }

        for (apply_index = index;
             apply_index < operation_count;
             ++apply_index) {
            char *next_content;

            if (strcmp(
                    operations[apply_index].path,
                    operations[index].path) != 0) {
                continue;
            }

            next_content =
                execute_apply_replacement_to_content(
                    working_content,
                    &operations[apply_index]
                );

            if (next_content == NULL) {
                free(working_content);
                return 0;
            }

            free(working_content);
            working_content = next_content;
        }

        if (!edit_txn_add(
                transaction,
                operations[index].path,
                working_content)) {
            (void)printf(
                "Unable to stage saved operations for %s. "
                "The path may be unreadable, unsafe, "
                "or impossible to snapshot.\n",
                operations[index].path
            );
            free(working_content);
            return 0;
        }

        free(working_content);
    }

    return 1;
}
