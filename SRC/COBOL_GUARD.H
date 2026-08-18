#ifndef COBOL_GUARD_H
#define COBOL_GUARD_H

#include <stddef.h>

int cobol_edit_safe(
    const char *path,
    const char *original,
    const char *candidate,
    char *reason,
    size_t reason_size);

int cobol_text_safe(
    const char *path,
    const char *original,
    const char *old_text,
    const char *new_text,
    char *reason,
    size_t reason_size);

#endif
