static void gap009_remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static char *gap009_read_plan(void)
{
    FILE *file;
    long length;
    char *text;
    size_t got;

    file = fopen(OPENAI_PLAN_FILE, "r");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    got = fread(text, 1U, (size_t)length, file);
    if (fclose(file) != 0 || got != (size_t)length) {
        free(text);
        return NULL;
    }

    text[got] = '\0';
    return text;
}

static int gap009_has(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

int main(void)
{
    static const char scoped_plan[] =
        "Goal\nTest saved file scope.\n"
        "Files to modify\nSRC/OPENAI_PLAN.C\n"
        "Files to create\nTEST/GAP009_NEW.TXT\n"
        "Ordered edits\nTest only.\n"
        "operation_count=2\n"
        "BEGIN_OPERATION\n"
        "type=replace_block\n"
        "path=SRC/OPENAI_PLAN.C\n"
        "BEGIN_OLD_TEXT\nold\nEND_OLD_TEXT\n"
        "BEGIN_NEW_TEXT\nnew\nEND_NEW_TEXT\n"
        "END_OPERATION\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=TEST/GAP009_NEW.TXT\n"
        "BEGIN_NEW_TEXT\nnew\nEND_NEW_TEXT\n"
        "END_OPERATION\n";
    static const char zero_plan[] =
        "Goal\nInspect only.\n"
        "Files to modify\nNone.\n"
        "Files to create\nNone.\n"
        "Ordered edits\nNone.\n";
    static const char unsafe_plan[] =
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=../BAD.TXT\n"
        "BEGIN_NEW_TEXT\nx\nEND_NEW_TEXT\n"
        "END_OPERATION\n";
    const char *new_path;
    char *text;
    FILE *file;
    int ok;

    new_path = "TEST/GAP009_NEW.TXT";
    gap009_remove_all(OPENAI_PLAN_FILE);
    gap009_remove_all(new_path);
    ok = 1;

    if (!openai_plan_save("GAP-009 scoped plan", scoped_plan)) {
        (void)puts("M266: unable to save scoped GAP-009 plan.");
        ok = 0;
    }

    text = gap009_read_plan();
    if (text == NULL ||
        !gap009_has(text, "file_count=2\n") ||
        !gap009_has(text, "file=SRC/OPENAI_PLAN.C|size=") ||
        !gap009_has(text, "file=TEST/GAP009_NEW.TXT|missing=1\n")) {
        (void)puts("M266: scoped plan metadata is incorrect.");
        ok = 0;
    }
    free(text);

    if (ok && !openai_plan_file_current(OPENAI_PLAN_FILE, 0)) {
        (void)puts("M266: scoped saved plan is not current after save.");
        ok = 0;
    }

    file = fopen(new_path, "w");
    if (file == NULL || fputs("created after planning\n", file) == EOF) {
        if (file != NULL) {
            (void)fclose(file);
        }
        (void)puts("M266: unable to create planned-new fixture.");
        ok = 0;
    } else if (fclose(file) != 0) {
        (void)puts("M266: unable to close planned-new fixture.");
        ok = 0;
    }

    if (ok && openai_plan_file_current(OPENAI_PLAN_FILE, 0)) {
        (void)puts("M266: saved plan did not become stale when new file appeared.");
        ok = 0;
    }

    gap009_remove_all(new_path);
    gap009_remove_all(OPENAI_PLAN_FILE);

    if (ok && !openai_plan_save("GAP-009 zero scope", zero_plan)) {
        (void)puts("M266: unable to save zero-file plan.");
        ok = 0;
    }

    text = gap009_read_plan();
    if (ok && (text == NULL || !gap009_has(text, "file_count=0\n"))) {
        (void)puts("M266: zero-file plan did not persist file_count=0.");
        ok = 0;
    }
    free(text);

    if (ok && !openai_plan_file_current(OPENAI_PLAN_FILE, 0)) {
        (void)puts("M266: zero-file saved plan is not current.");
        ok = 0;
    }

    gap009_remove_all(OPENAI_PLAN_FILE);

    if (ok && openai_plan_save("GAP-009 unsafe scope", unsafe_plan)) {
        (void)puts("M266: unsafe plan scope was accepted.");
        ok = 0;
    }

    gap009_remove_all(OPENAI_PLAN_FILE);
    gap009_remove_all(new_path);

    if (!ok) {
        return 2;
    }

    (void)puts("M266 GAP-009 saved plan file-scope regression passed.");
    return 1;
}
