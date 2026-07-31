#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "openai_repair.h"
#include "openai_internal.h"

static void lowercase(char *s)
{
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

void openai_repair_plan(agent_state *state)
{
    (void)state; /* ignored as requested */

    const char *path = "OVMS_AGENT_FAILED_BUILD.TXT";
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No failed build output is available.\n");
        return;
    }

    char line[1024];
    char diagnostic[1024] = {0};
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* trim leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "%CC-", 4) == 0 || strncmp(p, "%ILINK-", 7) == 0) {
            printf("First actionable diagnostic:\n");
            /* print the line as-is (from p) */
            printf("%s", p);
            /* Ensure newline at end if not present */
            size_t len = strlen(p);
            if (len == 0 || p[len-1] != '\n')
                printf("\n");
            /* save diagnostic for prompt */
            strncpy(diagnostic, p, sizeof(diagnostic)-1);
            diagnostic[sizeof(diagnostic)-1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("No actionable compiler or linker diagnostic was found.\n");
        fclose(f);
        return;
    }

    /* Search following lines for: at line number <number> in file <filespec> */
    int target_line = -1;
    char filespec[512] = {0};
    while (fgets(line, sizeof(line), f)) {
        char *q = strstr(line, "at line number ");
        if (!q) continue;
        char *r = strstr(q, " in file ");
        if (!r) continue;
        /* parse the line number */
        int n = 0;
        if (sscanf(q, "at line number %d", &n) != 1) continue;
        /* extract filespec (rest of line after " in file ") */
        char *fs = r + strlen(" in file ");
        while (*fs == ' ' || *fs == '\t') fs++;
        /* copy and trim newline */
        size_t i = 0;
        while (*fs && *fs != '\n' && i + 1 < sizeof(filespec)) filespec[i++] = *fs++;
        filespec[i] = '\0';
        if (i == 0) continue;
        target_line = n;
        break;
    }

    if (target_line < 0) {
        printf("Could not find a matching 'at line number <number> in file <filespec>' after the diagnostic; refusing.\n");
        fclose(f);
        return;
    }

    /* Convert filespec like "[MIKE.OVMS_AGENT.SRC]NAME.C;version" to "src/name.c" */
    char projpath[512] = {0};
    char *br_open = strchr(filespec, '[');
    char *br_close = strchr(filespec, ']');
    if (!br_open || !br_close || br_close < br_open) {
        printf("Filespec parsing failed (no bracketed area) for '%s'; refusing.\n", filespec);
        fclose(f);
        return;
    }
    /* extract inside of brackets and take last '.'-separated token */
    char inside[256] = {0};
    size_t inside_len = (size_t)(br_close - (br_open + 1));
    if (inside_len == 0 || inside_len >= sizeof(inside)) {
        printf("Filespec parsing failed (invalid bracket contents) for '%s'; refusing.\n", filespec);
        fclose(f);
        return;
    }
    memcpy(inside, br_open + 1, inside_len);
    inside[inside_len] = '\0';
    char *lastdot = strrchr(inside, '.');
    char dirpart[128] = {0};
    if (lastdot) strncpy(dirpart, lastdot + 1, sizeof(dirpart)-1);
    else strncpy(dirpart, inside, sizeof(dirpart)-1);
    /* filename is what's after the closing ']' */
    char filename[256] = {0};
    char *fname = br_close + 1;
    while (*fname == ' ' || *fname == '\t' || *fname == ':') fname++;
    /* strip version after ';' if present */
    char *semi = strchr(fname, ';');
    size_t fn_len = 0;
    if (semi) fn_len = (size_t)(semi - fname);
    else fn_len = strlen(fname);
    if (fn_len == 0 || fn_len >= sizeof(filename)) {
        printf("Filespec parsing failed (invalid filename) for '%s'; refusing.\n", filespec);
        fclose(f);
        return;
    }
    memcpy(filename, fname, fn_len);
    filename[fn_len] = '\0';

    /* build project-relative path: <dirpart>/<filename> in lowercase */
    lowercase(dirpart);
    lowercase(filename);
    /* Accept only when dirpart equals "src" (after lowercasing) or when originally came from ...SRC */
    /* We will construct path as dirpart/filename */
    if (snprintf(projpath, sizeof(projpath), "%s/%s", dirpart, filename) >= (int)sizeof(projpath)) {
        printf("Path conversion produced too long a path for '%s'; refusing.\n", filespec);
        fclose(f);
        return;
    }

    /* Normalize leading ./ or similar not expected; refuse if path doesn't start with "src/" */
    if (strncmp(projpath, "src/", 4) != 0) {
        printf("Filespec conversion produced unexpected project path '%s'; refusing.\n", projpath);
        fclose(f);
        return;
    }

    /* Open source file and print context */
    FILE *src = fopen(projpath, "r");
    if (!src) {
        printf("Could not open source file '%s': %s; refusing.\n", projpath, strerror(errno));
        fclose(f);
        return;
    }

    int cur = 1;
    int start = target_line - 2;
    if (start < 1) start = 1;
    int end = target_line + 2;
    char lbuf[1024];
    int saw_target = 0;
    /* accumulate context for prompt */
    char contextbuf[8192] = {0};
    size_t ctx_pos = 0;

    printf("Referenced file: %s\n", projpath);
    printf("Referenced line: %d\n", target_line);
    printf("Context (lines %d..%d):\n", start, end);

    while (fgets(lbuf, sizeof(lbuf), src)) {
        if (cur >= start && cur <= end) {
            /* ensure newline */
            size_t llen = strlen(lbuf);
            if (llen == 0 || lbuf[llen-1] != '\n') {
                /* print with added newline */
                printf("%6d: %s\n", cur, lbuf);
                /* append to context buffer */
                int written = snprintf(contextbuf + ctx_pos, sizeof(contextbuf) - ctx_pos, "%6d: %s\n", cur, lbuf);
                if (written > 0) ctx_pos += (size_t)written;
            } else {
                printf("%6d: %s", cur, lbuf);
                int written = snprintf(contextbuf + ctx_pos, sizeof(contextbuf) - ctx_pos, "%6d: %s", cur, lbuf);
                if (written > 0) ctx_pos += (size_t)written;
            }
            if (cur == target_line) saw_target = 1;
        }
        if (cur > end) break;
        cur++;
    }

    if (!saw_target) {
        printf("Referenced line %d not found in '%s'; refusing.\n", target_line, projpath);
        fclose(src);
        fclose(f);
        return;
    }

    /* Build a single prompt string containing only: diagnostic, referenced path, referenced line, context,
       and (if available) the captured failed operation's old and new text from
       OVMS_AGENT_FAILED_OPERATIONS.TXT */
    {
        char prompt[65536] = {0};
        /* Try to open and parse OVMS_AGENT_FAILED_OPERATIONS.TXT to capture the failed operation
           that references the same project-relative path. If the file is missing, malformed, or
           contains no matching operation, refuse. */
        char failed_old[32768] = {0};
        char failed_new[32768] = {0};
        int have_failed_operation = 0;

        FILE *opf = fopen("OVMS_AGENT_FAILED_OPERATIONS.TXT", "r");
        if (opf) {
            /* Parse operations file with BEGIN_OPERATION/END_OPERATION blocks.
               Each block contains fields using equals signs: path=, old_text=, new_text=
               old_text and new_text MUST be present entirely on their key line (no multiline values).
               Any unsupported or non-blank unrecognized line inside an operation is treated as a malformed file. */
            char line2[4096];
            int in_op = 0;
            char op_path[512] = {0};

            while (fgets(line2, sizeof(line2), opf)) {
                char *p2 = line2;
                /* trim leading whitespace (spaces and tabs only) */
                while (*p2 == ' ' || *p2 == '\t') p2++;

                /* detect block boundaries */
                if (strcmp(p2, "BEGIN_OPERATION\n") == 0 || strcmp(p2, "BEGIN_OPERATION\r\n") == 0) {
                    in_op = 1;
                    op_path[0] = '\0';
                    failed_old[0] = failed_new[0] = '\0';
                    continue;
                }
                if (in_op && (strcmp(p2, "END_OPERATION\n") == 0 || strcmp(p2, "END_OPERATION\r\n") == 0)) {
                    /* end of this operation: if path matches, we are done */
                    if (op_path[0] != '\0' && strcmp(op_path, projpath) == 0) {
                        have_failed_operation = 1;
                        break;
                    }
                    in_op = 0;
                    continue;
                }

                if (!in_op) continue;

                if (strncmp(p2, "path=", 5) == 0) {
                    char *val = p2 + 5;
                    /* trim trailing newline/carriage return */
                    size_t l = strlen(val);
                    while (l > 0 && (val[l-1] == '\n' || val[l-1] == '\r')) { val[--l] = '\0'; }
                    strncpy(op_path, val, sizeof(op_path)-1);
                    op_path[sizeof(op_path)-1] = '\0';
                    continue;
                }

                if (strncmp(p2, "old_text=", 9) == 0) {
                    char *val = p2 + 9;
                    /* trim trailing newline/carriage return */
                    size_t l = strlen(val);
                    while (l > 0 && (val[l-1] == '\n' || val[l-1] == '\r')) { val[--l] = '\0'; }
                    /* empty values are rejected */
                    if (l == 0 || l >= sizeof(failed_old)) { have_failed_operation = -1; break; }
                    memcpy(failed_old, val, l);
                    failed_old[l] = '\0';
                    /* Do NOT accept continuation lines for old_text; the value must be wholly on this line */
                    continue;
                }

                if (strncmp(p2, "new_text=", 9) == 0) {
                    char *val = p2 + 9;
                    /* trim trailing newline/carriage return */
                    size_t l = strlen(val);
                    while (l > 0 && (val[l-1] == '\n' || val[l-1] == '\r')) { val[--l] = '\0'; }
                    /* empty values are rejected */
                    if (l == 0 || l >= sizeof(failed_new)) { have_failed_operation = -1; break; }
                    memcpy(failed_new, val, l);
                    failed_new[l] = '\0';
                    /* Do NOT accept continuation lines for new_text; the value must be wholly on this line */
                    continue;
                }

                /* Any other non-blank, unrecognized line inside an operation is malformed. */
                if (*p2 != '\n' && *p2 != '\r' && *p2 != '\0') {
                    have_failed_operation = -1;
                    break;
                }
            }
            fclose(opf);
        } else {
            puts("No OVMS_AGENT_FAILED_OPERATIONS.TXT found; refusing.");
            fclose(src);
            fclose(f);
            return;
        }
        if (have_failed_operation == -1) {
            /* malformed */
            fclose(src);
            fclose(f);
            return;
        }
        if (!have_failed_operation) {
            printf("No matching operation for '%s' was found in OVMS_AGENT_FAILED_OPERATIONS.TXT; refusing.\n", projpath);
            fclose(src);
            fclose(f);
            return;
        }

        /* Format: diagnostic
                 <projpath>
                 <target_line>
                 <context>
                 FAILED_OPERATION_OLD_TEXT:
                 <failed_old>
                 FAILED_OPERATION_NEW_TEXT:
                 <failed_new>
                 <instructions> */
        const char *instructions =
            "\n\nINSTRUCTIONS:\n"
            "You MUST output a single JSON object (not free text).\n"
            "This JSON object MUST contain the integer field \"operation_count\" with value 1, and the array field \"operations\" which MUST contain exactly one operation object.\n"
            "The sole operation MUST be a replace_text operation that targets the referenced project-relative file path (the second line of this prompt).\n"
            "The operation object itself MUST be encoded on a single line (i.e. no embedded newlines) and must have these fields exactly and in any order:\n"
            "  { \"op\": \"replace_text\", \"path\": \"<project-relative-path>\", \"old_text\": \"<exact original line>\", \"new_text\": \"<single replacement source line>\" }\n"
            "Requirements for the fields:\n"
            "  - \"operation_count\" MUST be the integer 1.\n"
            "  - \"path\" MUST exactly equal the referenced project-relative path shown above.\n"
            "  - \"old_text\" MUST be a single-line string that exactly equals the failed operation's original source line captured below (match byte-for-byte, excluding the terminating newline).\n"
            "  - \"new_text\" MUST be a single-line string containing one valid replacement source line that, when substituted for the old line, fixes the compiler error on the referenced line.\n"
            "Explicit prohibitions:\n"
            "  - Do NOT perform whole-file replacements. The model MUST NOT replace the entire file contents.\n"
            "  - \"old_text\" and \"new_text\" MUST NOT contain embedded newlines; multiline old_text or new_text is forbidden.\n"
            "  - Do NOT include any additional operations, explanatory text, comments, or metadata; the model output MUST be exactly the required JSON object and nothing else.\n";

        int n = snprintf(prompt, sizeof(prompt), "%s\n%s\n%d\n%s\nFAILED_OPERATION_OLD_TEXT:\n%s\nFAILED_OPERATION_NEW_TEXT:\n%s\n%s",
                         diagnostic, projpath, target_line, contextbuf, failed_old, failed_new, instructions);
        if (n < 0 || n >= (int)sizeof(prompt)) {
            /* truncate safely if overflow */
            prompt[sizeof(prompt)-1] = '\0';
        }

        /* Call agent planner which will save the returned plan itself */
        openai_agent_plan(state, prompt);
    }

    fclose(src);
    fclose(f);
}
