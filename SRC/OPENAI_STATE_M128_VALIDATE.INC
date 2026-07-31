static void openai_m128_save_all(
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
    *last_workflow = openai_last_workflow;
    *last_build_known = openai_last_build_known;
    *last_build_status = openai_last_build_status;
    *last_rollback = openai_last_rollback;
    *saved_workflow = openai_saved_workflow;
    *saved_build_known = openai_saved_build_known;
    *saved_build_status = openai_saved_build_status;
    *saved_rollback = openai_saved_rollback;
    *loaded = openai_state_loaded;
    *valid = openai_state_valid;
    *recovered = openai_state_recovered;
}

static void openai_m128_restore_all(
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
    openai_last_workflow = last_workflow;
    openai_last_build_known = last_build_known;
    openai_last_build_status = last_build_status;
    openai_last_rollback = last_rollback;
    openai_saved_workflow = saved_workflow;
    openai_saved_build_known = saved_build_known;
    openai_saved_build_status = saved_build_status;
    openai_saved_rollback = saved_rollback;
    openai_state_loaded = loaded;
    openai_state_valid = valid;
    openai_state_recovered = recovered;
}

int openai_m128_load_snapshot(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    openai_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            11,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        openai_m128_restore_all(
            lw, lbk, lbs, lr,
            sw, sbk, sbs, sr,
            loaded, valid, recovered);
        return 0;
    }

    ok =
        openai_state_apply_load(path) &&
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        openai_saved_build_status == 11 &&
        openai_saved_rollback == OPENAI_ROLLBACK_SUCCEEDED;

    (void)openai_state_purge_all(path);
    openai_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int openai_m128_current_isolated(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    openai_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 13;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 0;
    openai_last_build_status = 0;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    ok =
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        openai_saved_build_status == 13 &&
        openai_saved_rollback == OPENAI_ROLLBACK_SUCCEEDED;

    openai_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int openai_m128_save_refreshes(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    openai_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    (void)openai_state_purge_all(path);

    openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_last_build_known = 1;
    openai_last_build_status = 15;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok = openai_state_save_to(
        path,
        openai_last_workflow,
        openai_last_build_known,
        openai_last_build_status,
        openai_last_rollback);

    if (ok) {
        openai_state_snapshot_current();
    }

    ok =
        ok &&
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        openai_saved_build_status == 15 &&
        openai_saved_rollback == OPENAI_ROLLBACK_SUCCEEDED;

    (void)openai_state_purge_all(path);
    openai_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}

int openai_m128_fail_hides_current(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered;
    int ok;

    openai_m128_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered);

    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 17;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = -1;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    openai_state_note_save_result(0);

    ok =
        openai_state_valid == 0 &&
        strcmp(
            openai_persist_workflow(
                openai_state_valid,
                openai_saved_workflow),
            "unavailable") == 0 &&
        strcmp(
            openai_persist_build(
                openai_state_valid,
                openai_saved_build_known,
                openai_saved_build_status),
            "unavailable") == 0 &&
        strcmp(
            openai_persist_rollback(
                openai_state_valid,
                openai_saved_rollback),
            "unavailable") == 0;

    openai_m128_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered);

    return ok;
}
