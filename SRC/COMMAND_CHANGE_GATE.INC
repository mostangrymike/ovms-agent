#include <ctype.h>
#include <string.h>

static int m303_change_word_eq(const char *text,
                               const char *word)
{
    while (*text != '\0' && *word != '\0') {
        if (toupper((unsigned char)*text) !=
            toupper((unsigned char)*word)) {
            return 0;
        }
        ++text;
        ++word;
    }

    return *word == '\0';
}

static int m303_changeset_apply_input(const char *input)
{
    const char *cursor;
    const char *start;
    char word[16];
    size_t used;

    if (input == NULL) {
        return 0;
    }

    cursor = input;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    start = cursor;
    used = 0U;
    while (*cursor != '\0' &&
           *cursor != ' ' &&
           *cursor != '\t' &&
           used + 1U < sizeof(word)) {
        word[used++] = *cursor++;
    }
    word[used] = '\0';

    if (start == cursor ||
        !m303_change_word_eq(word, "CHANGESET")) {
        return 0;
    }

    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    start = cursor;
    used = 0U;
    while (*cursor != '\0' &&
           *cursor != ' ' &&
           *cursor != '\t' &&
           used + 1U < sizeof(word)) {
        word[used++] = *cursor++;
    }
    word[used] = '\0';

    if (start == cursor ||
        !m303_change_word_eq(word, "APPLY")) {
        return 0;
    }

    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    return *cursor == '\0';
}

static int m303_change_policy_ok(const char *approval)
{
    return approval != NULL &&
           (strcmp(approval, "workspace") == 0 ||
            strcmp(approval, "full") == 0 ||
            strcmp(approval, "autopilot") == 0);
}
