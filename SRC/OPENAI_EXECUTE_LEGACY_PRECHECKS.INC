        if (!operation->is_block &&
            strncmp(line, "old_text=", 9U) == 0 &&
            (!saw_type ||
             !saw_path ||
             saw_old ||
             saw_new)) {
            (void)puts(
                "Saved operation old_text field is duplicate or out of order."
            );
            (void)fclose(file);
            return 0;
        }

        if (!operation->is_block &&
            strncmp(line, "new_text=", 9U) == 0 &&
            (!saw_type ||
             !saw_path ||
             !saw_old ||
             saw_new)) {
            (void)puts(
                "Saved operation new_text field is duplicate or out of order."
            );
            (void)fclose(file);
            return 0;
        }
