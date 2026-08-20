{
    unsigned int duplicate_index;

    for (duplicate_index = 0U;
         duplicate_index < operation_count;
         ++duplicate_index) {
        if (strcmp(operations[duplicate_index].path,
                   operation->path) == 0) {
            (void)puts(
                "Saved plan contains duplicate operation paths."
            );
            (void)fclose(file);
            return 0;
        }
    }
}
