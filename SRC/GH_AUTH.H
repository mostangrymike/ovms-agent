#ifndef GH_AUTH_H
#define GH_AUTH_H

#include <stddef.h>

int gh_saved_exec(const char *operation,
                  const char *arguments,
                  char *result,
                  size_t result_size,
                  void *context);

/* Prepare/remove the same short-lived repository-scoped Git authentication
   config used by guarded AGENT/GITHUB network operations.  The returned path
   contains no credential material; credentials remain only in the protected
   temporary config file. */
int gh_auth_prepare_active(char *path, size_t path_size);
void gh_auth_remove_active(void);

#endif /* GH_AUTH_H */
