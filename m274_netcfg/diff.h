/*
 * DIFF.H - Phase 2 configuration diff public API
 * DEC C / OpenVMS C89 compatible
 *
 * Provides a small API to compare a stored/old configuration file
 * (which may be absent) with a required candidate configuration file,
 * report a summary of added/removed/changed line counts, and optionally
 * write a compact human-readable unified-diff-like output file.
 *
 * Notes:
 *  - The implementation may depend on FILEUTIL to read files, but this
 *    header exposes no FILEUTIL symbols or semantics.
 *  - If old_cfg_path is NULL, points to an empty string, or refers to a
 *    non-existent file, it is treated as an absent/empty configuration.
 *  - If diff_out_path is NULL or points to an empty string, no diff file
 *    is written. If provided, an existing file at that path may be
 *    overwritten by the implementation.
 *  - The unified-diff-like format is intended for human reading. A
 *    reasonable compact form includes headers ("---", "+++"), hunk
 *    ranges ("@@ -a,b +c,d @@"), and per-line markers: ' ' (context),
 *    '-' (removal), '+' (addition). The exact formatting may be
 *    simplified to remain compact and VMS-friendly.
 *  - Summary counts semantics: lines classified as additions and
 *    removals follow the diff output. "changed" represents line
 *    replacements within hunks and is typically computed as the
 *    minimum of added and removed lines within each replacement group,
 *    aggregated across all hunks. Remaining adds/removes beyond that
 *    minimum are counted in their respective categories.
 */

#ifndef DIFF_H_INCLUDED
#define DIFF_H_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes */
#define CDIFF_OK            0
#define CDIFF_ERR_PARAM     1
#define CDIFF_ERR_IO        2
#define CDIFF_ERR_NOMEM     3
#define CDIFF_ERR_STATE     4

/* Default context lines for unified-diff-like output */
#define CDIFF_DEF_CONTEXT   3

/*
 * CFGDIFF_SUMMARY
 *
 * Summary of differences between old and new configurations.
 * All counts are non-negative and fit in unsigned long.
 */
typedef struct cfgdiff_summary {
    unsigned long added;    /* lines present only in candidate/new */
    unsigned long removed;  /* lines present only in stored/old */
    unsigned long changed;  /* line replacements (see notes above) */
} CFGDIFF_SUMMARY;

/*
 * cfgdiff_compare_paths
 *
 * Compare a stored/old configuration (may be absent) with a required
 * candidate configuration at new_cfg_path. Optionally write a compact
 * human-readable unified-diff-like output file to diff_out_path. When
 * context_lines is 0, a reasonable default (CDIFF_DEF_CONTEXT) is used.
 *
 * Parameters:
 *   old_cfg_path   - path to stored/old config; may be NULL/empty/absent
 *   new_cfg_path   - path to candidate/new config (required, non-NULL)
 *   diff_out_path  - optional path to write diff output; NULL/empty to skip
 *   context_lines  - number of context lines in unified output (0 => default)
 *   out_summary    - optional; receives summary counts on success
 *
 * Returns: CDIFF_OK on success, or a CDIFF_ERR_* code on failure.
 *
 * Failure cases include parameter errors, I/O errors reading inputs or
 * writing the diff file, or memory exhaustion. Absence of old_cfg_path
 * is not an error and is treated as an empty configuration.
 */
int cfgdiff_compare_paths(const char *old_cfg_path,
                          const char *new_cfg_path,
                          const char *diff_out_path,
                          unsigned int context_lines,
                          CFGDIFF_SUMMARY *out_summary);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DIFF_H_INCLUDED */
