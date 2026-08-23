#ifndef GH_AUTH_H
#define GH_AUTH_H

#include <stddef.h>

int gh_saved_exec(const char *operation,
                  const char *arguments,
                  char *result,
                  size_t result_size,
                  void *context);

#endif /* GH_AUTH_H */
