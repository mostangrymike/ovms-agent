        if (saw_new) {
            (void)puts(
                "Saved operation contains trailing fields after new_text."
            );
            (void)fclose(file);
            return 0;
        }
