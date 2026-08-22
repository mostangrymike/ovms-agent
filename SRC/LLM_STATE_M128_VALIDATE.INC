static void llm_m128_save_all(
    int *last_workflow,
    int *last_build_known,
    int *last_build_status,
    int *last_rollback,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback,
    int *loaded,
    int *valid,
    int *recovered)
{
    *last_workflow = llm_last_workflow;
    *last_build_known = llm_last_build_known;
    *last_build_status = llm_last_build_status;
    *last_rollback = llm_last_rollback;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
    *loaded = llm_state_loaded;
    *valid = llm_state_valid;
    *recovered = llm_state_recovered;
}

static void llm_m128_restore_all(
    int last_workflow,
    int last_build_known,
    int last_build_status,
    int last_rollback,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback,
    int loaded,
    int valid,
    int recovered)
{
    llm_last_workflow = last_workflow;
    llm_last_build_known = last_build_known;
    llm_last_build_status = last_build_status;
    llm_last_rollback = last_rollback;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
    llm_state_loaded = loaded;
    llm_state_valid = valid;
    llm_state_recovered = recovered;
}

int llm_m128_load_snapshot(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    llm_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            11,
            LLM_ROLLBACK_SUCCEEDED)) {
        llm_m128_restore_all(
            lw, lbk, lbs, lr,
            sw, sbk, sbs, sr,
            loaded, valid, recovered);
        return 0;
    }

    ok =
        llm_state_apply_load(path) &&
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        llm_saved_build_status == 11 &&
        llm_saved_rollback == LLM_ROLLBACK_SUCCEEDED;

    (void)llm_state_purge_all(path);
    llm_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int llm_m128_current_isolated(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    llm_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 13;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 0;
    llm_last_build_status = 0;
    llm_last_rollback = LLM_ROLLBACK_NONE;

    ok =
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        llm_saved_build_status == 13 &&
        llm_saved_rollback == LLM_ROLLBACK_SUCCEEDED;

    llm_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int llm_m128_save_refreshes(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    llm_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    (void)llm_state_purge_all(path);

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 1;
    llm_last_build_status = 15;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok = llm_state_save_to(
        path,
        llm_last_workflow,
        llm_last_build_known,
        llm_last_build_status,
        llm_last_rollback);

    if (ok) {
        llm_state_snapshot_current();
    }

    ok =
        ok &&
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        llm_saved_build_status == 15 &&
        llm_saved_rollback == LLM_ROLLBACK_SUCCEEDED;

    (void)llm_state_purge_all(path);
    llm_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int llm_m128_fail_hides_current(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    llm_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 17;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = -1;
    llm_last_rollback = LLM_ROLLBACK_NONE;

    llm_state_note_save_result(0);

    ok =
        llm_state_valid == 0 &&
        strcmp(
            llm_persist_workflow(
                llm_state_valid,
                llm_saved_workflow),
            "unavailable") == 0 &&
        strcmp(
            llm_persist_build(
                llm_state_valid,
                llm_saved_build_known,
                llm_saved_build_status),
            "unavailable") == 0 &&
        strcmp(
            llm_persist_rollback(
                llm_state_valid,
                llm_saved_rollback),
            "unavailable") == 0;

    llm_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}
