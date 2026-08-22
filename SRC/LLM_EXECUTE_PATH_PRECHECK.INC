        if (strncmp(line, "target_path=", 12U) == 0) {
            if (!saw_type ||
                !saw_path ||
                (operation->is_block != 3 && operation->is_block != 4) ||
                saw_old ||
                saw_new ||
                !execute_copy_value(
                    operation->old_text,
                    sizeof(operation->old_text),
                    line + 12)) {
                (void)puts(
                    "Saved rename/move target_path is invalid, duplicate, or out of order."
                );
                (void)fclose(file);
                return 0;
            }

            /* rename_file and move_file are path + target_path only.  Reuse
             * old_text to carry target_path internally and mark the normal
             * payload requirements satisfied so END_OPERATION remains
             * authoritative. */
            saw_old = 1;
            saw_new = 1;
            continue;
        }

        if (strncmp(line, "path=", 5U) == 0 &&
            (!saw_type ||
             saw_path ||
             saw_old ||
             saw_new)) {
            (void)puts(
                "Saved operation path field is duplicate or out of order."
            );
            (void)fclose(file);
            return 0;
        }

        if (strncmp(line, "path=", 5U) == 0 &&
            saw_type &&
            operation->is_block == 2) {
            /* delete_file is intentionally path-only.  Mark the text
             * requirements satisfied so END_OPERATION can use the existing
             * completeness check; the after-new guard rejects any trailing
             * payload or metadata before END_OPERATION. */
            saw_old = 1;
            saw_new = 1;
        }
