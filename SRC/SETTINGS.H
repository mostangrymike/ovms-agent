#ifndef OVMS_AGENT_SETTINGS_H
#define OVMS_AGENT_SETTINGS_H

#include <stddef.h>

int settings_load(void);
int settings_reload(void);
int settings_save(void);
int settings_reset(void);
const char *settings_get(const char *key);
int settings_set(const char *key, const char *value);
int settings_set_bool(const char *key, int enabled);
int settings_set_long(const char *key, long value);
const char *settings_effective_text(const char *key,
                                    const char *logical_name,
                                    const char *fallback);
int settings_effective_bool(const char *key,
                            const char *logical_name,
                            int fallback);
long settings_effective_long(const char *key,
                             const char *logical_name,
                             long fallback,
                             long minimum,
                             long maximum);
const char *settings_value_source(const char *key,
                                  const char *logical_name);

#endif /* OVMS_AGENT_SETTINGS_H */
