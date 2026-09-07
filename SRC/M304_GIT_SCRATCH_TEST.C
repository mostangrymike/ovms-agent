#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M290_GIT_META_COM    "OVMS_AGENT_GIT_META.TMP"
#define M290_GIT_INDEX_TMP   "OVMS_AGENT_GIT_INDEX.TMP"
#define M290_GIT_CACHED_TMP  "OVMS_AGENT_GIT_CACHED.TMP"
#define M290_GIT_OTHER_TMP   "OVMS_AGENT_GIT_OTHER.TMP"
#define M290_GIT_HASH_TMP    "OVMS_AGENT_GIT_HASH.TMP"
#define M290_GIT_TOP_TMP     "OVMS_AGENT_GIT_TOP.TMP"

static char llm_git_status[4096];

static int m290_git_rms_status_base(void)
{
    return 1;
}

#include "LLM_GIT_STATUS_M291.INC"

int main(void)
{
    char status[2048];

    (void)strcpy(
        status,
        "?? ovms_agent_git_meta.tmp\n"
        "?? SRC/Ovms_Agent_Git_Index.Tmp\n"
        "?? ovms_agent_git_cached.tmp\n"
        "?? OVMS_agent_GIT_other.TMP\n"
        "?? ovms_AGENT_git_HASH.tmp\n"
        "?? src/ovms_agent_git_top.tmp\n"
        "?? ovms_agent_session.cur\n"
        "?? SRC/OVMS_AGENT_SESSIONS.DAT\n"
        "?? Ovms_Agent_Transcript.Dat\n"
        "?? OVMS_AGENT_LANGUAGE.MD\n"
        "?? KEEP.TXT\n"
        " M ovms_agent_git_cached.tmp\n"
        " M ovms_agent_session.cur\n"
        "?? ovms_agent_git_cached.tmp.bak");

    if (!llm_git_m291_filter_status(status, sizeof(status)) ||
        strcmp(
            status,
            "?? OVMS_AGENT_LANGUAGE.MD\n"
            "?? KEEP.TXT\n"
            " M ovms_agent_git_cached.tmp\n"
            " M ovms_agent_session.cur\n"
            "?? ovms_agent_git_cached.tmp.bak") != 0) {
        (void)puts("M304 failed: Git runtime/scratch filtering.");
        return EXIT_FAILURE;
    }

    (void)puts("M304 Git scratch case-filter regression passed.");
    (void)puts("M304 Git runtime-artifact isolation regression passed.");
    return EXIT_SUCCESS;
}
