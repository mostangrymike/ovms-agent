        if (strncmp(line, "type=", 5U) == 0) {
            char type[64];

            if (saw_type ||
                saw_path ||
                saw_old ||
                saw_new) {
                (void)puts(
                    "Saved operation type field is duplicate or out of order."
                );
                (void)fclose(file);
                return 0;
            }

            if (!execute_copy_value(
                    type,
                    sizeof(type),
                    line + 5)) {
                (void)puts(
                    "Saved operation type is invalid or too long."
                );
                (void)fclose(file);
                return 0;
            }

            if (strcmp(type, "replace_text") == 0) {
                operation->is_block = 0;
                operation->is_create = 0;
            } else if (strcmp(type, "replace_block") == 0) {
                operation->is_block = 1;
                operation->is_create = 0;
            } else if (strcmp(type, "create_file") == 0) {
                operation->is_block = 1;
                operation->is_create = 1;
            } else if (strcmp(type, "delete_file") == 0) {
                operation->is_block = 2;
                operation->is_create = 0;
            } else {
                (void)puts(
                    "Saved operation type is unsupported; only replace_text, "
                    "replace_block, create_file, and delete_file are allowed."
                );
                (void)fclose(file);
                return 0;
            }

            saw_type = 1;
            continue;
        }
