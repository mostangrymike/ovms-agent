/* M279 flat-root plan-scope compatibility. */

static int m279_plan_ext_equal(const char *left,
                               const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;
        if (tolower(a) != tolower(b)) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int m279_plan_local_file(const char *candidate)
{
    static const char *extensions[] = {
        "c", "h", "com", "opt", "inc", "txt", "md",
        "cob", "mar", "mms", "dat", "json", "cld", NULL
    };
    const char *dot;
    const char **extension;

    if (candidate == NULL || *candidate == '\0') {
        return 0;
    }

    dot = strrchr(candidate, '.');
    if (dot == NULL || dot == candidate || dot[1] == '\0') {
        return 0;
    }

    for (extension = extensions; *extension != NULL; ++extension) {
        if (m279_plan_ext_equal(dot + 1, *extension)) {
            return 1;
        }
    }

    return 0;
}

static char *m279_plan_strchr(const char *text, int ch)
{
    char *match;

    match = strchr(text, ch);
    if (match != NULL || ch != '/') {
        return match;
    }

    if (m279_plan_local_file(text)) {
        return (char *)text + strlen(text);
    }

    return NULL;
}

/*
 * LLM_PLAN.C's legacy candidate predicate uses strchr(candidate, '/') as its
 * generic-path test. Redirect only subsequent calls in this translation unit;
 * the wrapper is identical for every character except '/' and only broadens
 * that one predicate to known flat-root project filenames.
 */
#define strchr m279_plan_strchr
