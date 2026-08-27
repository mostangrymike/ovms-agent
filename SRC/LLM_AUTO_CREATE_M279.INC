/* M279: permit bounded multi-file scaffolding without reopening replacement loops. */

#define LLM_AUTO_CREATES_DEFAULT 8U
#define LLM_AUTO_CREATES_MAX 8U

int llm_auto_allow_create(void)
{
    unsigned int limit;

    if (m268_auto_outer_active) {
        limit = llm_auto_write_cfg();
    } else {
        limit = llm_auto_env_limit(
            "OVMS_AGENT_AUTO_CREATES",
            LLM_AUTO_CREATES_DEFAULT,
            LLM_AUTO_CREATES_MAX
        );
    }

    if (llm_auto_cur_writes >= limit) {
        llm_auto_write_blocked = 1;
        return 0;
    }

    ++llm_auto_cur_writes;
    return 1;
}
