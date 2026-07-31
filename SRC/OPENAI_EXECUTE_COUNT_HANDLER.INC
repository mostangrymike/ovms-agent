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
