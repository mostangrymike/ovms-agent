        if (strcmp(line, "BEGIN_OLD_TEXT\n") == 0 ||
            strcmp(line, "BEGIN_OLD_TEXT\r\n") == 0) {
            if (!saw_type ||
                !saw_path ||
                !operation->is_block ||
                operation->is_create ||
                saw_old ||
                saw_new ||
                !execute_read_text_block(
                    file,
                    "END_OLD_TEXT",
                    operation->old_text,
                    sizeof(operation->old_text),
                    0)) {
                (void)puts(
                    "Saved operation old_text block is invalid, empty, "
                    "out of order, unterminated, or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_old = 1;
            continue;
        }

        if (strcmp(line, "BEGIN_NEW_TEXT\n") == 0 ||
            strcmp(line, "BEGIN_NEW_TEXT\r\n") == 0) {
            if (!saw_type ||
                !saw_path ||
                !operation->is_block ||
                (!operation->is_create && !saw_old) ||
                saw_new ||
                !execute_read_text_block(
                    file,
                    "END_NEW_TEXT",
                    operation->new_text,
                    sizeof(operation->new_text),
                    1)) {
                (void)puts(
                    "Saved operation new_text block is invalid, "
                    "out of order, unterminated, or too long."
                );
                (void)fclose(file);
                return 0;
            }

            saw_new = 1;
            continue;
        }
