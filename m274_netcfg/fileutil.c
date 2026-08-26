/*
 * FILEUTIL.C - Phase 2 minimal file reading implementation
 * DEC C / OpenVMS C89 compatible
 *
 * Implements:
 *   - fu_read_file_lines: read a text file into an array of C strings
 *   - fu_free_lines: free that representation
 *
 * Behavior:
 *   - Splits lines on \n, \r\n, or \r
 *   - Returned strings exclude any newline terminators
 *   - Uses only fopen/fgetc/ungetc/malloc/realloc/free
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "fileutil.h"

/*
 * Internal helpers are static to avoid external symbol exposure.
 */

/* Safely grow a pointer array of char* lines. */
static int fu_grow_lines(char ***plines, size_t *pcap, size_t need)
{
    char **nl;
    size_t cap, newcap;

    if (plines == NULL || pcap == NULL) {
        return 0;
    }

    cap = *pcap;
    if (need <= cap) {
        return 1; /* already large enough */
    }

    newcap = cap ? cap : 16u;
    while (newcap < need) {
        if (newcap > (((size_t)-1) / 2u)) {
            /* Would overflow size_t on doubling */
            return 0;
        }
        newcap *= 2u;
    }

    /* Check bytes overflow for realloc argument */
    if (newcap > (((size_t)-1) / sizeof(char *))) {
        return 0;
    }

    nl = (char **)realloc(*plines, newcap * sizeof(char *));
    if (nl == NULL) {
        return 0;
    }

    *plines = nl;
    *pcap = newcap;
    return 1;
}

/* Safely grow a byte buffer to hold at least need bytes. */
static int fu_grow_buf(char **pbuf, size_t *pcap, size_t need)
{
    char *nb;
    size_t cap, newcap;

    if (pbuf == NULL || pcap == NULL) {
        return 0;
    }

    cap = *pcap;
    if (need <= cap) {
        return 1; /* already large enough */
    }

    newcap = cap ? cap : 64u;
    while (newcap < need) {
        if (newcap > (((size_t)-1) / 2u)) {
            return 0; /* overflow on doubling */
        }
        newcap *= 2u;
    }

    nb = (char *)realloc(*pbuf, newcap);
    if (nb == NULL) {
        return 0;
    }

    *pbuf = nb;
    *pcap = newcap;
    return 1;
}

/* Free an array of lines with count entries; safe on NULL/0. */
static void fu_free_vec(char **lines, size_t count)
{
    size_t i;
    if (lines == NULL) {
        return;
    }
    for (i = 0; i < count; ++i) {
        free(lines[i]);
    }
    free(lines);
}

int fu_read_file_lines(const char *path, char ***out_lines, unsigned long *out_count)
{
    FILE *fp;
    char **lines;
    size_t lcount, lcap;
    char *lbuf;
    size_t llen, lcapbytes;
    int ch;

    if (out_lines != NULL) {
        *out_lines = NULL;
    }
    if (out_count != NULL) {
        *out_count = 0UL;
    }

    if (path == NULL || out_lines == NULL || out_count == NULL) {
        return FU_ERR_PARAM;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return FU_ERR_IO;
    }

    lines = NULL;
    lcount = 0u;
    lcap = 0u;
    lbuf = NULL;
    llen = 0u;
    lcapbytes = 0u;

    for (;;) {
        ch = fgetc(fp);
        if (ch == EOF) {
            if (ferror(fp)) {
                /* I/O error: discard partial results and fail */
                fu_free_vec(lines, lcount);
                free(lbuf);
                fclose(fp);
                return FU_ERR_IO;
            }
            /* EOF: emit last line if it has any content */
            if (llen > 0u) {
                char *line;
                if (llen + 1u < llen) { /* overflow check */
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                line = (char *)malloc(llen + 1u);
                if (line == NULL) {
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                if (llen > 0u) {
                    memcpy(line, lbuf, llen);
                }
                line[llen] = '\0';

                if (!fu_grow_lines(&lines, &lcap, lcount + 1u)) {
                    free(line);
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                lines[lcount++] = line;
                llen = 0u; /* reset, though we will exit */
            }
            break; /* normal EOF */
        }

        if (ch == '\n' || ch == '\r') {
            /* Handle CRLF as a single newline */
            if (ch == '\r') {
                int ch2 = fgetc(fp);
                if (ch2 != '\n' && ch2 != EOF) {
                    /* Not a CRLF sequence; push back */
                    ungetc(ch2, fp);
                }
            }

            /* Emit current line (may be empty between delimiters) */
            {
                char *line;
                if (llen + 1u < llen) {
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                line = (char *)malloc(llen + 1u);
                if (line == NULL) {
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                if (llen > 0u) {
                    memcpy(line, lbuf, llen);
                }
                line[llen] = '\0';

                if (!fu_grow_lines(&lines, &lcap, lcount + 1u)) {
                    free(line);
                    fu_free_vec(lines, lcount);
                    free(lbuf);
                    fclose(fp);
                    return FU_ERR_NOMEM;
                }
                lines[lcount++] = line;
                llen = 0u; /* reset for next line */
            }
        } else {
            /* Ordinary character: append to line buffer */
            if (!fu_grow_buf(&lbuf, &lcapbytes, llen + 1u)) {
                fu_free_vec(lines, lcount);
                free(lbuf);
                fclose(fp);
                return FU_ERR_NOMEM;
            }
            lbuf[llen++] = (char)ch;
        }
    }

    free(lbuf);

    /* Final safety: ensure count fits in unsigned long */
    if (lcount > (size_t)ULONG_MAX) {
        fu_free_vec(lines, lcount);
        fclose(fp);
        return FU_ERR_STATE;
    }

    /* Optional shrink to exact size (avoid realloc(,0)) */
    if (lcount > 0u) {
        char **sh;
        if (lcount <= (((size_t)-1) / sizeof(char *))) {
            sh = (char **)realloc(lines, lcount * sizeof(char *));
            if (sh != NULL) {
                lines = sh;
            }
        }
    }

    fclose(fp);

    *out_lines = lines;
    *out_count = (unsigned long)lcount;
    return FU_OK;
}

void fu_free_lines(char **lines, unsigned long count)
{
    unsigned long i;
    if (lines == NULL || count == 0UL) {
        return;
    }
    for (i = 0UL; i < count; ++i) {
        free(lines[i]);
    }
    free(lines);
}
