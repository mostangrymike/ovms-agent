#ifndef M289_NATIVE_BUILD_H
#define M289_NATIVE_BUILD_H

#include <stddef.h>

#include "agent.h"

#define M289_BUILD_COMPILE 1
#define M289_BUILD_LINK 2
#define M289_NATIVE_RESULT_MAX 32768U

int m289_command_allowed(const char *command,
                         int phase,
                         const char *language);

char *m289_build_source(agent_state *state,
                        const char *source,
                        unsigned long *status_out);

#endif /* M289_NATIVE_BUILD_H */
