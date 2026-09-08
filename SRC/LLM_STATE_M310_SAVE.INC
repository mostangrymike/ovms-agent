void llm_m310_state_save_raw(void);

static int llm_m310_runtime_state_default(void)
{
    return
        llm_last_workflow == LLM_WORKFLOW_NONE &&
        llm_last_build_known == 0 &&
        llm_last_build_status == 0 &&
        llm_last_rollback == LLM_ROLLBACK_NONE;
}

void llm_state_save(void)
{
    FILE *probe;

    if (!llm_state_loaded &&
        llm_m310_runtime_state_default()) {
        errno = 0;
        probe = fopen(LLM_STATE_FILE, "r");

        if (probe != NULL) {
            (void)fclose(probe);
            llm_load_state();

            /* Never replace an existing unreadable/invalid state with
             * process defaults just because a log event happened first. */
            if (!llm_state_valid) {
                return;
            }
        } else if (errno != ENOENT) {
            return;
        }
    }

    llm_m310_state_save_raw();
}

/* Rename the legacy implementation that follows in LLM_STATE_CORE.C. */
#define llm_state_save llm_m310_state_save_raw
