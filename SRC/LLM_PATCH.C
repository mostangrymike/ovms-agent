#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_PATCH.H"
#include "ANSI_TERM.H"
#include "llm_internal.h"

static int m273_patch_marker(const char *line, const char *marker)
{
    size_t length;

    if (line == NULL || marker == NULL) {
        return 0;
    }

    length = strlen(marker);
    if (strncmp(line, marker, length) != 0) {
        return 0;
    }

    return line[length] == '\0' ||
           line[length] == '\n' ||
           line[length] == '\r';
}

static void m273_patch_preview(const char *path)
{
    FILE *file;
    char line[1024];
    int kind;

    if (!ansi_term_enabled() || path == NULL || *path == '\0') {
        return;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    ansi_term_puts("Patch preview:");
    kind = ANSI_DIFF_CONTEXT;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (m273_patch_marker(line, "@@OLD")) {
            kind = ANSI_DIFF_DELETE;
            ansi_term_diff(ANSI_DIFF_CONTEXT, line);
        } else if (m273_patch_marker(line, "@@NEW")) {
            kind = ANSI_DIFF_ADD;
            ansi_term_diff(ANSI_DIFF_CONTEXT, line);
        } else if (m273_patch_marker(line, "@@END")) {
            ansi_term_diff(ANSI_DIFF_CONTEXT, line);
            kind = ANSI_DIFF_CONTEXT;
        } else if (strncmp(line, "TARGET ", 7U) == 0) {
            ansi_term_diff(ANSI_DIFF_CONTEXT, line);
        } else {
            ansi_term_diff(kind, line);
        }
    }

    (void)fclose(file);
}

static char *m279_patch_getenv(const char *name)
{
    const char *policy;

    if (name != NULL &&
        strcmp(name, "OVMS_AGENT_APPROVAL_POLICY") == 0) {
        policy = llm_approval_name();
        return (char *)(policy != NULL ? policy : "read-only");
    }

    return getenv(name);
}

#define getenv m279_patch_getenv
#define llm_patch_apply_cmd m273_patch_apply_core
#include "LLM_PATCH_CORE.INC"
#undef llm_patch_apply_cmd
#undef getenv

void llm_patch_apply_cmd(const char *arguments)
{
    char validation[2048];

    if (ansi_term_enabled() &&
        arguments != NULL &&
        *arguments != '\0' &&
        llm_patch_validate(
            arguments,
            validation,
            sizeof(validation))) {
        m273_patch_preview(arguments);
    }

    m273_patch_apply_core(arguments);
}
