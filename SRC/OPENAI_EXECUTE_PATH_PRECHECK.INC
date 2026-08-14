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
