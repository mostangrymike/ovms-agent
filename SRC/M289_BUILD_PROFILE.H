#ifndef M289_BUILD_PROFILE_H
#define M289_BUILD_PROFILE_H

#include <stddef.h>

#define M289_PROFILE_NAME_MAX 32U
#define M289_PROFILE_EXTS_MAX 128U
#define M289_PROFILE_CMD_MAX 512U
#define M289_PROFILE_OPTS_MAX 256U
#define M289_PROFILE_SUFFIX_MAX 16U
#define M289_PROFILE_PATH_MAX 1024U
#define M289_PROFILE_ERROR_MAX 160U

typedef struct m289_build_profile {
    char language[M289_PROFILE_NAME_MAX];
    char extensions[M289_PROFILE_EXTS_MAX];
    char kind[M289_PROFILE_NAME_MAX];
    char compile_command[M289_PROFILE_CMD_MAX];
    char compile_options[M289_PROFILE_OPTS_MAX];
    char link_command[M289_PROFILE_CMD_MAX];
    char link_options[M289_PROFILE_OPTS_MAX];
    char object_extension[M289_PROFILE_SUFFIX_MAX];
    char executable_extension[M289_PROFILE_SUFFIX_MAX];
} m289_build_profile;

int m289_profile_load(const char *path,
                      m289_build_profile *profile,
                      char *error,
                      size_t error_size);

int m289_profile_commands(const m289_build_profile *profile,
                          const char *source,
                          char *compile_command,
                          size_t compile_size,
                          char *link_command,
                          size_t link_size,
                          char *error,
                          size_t error_size);

#endif
