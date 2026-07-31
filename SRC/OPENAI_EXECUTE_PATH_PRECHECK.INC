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
