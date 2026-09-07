        if (strcmp(line, "[plan]\n") == 0 ||
            strcmp(line, "[plan]\r\n") == 0 ||
            strcmp(line, "[plan]\r") == 0 ||
            strcmp(line, "[plan]") == 0) {
            if (in_operation ||
                saw_declared_count ||
                declared_operation_count != 0U) {
                (void)puts(
                    "Saved plan contains a duplicate or misplaced [plan] section."
                );
                (void)fclose(file);
                return 0;
            }

            declared_operation_count = operation_capacity + 1U;
            continue;
        }

        if (!saw_declared_count &&
            declared_operation_count == 0U) {
            continue;
        }

        if (strncmp(line, "operation_count=", 16U) == 0) {
            const char *count_text;
            const char *cursor;
            unsigned int value;

            if (saw_declared_count || in_operation) {
                (void)puts(
                    "Saved plan contains a misplaced or duplicate operation_count."
                );
                (void)fclose(file);
                return 0;
            }

            count_text = line + 16;
            cursor = count_text;
            value = 0U;

            if (*cursor < '1' || *cursor > '9') {
                (void)puts(
                    "Saved plan operation_count is invalid."
                );
                (void)fclose(file);
                return 0;
            }

            while (*cursor >= '0' && *cursor <= '9') {
                unsigned int digit;

                digit = (unsigned int)(*cursor - '0');

                if (value > (operation_capacity - digit) / 10U) {
                    (void)puts(
                        "Saved plan operation_count is invalid."
                    );
                    (void)fclose(file);
                    return 0;
                }

                value = value * 10U + digit;
                ++cursor;
            }

            if (value == 0U ||
                value > operation_capacity ||
                (*cursor != '\n' &&
                 *cursor != '\r' &&
                 *cursor != '\0')) {
                (void)puts(
                    "Saved plan operation_count is invalid."
                );
                (void)fclose(file);
                return 0;
            }

            if (*cursor == '\r' &&
                cursor[1] != '\n' &&
                cursor[1] != '\0') {
                (void)puts(
                    "Saved plan operation_count is invalid."
                );
                (void)fclose(file);
                return 0;
            }

            declared_operation_count = value;
            saw_declared_count = 1;
            continue;
        }
