/* Add near the writer module's includes. */
#include "rms_write.h"

/*
 * Replace full-file write code like:
 *
 *   file = fopen(path, "w");
 *   fputs(text, file);
 *   fclose(file);
 *
 * with:
 */
if (!rms_replace_text_file(path, text)) {
    /* preserve the existing error path */
}

/*
 * Route only complete-file replacement buffers through this function.
 * Incremental line editors may keep their current logic until explicitly
 * converted.
 */
