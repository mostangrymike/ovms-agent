        if (operation->is_block &&
            (strncmp(line, "old_text=", 9U) == 0 ||
             strncmp(line, "new_text=", 9U) == 0)) {
            (void)puts(
                operation->is_create ?
                "Saved create_file operation must use a new_text block." :
                "Saved operation uses a legacy text field with replace_block."
            );
            (void)fclose(file);
            return 0;
        }
