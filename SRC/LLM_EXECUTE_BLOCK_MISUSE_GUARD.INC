        if (saw_type &&
            !operation->is_block &&
            (strcmp(line, "BEGIN_OLD_TEXT\n") == 0 ||
             strcmp(line, "BEGIN_OLD_TEXT\r\n") == 0 ||
             strcmp(line, "BEGIN_NEW_TEXT\n") == 0 ||
             strcmp(line, "BEGIN_NEW_TEXT\r\n") == 0)) {
            (void)puts(
                "Saved operation uses a block marker with replace_text."
            );
            (void)fclose(file);
            return 0;
        }
