        if (!in_operation) {
            if (!saw_declared_count) {
                continue;
            }

            if (strcmp(line, "\n") == 0 ||
                strcmp(line, "\r\n") == 0 ||
                strcmp(line, "\r") == 0 ||
                line[0] == '\0') {
                continue;
            }

            (void)puts(
                "Saved plan contains an unexpected token outside operation blocks."
            );
            (void)fclose(file);
            return 0;
        }
