#ifndef LLM_PATH_H
#define LLM_PATH_H

#include <string.h>

static int llm_path_is_current_dir(const char *path)
{
    return path != NULL &&
        (strcmp(path, "") == 0 ||
         strcmp(path, ".") == 0 ||
         strcmp(path, "./") == 0 ||
         strcmp(path, "[]") == 0 ||
         strcmp(path, "<>") == 0 ||
         strcmp(path, "[.]") == 0 ||
         strcmp(path, "<.>") == 0);
}

int llm_path_is_safe(const char *path);
int llm_runtime_artifact(const char *path);
int llm_contains_ignore_case(const char *text,
                             const char *pattern);
int llm_path_is_sensitive(const char *path);
int llm_listing_entry_hidden(const char *name);

#endif /* LLM_PATH_H */
