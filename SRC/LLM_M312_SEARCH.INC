#define LLM_M312_SUMMARY_RESERVE 160U

static int llm_m312_search_stream(
    FILE *file,
    const char *pattern,
    char *output,
    size_t output_size,
    unsigned long *total_matches,
    unsigned long *returned_matches,
    int *truncated)
{
    char line[2048];
    unsigned long line_number;
    size_t used;

    if (file == NULL || pattern == NULL || *pattern == '\0' ||
        output == NULL || output_size == 0U ||
        total_matches == NULL || returned_matches == NULL ||
        truncated == NULL) {
        return 0;
    }

    output[0] = '\0';
    *total_matches = 0UL;
    *returned_matches = 0UL;
    *truncated = 0;
    line_number = 1UL;
    used = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, pattern) != NULL) {
            int written;

            ++(*total_matches);

            if (!*truncated) {
                written = snprintf(
                    output + used,
                    output_size - used,
                    "%lu: %s",
                    line_number,
                    line
                );

                if (written < 0 ||
                    (size_t)written >= output_size - used) {
                    *truncated = 1;
                } else {
                    used += (size_t)written;
                    if (used > 0U && output[used - 1U] != '\n') {
                        if (used + 1U < output_size) {
                            output[used++] = '\n';
                            output[used] = '\0';
                        } else {
                            *truncated = 1;
                        }
                    }

                    if (!*truncated) {
                        ++(*returned_matches);
                    }
                }
            }
        }

        ++line_number;
    }

    return !ferror(file);
}
