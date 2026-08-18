#ifndef OVMS_AGENT_SECRET_INPUT_H
#define OVMS_AGENT_SECRET_INPUT_H

#include <stddef.h>

int secret_read(const char *prompt,
                char *buffer,
                size_t buffer_size);

#endif /* OVMS_AGENT_SECRET_INPUT_H */
