#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cobol_guard.h"

static int expect_safe(const char *path,
                       const char *before,
                       const char *after,
                       int wanted,
                       const char *name)
{
    char reason[256];
    int actual;

    actual = cobol_edit_safe(path, before, after,
                             reason, sizeof(reason));
    if (actual != wanted) {
        (void)printf("M264 failed: %s (%s)\n",
                     name,
                     reason[0] != '\0' ? reason : "no reason");
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char before[] =
        "       IDENTIFICATION DIVISION.\n"
        "       PROGRAM-ID. GAP007.\n"
        "       PROCEDURE DIVISION.\n"
        "       MAIN-PARA.\n"
        "           IF A = B\n"
        "               DISPLAY \"MATCH\"\n"
        "           ELSE\n"
        "               DISPLAY \"NO MATCH\"\n"
        "           END-IF\n"
        "           STOP RUN.\n"
        "       END PROGRAM GAP007.\n";
    static const char duplicate_if[] =
        "       IDENTIFICATION DIVISION.\n"
        "       PROGRAM-ID. GAP007.\n"
        "       PROCEDURE DIVISION.\n"
        "       MAIN-PARA.\n"
        "           IF A = B\n"
        "           IF A = B\n"
        "               DISPLAY \"MATCH\"\n"
        "           ELSE\n"
        "               DISPLAY \"NO MATCH\"\n"
        "           END-IF\n"
        "           STOP RUN.\n"
        "       END PROGRAM GAP007.\n";
    static const char balanced_if[] =
        "       IDENTIFICATION DIVISION.\n"
        "       PROGRAM-ID. GAP007.\n"
        "       PROCEDURE DIVISION.\n"
        "       MAIN-PARA.\n"
        "           IF A = B\n"
        "               IF C = D\n"
        "                   DISPLAY \"INNER\"\n"
        "               END-IF\n"
        "               DISPLAY \"MATCH\"\n"
        "           ELSE\n"
        "               DISPLAY \"NO MATCH\"\n"
        "           END-IF\n"
        "           STOP RUN.\n"
        "       END PROGRAM GAP007.\n";
    static const char display_edit[] =
        "       IDENTIFICATION DIVISION.\n"
        "       PROGRAM-ID. GAP007.\n"
        "       PROCEDURE DIVISION.\n"
        "       MAIN-PARA.\n"
        "           IF A = B\n"
        "               DISPLAY \"MATCHED\"\n"
        "           ELSE\n"
        "               DISPLAY \"NO MATCH\"\n"
        "           END-IF\n"
        "           STOP RUN.\n"
        "       END PROGRAM GAP007.\n";
    static const char duplicate_para[] =
        "       IDENTIFICATION DIVISION.\n"
        "       PROGRAM-ID. GAP007.\n"
        "       PROCEDURE DIVISION.\n"
        "       MAIN-PARA.\n"
        "       MAIN-PARA.\n"
        "           IF A = B\n"
        "               DISPLAY \"MATCH\"\n"
        "           ELSE\n"
        "               DISPLAY \"NO MATCH\"\n"
        "           END-IF\n"
        "           STOP RUN.\n"
        "       END PROGRAM GAP007.\n";

    if (!expect_safe("M264.COB", before, duplicate_if, 0,
                     "duplicated IF accepted") ||
        !expect_safe("M264.COB", before, balanced_if, 1,
                     "balanced nested IF rejected") ||
        !expect_safe("M264.COB", before, display_edit, 1,
                     "ordinary COBOL text edit rejected") ||
        !expect_safe("M264.COB", before, duplicate_para, 0,
                     "duplicated paragraph accepted") ||
        !expect_safe("M264.C", before, duplicate_if, 1,
                     "non-COBOL file affected")) {
        return EXIT_FAILURE;
    }

    (void)puts("M264 COBOL structural guard regression passed.");
    return EXIT_SUCCESS;
}
