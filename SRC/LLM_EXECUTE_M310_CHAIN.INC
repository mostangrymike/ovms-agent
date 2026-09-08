static int llm_m310_create_precheck(
    const llm_saved_operation *operations,
    unsigned int operation_count)
{
    unsigned int index;
    unsigned int other_index;

    for (index = 0U; index < operation_count; ++index) {
        if (!operations[index].is_create) {
            continue;
        }

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan create_file refused for path: %s\n",
                operations[index].path);
            return 0;
        }

        if (execute_target_exists(operations[index].path)) {
            (void)printf(
                "Saved plan create_file target already exists: %s\n",
                operations[index].path);
            return 0;
        }

        for (other_index = 0U;
             other_index < operation_count;
             ++other_index) {
            if (other_index == index ||
                strcmp(
                    operations[index].path,
                    operations[other_index].path) != 0) {
                continue;
            }

            if (other_index < index ||
                operations[other_index].is_create ||
                operations[other_index].is_block != 1) {
                (void)printf(
                    "Saved plan create_file path has an invalid ordered reuse: %s\n",
                    operations[index].path);
                return 0;
            }
        }
    }

    return 1;
}

static char *llm_m310_copy_text(const char *text)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1U);

    if (copy != NULL) {
        (void)memcpy(copy, text, length + 1U);
    }

    return copy;
}

static int llm_m310_stage_operations_chained(
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

    if (!llm_m310_create_precheck(operations, operation_count) ||
        !execute_delete_precheck(operations, operation_count) ||
        !execute_rename_precheck(operations, operation_count) ||
        !execute_move_precheck(operations, operation_count)) {
        return 0;
    }

    for (index = 0U; index < operation_count; ++index) {
        char *working_content;
        int already_processed;

        if (operations[index].is_block == 2) {
            if (!edit_txn_add_delete(
                    transaction,
                    operations[index].path)) {
                (void)printf(
                    "Unable to stage delete_file operation for %s. "
                    "Use an existing explicit numeric file version such as ;1.\n",
                    operations[index].path);
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
                    "and absent target explicit numeric versions in the same directory.");
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
                    "absent target explicit numeric versions in different directories.");
                return 0;
            }
            continue;
        }

        already_processed = 0;
        for (prior_index = 0U; prior_index < index; ++prior_index) {
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

        if (operations[index].is_create) {
            working_content =
                llm_m310_copy_text(operations[index].new_text);

            if (working_content == NULL) {
                return 0;
            }

            for (apply_index = index + 1U;
                 apply_index < operation_count;
                 ++apply_index) {
                char *next_content;

                if (strcmp(
                        operations[apply_index].path,
                        operations[index].path) != 0) {
                    continue;
                }

                if (operations[apply_index].is_create ||
                    operations[apply_index].is_block != 1) {
                    free(working_content);
                    return 0;
                }

                next_content = execute_apply_replacement_to_content(
                    working_content,
                    &operations[apply_index]);

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
                    "Unable to stage create_file operation for %s.\n",
                    operations[index].path);
                free(working_content);
                return 0;
            }

            free(working_content);
            continue;
        }

        if (!llm_path_is_safe(operations[index].path) ||
            llm_path_is_sensitive(operations[index].path)) {
            (void)printf(
                "Saved plan execution refused for path: %s\n",
                operations[index].path);
            return 0;
        }

        working_content = llm_read_text_file(operations[index].path);

        if (working_content == NULL) {
            (void)printf(
                "Unable to read saved operation target: %s\n",
                operations[index].path);
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

            if (operations[apply_index].is_create) {
                free(working_content);
                return 0;
            }

            next_content = execute_apply_replacement_to_content(
                working_content,
                &operations[apply_index]);

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
                "The path may be unreadable, unsafe, or impossible to snapshot.\n",
                operations[index].path);
            free(working_content);
            return 0;
        }

        free(working_content);
    }

    return 1;
}

#define execute_stage_operations_chained llm_m310_stage_operations_chained
