        if (operation->is_block &&
            (strncmp(line, "old_text=", 9U) == 0 ||
             strncmp(line, "new_text=", 9U) == 0)) {
            (void)puts(
                "Saved operation uses a legacy text field with replace_block."
            );
            (void)fclose(file);
            return 0;
        }
