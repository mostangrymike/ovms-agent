#include <stdio.h>

#include "LLM_PATH.H"

static int expect_path(const char *path, int expected)
{
    return llm_path_is_safe(path) == expected;
}

int main(void)
{
    int ok;

    ok =
        expect_path("SRC/MAIN.C", 1) &&
        expect_path("README.MD;2", 1) &&
        expect_path("[]README.MD;1", 1) &&
        expect_path("[.SUB]FILE.TXT;2", 1) &&
        expect_path("<.SUB-DIR>FILE.TXT;3", 1) &&
        expect_path("[-.OVMS_AGENT]RULES.MD", 0) &&
        expect_path("[.-.OVMS_AGENT]RULES.MD", 0) &&
        expect_path("[.SUB.-.OTHER]FILE.TXT", 0) &&
        expect_path("<-.OVMS_AGENT>RULES.MD", 0) &&
        expect_path("[OTHER]FILE.TXT", 0) &&
        expect_path("<OTHER>FILE.TXT", 0) &&
        expect_path("../LOGIN.COM", 0) &&
        expect_path("SYS$LOGIN:LOGIN.COM", 0);

    if (!ok) {
        (void)puts("M304 path confinement regression failed.");
        return 2;
    }

    (void)puts("M304 path confinement regression passed.");
    return 1;
}
