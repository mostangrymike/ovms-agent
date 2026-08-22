static char *execute_apply_replacement_to_content(
    const char *content,
    const llm_saved_operation *operation)
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
    const llm_saved_operation *operations,
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

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path)) {
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

static int execute_delete_precheck(
    const llm_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int delete_count;

    delete_count = 0U;

    for (index = 0U; index < operation_count; ++index) {
        if (operations[index].is_block != 2) {
            continue;
        }

        ++delete_count;

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan delete_file refused for path: %s\n",
                operations[index].path
            );
            return 0;
        }
    }

    if (delete_count > 0U &&
        (delete_count != 1U || operation_count != 1U)) {
        (void)puts(
            "M252 delete_file plans must contain exactly one operation."
        );
        return 0;
    }

    return 1;
}

static int execute_rename_precheck(
    const llm_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int rename_count;

    rename_count = 0U;

    for (index = 0U; index < operation_count; ++index) {
        if (operations[index].is_block != 3) {
            continue;
        }

        ++rename_count;

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path) ||
            !llm_path_is_safe(operations[index].old_text) ||
            llm_path_is_sensitive(operations[index].old_text)) {
            (void)puts(
                "Saved plan rename_file refused for unsafe source or target path."
            );
            return 0;
        }
    }

    if (rename_count > 0U &&
        (rename_count != 1U || operation_count != 1U)) {
        (void)puts(
            "M252 rename_file plans must contain exactly one operation."
        );
        return 0;
    }

    return 1;
}

static int execute_move_precheck(
    const llm_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int move_count;

    move_count = 0U;

    for (index = 0U; index < operation_count; ++index) {
        if (operations[index].is_block != 4) {
            continue;
        }

        ++move_count;

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path) ||
            !llm_path_is_safe(operations[index].old_text) ||
            llm_path_is_sensitive(operations[index].old_text)) {
            (void)puts(
                "Saved plan move_file refused for unsafe source or target path."
            );
            return 0;
        }
    }

    if (move_count > 0U &&
        (move_count != 1U || operation_count != 1U)) {
        (void)puts(
            "M252 move_file plans must contain exactly one operation."
        );
        return 0;
    }

    return 1;
}

static int execute_stage_operations_chained(
    edit_txn *transaction,
    const llm_saved_operation *operations,
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
            operation_count) ||
        !execute_delete_precheck(
            operations,
            operation_count) ||
        !execute_rename_precheck(
            operations,
            operation_count) ||
        !execute_move_precheck(
            operations,
            operation_count)) {
        return 0;
    }

    for (index = 0U;
         index < operation_count;
         ++index) {
        char *working_content;
        int already_processed;

        if (operations[index].is_block == 2) {
            if (!edit_txn_add_delete(
                    transaction,
                    operations[index].path)) {
                (void)printf(
                    "Unable to stage delete_file operation for %s. "
                    "Use an existing explicit numeric file version such as ;1.\n",
                    operations[index].path
                );
                return 0;
            }

            continue;
        }

        if (operations[index].is_block == 3) {
            if (!edit_txn_add_rename(
                    transaction,
                    operations[index].path,
                    operations[index].old_text)) {
                (void)puts(
                    "Unable to stage rename_file operation. Use existing source "
                    "and absent target explicit numeric versions in the same directory."
                );
                return 0;
            }

            continue;
        }

        if (operations[index].is_block == 4) {
            if (!edit_txn_add_move(
                    transaction,
                    operations[index].path,
                    operations[index].old_text)) {
                (void)puts(
                    "Unable to stage move_file operation. Use existing source and "
                    "absent target explicit numeric versions in different directories."
                );
                return 0;
            }

            continue;
        }

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

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan execution refused for path: %s\n",
                operations[index].path
            );
            return 0;
        }

        working_content =
            llm_read_text_file(operations[index].path);

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
