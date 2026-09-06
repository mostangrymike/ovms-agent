#include <stdio.h>

#include "LLM_PATH.H"

static int expect_path(const char *path, int expected)
{
    return llm_path_is_safe(path) == expected;
}

static int expect_sensitive(const char *path, int expected)
{
    return llm_path_is_sensitive(path) == expected;
}

static int expect_hidden(const char *path, int expected)
{
    return llm_listing_entry_hidden(path) == expected;
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
        expect_path("SYS$LOGIN:LOGIN.COM", 0) &&
        expect_path("OVMS_AGENT_TRANSCRIPT.DAT", 0) &&
        expect_path("ovms_agent_sessions.dat;7", 0) &&
        expect_path("[.SUB]OVMS_AGENT_SESSION.CUR;2", 0) &&
        expect_path("OVMS_AGENT.STATE", 0) &&
        expect_path("OVMS_AGENT_FAILED_BUILD.TXT", 0) &&
        expect_path("OVMS_AGENT_FAILED_OPERATIONS.TXT;3", 0) &&
        expect_path("OVMS_AGENT_LANGUAGE.MD", 1) &&
        expect_path("SRC/OVMS_AGENT_LANGUAGE.MD", 1) &&
        expect_sensitive("OVMS_AGENT_TRANSCRIPT.DAT", 0) &&
        expect_sensitive("OVMS_AGENT_RESPONSE.JSON", 0) &&
        expect_sensitive("OPENAIKEY.TXT", 1) &&
        expect_hidden("OVMS_AGENT_TRANSCRIPT.DAT;9", 1) &&
        expect_hidden("ovms_agent_activity.log", 1) &&
        expect_hidden("OVMS_AGENT_LANGUAGE.MD", 0);

    if (!ok) {
        (void)puts("M304 path/isolation regression failed.");
        return 2;
    }

    (void)puts("M304 path/isolation regression passed.");
    return 1;
}
