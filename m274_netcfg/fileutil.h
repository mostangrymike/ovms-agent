/*
 * FILEUTIL.H - Phase 2 minimal file reading public API
 * DEC C / OpenVMS C89 compatible
 * Read a text file into an array of C strings (lines) and free them.
 * No diff logic, rule logic, storage behavior, CLI behavior, or Phase 3.
 */

#ifndef FILEUTIL_H_INCLUDED
#define FILEUTIL_H_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes */
#define FU_OK               0
#define FU_ERR_PARAM        1
#define FU_ERR_IO           2
#define FU_ERR_NOMEM        3
#define FU_ERR_STATE        4

/*
 * fu_read_file_lines
 *
 * Read a text file identified by path into an array of NUL-terminated
 * C strings, one entry per logical line. Newline terminators are not
 * included in the returned strings. Line splitting should accept common
 * text conventions (e.g., "\n", "\r\n", or "\r").
 *
 * On success, *out_lines receives a heap-allocated array of pointers,
 * each pointer referencing a heap-allocated char buffer for that line.
 * *out_count receives the number of lines. Call fu_free_lines to
 * release all memory returned by this function.
 *
 * On failure, no memory is leaked; *out_lines and *out_count are set to
 * a defined empty state (NULL and 0 respectively) before returning an
 * error code.
 *
 * Parameters:
 *   path       - file path of the text file to read (non-NULL)
 *   out_lines  - receives array of line pointers (non-NULL)
 *   out_count  - receives number of lines (non-NULL)
 *
 * Returns: FU_OK on success, or an FU_ERR_* code on failure.
 */
int fu_read_file_lines(const char *path,
                       char ***out_lines,
                       unsigned long *out_count);

/*
 * fu_free_lines
 *
 * Free an array of lines previously returned by fu_read_file_lines.
 * Safe to call with lines == NULL and/or count == 0.
 */
void fu_free_lines(char **lines, unsigned long count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILEUTIL_H_INCLUDED */
