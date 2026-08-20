        if (strcmp(line, "BEGIN_OPERATION\n") == 0 ||
            strcmp(line, "BEGIN_OPERATION\r\n") == 0) {
            if (in_operation ||
                operation_count >= operation_capacity) {
                (void)puts(
                    "Saved plan contains too many or nested operations."
                );
                (void)fclose(file);
                return 0;
            }

            operation = &operations[operation_count];
            (void)memset(operation, 0, sizeof(*operation));
            in_operation = 1;
            saw_type = 0;
            saw_path = 0;
            saw_old = 0;
            saw_new = 0;
            continue;
        }

        #include "openai_execute_toplevel_guard.inc"

        if (strcmp(line, "END_OPERATION\n") == 0 ||
            strcmp(line, "END_OPERATION\r\n") == 0) {
            if (!saw_type ||
                !saw_path ||
                !saw_old ||
                !saw_new) {
                (void)puts(
                    "Saved plan contains an incomplete operation."
                );
                (void)fclose(file);
                return 0;
            }

            ++operation_count;
            operation = NULL;
            in_operation = 0;
            continue;
        }

        #include "openai_execute_after_new_guard.inc"
        #include "openai_execute_type_handler.inc"
        #include "openai_execute_path_precheck.inc"

        if (strncmp(line, "path=", 5U) == 0) {
            if (!execute_copy_value(
                    operation->path,
                    sizeof(operation->path),
                    line + 5)) {
                (void)puts(
                    "Saved operation path is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_path = 1;
            continue;
        }

        #include "openai_execute_block_misuse_guard.inc"
        #include "openai_execute_block_handlers.inc"
        #include "openai_execute_legacy_misuse_guard.inc"
        #include "openai_execute_legacy_prechecks.inc"

        if (!operation->is_block &&
            strncmp(line, "old_text=", 9U) == 0) {
            if (!execute_copy_value(
                    operation->old_text,
                    sizeof(operation->old_text),
                    line + 9)) {
                (void)puts(
                    "Saved operation old_text is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_old = 1;
            continue;
        }

        if (!operation->is_block &&
            strncmp(line, "new_text=", 9U) == 0) {
            if (!execute_copy_value(
                    operation->new_text,
                    sizeof(operation->new_text),
                    line + 9)) {
                (void)puts(
                    "Saved operation new_text is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_new = 1;
            continue;
        }

        #include "openai_execute_fallback_m100.inc"
    }
