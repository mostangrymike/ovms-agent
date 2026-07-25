#ifndef OPENAI_PATH_H
#define OPENAI_PATH_H

int openai_path_is_safe(const char *path);
int openai_path_is_sensitive(const char *path);
int openai_listing_entry_hidden(const char *name);

#endif /* OPENAI_PATH_H */
