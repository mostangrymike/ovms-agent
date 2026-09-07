#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

int llm_m307_diag_file(const char *build_path,
                       const char *project_root,
                       char *diagnostic,
                       size_t diagnostic_size,
                       char *project_path,
                       size_t project_path_size,
                       int *target_line);
int llm_m307_failed_op(const char *operations_path,
                       const char *target_path,
                       char *old_text,
                       size_t old_size,
                       char *new_text,
                       size_t new_size);

void llm_agent_plan(agent_state *state, const char *goal)
{
    (void)state;
    (void)goal;
}

static int write_text(const char *path, const char *text)
{
    FILE *file;
    int ok;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    ok = fputs(text, file) != EOF;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
                      "M307 repair regression failed: %s\n",
                      message);
        return 0;
    }
    return 1;
}

static void cleanup(void)
{
    (void)remove("M307_COBOL.TXT");
    (void)remove("M307_CC.TXT");
    (void)remove("M307_FORTRAN.TXT");
    (void)remove("M307_OUTSIDE.TXT");
    (void)remove("M307_FAILED_OPS.TXT");
    (void)remove("M307_BAD_OPS.TXT");
}

int main(void)
{
    const char *root;
    char diagnostic[1024];
    char project_path[512];
    char old_text[32768];
    char new_text[32768];
    int line_number;

    root = "SYS$SYSDEVICE:[MIKE.VMS-COBOL-SANDBOX]";
    cleanup();

    if (!write_text(
            "M307_COBOL.TXT",
            "%COBOL-F-SYN17, Invalid syntax\n"
            "at line number 7 in file SYS$SYSDEVICE:[MIKE.VMS-COBOL-SANDBOX]WC.COB;4\n") ||
        !require_true(
            llm_m307_diag_file(
                "M307_COBOL.TXT", root,
                diagnostic, sizeof(diagnostic),
                project_path, sizeof(project_path),
                &line_number) &&
            strstr(diagnostic, "%COBOL-F-") != NULL &&
            strcmp(project_path, "wc.cob") == 0 &&
            line_number == 7,
            "COBOL root-level diagnostic")) {
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "M307_CC.TXT",
            "%CC-E-UNDECLARED, identifier X is undefined\n"
            "at line number 42 in file SYS$SYSDEVICE:[MIKE.VMS-COBOL-SANDBOX.SRC]MAIN.C;2\n") ||
        !require_true(
            llm_m307_diag_file(
                "M307_CC.TXT", root,
                diagnostic, sizeof(diagnostic),
                project_path, sizeof(project_path),
                &line_number) &&
            strstr(diagnostic, "%CC-E-") != NULL &&
            strcmp(project_path, "src/main.c") == 0 &&
            line_number == 42,
            "CC nested diagnostic")) {
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "M307_FORTRAN.TXT",
            "%FORTRAN-E-SYNTAX, syntax error\n"
            "at line number 3 in file SYS$SYSDEVICE:[MIKE.VMS-COBOL-SANDBOX.LIB]CALC.F90;1\n") ||
        !require_true(
            llm_m307_diag_file(
                "M307_FORTRAN.TXT", root,
                diagnostic, sizeof(diagnostic),
                project_path, sizeof(project_path),
                &line_number) &&
            strstr(diagnostic, "%FORTRAN-E-") != NULL &&
            strcmp(project_path, "lib/calc.f90") == 0 &&
            line_number == 3,
            "non-C nested diagnostic")) {
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "M307_OUTSIDE.TXT",
            "%COBOL-F-SYN17, Invalid syntax\n"
            "at line number 1 in file SYS$SYSDEVICE:[MIKE.OUTSIDE]BAD.COB;1\n") ||
        !require_true(
            !llm_m307_diag_file(
                "M307_OUTSIDE.TXT", root,
                diagnostic, sizeof(diagnostic),
                project_path, sizeof(project_path),
                &line_number),
            "outside-root diagnostic refused")) {
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "M307_FAILED_OPS.TXT",
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_block\n"
            "path=WC.COB\n"
            "BEGIN_OLD_TEXT\n"
            "OLD-ONE\n"
            "\\END_OLD_TEXT\n"
            "OLD-THREE\n"
            "\n"
            "END_OLD_TEXT\n"
            "BEGIN_NEW_TEXT\n"
            "NEW-ONE\n"
            "\\END_NEW_TEXT\n"
            "NEW-THREE\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n") ||
        !require_true(
            llm_m307_failed_op(
                "M307_FAILED_OPS.TXT", "wc.cob",
                old_text, sizeof(old_text),
                new_text, sizeof(new_text)) &&
            strcmp(old_text,
                   "OLD-ONE\nEND_OLD_TEXT\nOLD-THREE\n") == 0 &&
            strcmp(new_text,
                   "NEW-ONE\nEND_NEW_TEXT\nNEW-THREE") == 0,
            "structured multiline failed-operation round trip")) {
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "M307_BAD_OPS.TXT",
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_block\n"
            "path=wc.cob\n"
            "BEGIN_OLD_TEXT\n"
            "OLD\n"
            "END_OLD_TEXT\n"
            "BEGIN_NEW_TEXT\n"
            "TRUNCATED\n") ||
        !require_true(
            !llm_m307_failed_op(
                "M307_BAD_OPS.TXT", "wc.cob",
                old_text, sizeof(old_text),
                new_text, sizeof(new_text)),
            "truncated structured operation refused")) {
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M307 repair portability/serialization regression passed.");
    return EXIT_SUCCESS;
}
