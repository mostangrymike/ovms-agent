static int m291_git_tmp_basename(
    const char *path,
    size_t length)
{
    const char *base;
    size_t base_length;
    size_t index;

    if (path == NULL || length == 0U) {
        return 0;
    }

    base = path;
    for (index = 0U; index < length; ++index) {
        if (path[index] == '/') {
            base = path + index + 1U;
        }
    }

    base_length = length - (size_t)(base - path);

#define M291_TMP_MATCH(name) \
    (base_length == sizeof(name) - 1U && \
     memcmp(base, name, sizeof(name) - 1U) == 0)

    if (M291_TMP_MATCH(M290_GIT_META_COM) ||
        M291_TMP_MATCH(M290_GIT_INDEX_TMP) ||
        M291_TMP_MATCH(M290_GIT_CACHED_TMP) ||
        M291_TMP_MATCH(M290_GIT_OTHER_TMP) ||
        M291_TMP_MATCH(M290_GIT_HASH_TMP) ||
        M291_TMP_MATCH(M290_GIT_TOP_TMP)) {
        return 1;
    }

#undef M291_TMP_MATCH

    return 0;
}

int llm_git_m291_filter_status(
    char *status,
    size_t status_size)
{
    char *read_position;
    char *write_position;
    char *line_end;
    size_t line_length;
    size_t path_length;
    int keep;
    int wrote_line;

    if (status == NULL || status_size == 0U ||
        strlen(status) >= status_size) {
        return 0;
    }

    read_position = status;
    write_position = status;
    wrote_line = 0;

    while (*read_position != '\0') {
        line_end = strchr(read_position, '\n');
        line_length = line_end == NULL ?
                      strlen(read_position) :
                      (size_t)(line_end - read_position);

        keep = 1;
        if (line_length >= 4U &&
            read_position[0] == '?' &&
            read_position[1] == '?' &&
            read_position[2] == ' ') {
            path_length = line_length - 3U;
            if (m291_git_tmp_basename(
                    read_position + 3U,
                    path_length)) {
                keep = 0;
            }
        }

        if (keep) {
            if (wrote_line) {
                *write_position++ = '\n';
            }
            if (write_position != read_position) {
                (void)memmove(
                    write_position,
                    read_position,
                    line_length);
            }
            write_position += line_length;
            wrote_line = 1;
        }

        if (line_end == NULL) {
            break;
        }
        read_position = line_end + 1U;
    }

    *write_position = '\0';
    return 1;
}

static int m290_git_rms_status(void)
{
    if (!m290_git_rms_status_base()) {
        return 0;
    }

    if (!llm_git_m291_filter_status(
            llm_git_status,
            sizeof(llm_git_status))) {
        llm_git_status[0] = '\0';
        return 0;
    }

    return 1;
}
