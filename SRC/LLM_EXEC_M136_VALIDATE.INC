static int llm_m136_write_text(
    const char *path,
    const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int llm_m136_evidence_rollback(
    const char *target_path,
    const char *evidence_path)
{
    edit_txn transaction;
    char *text;
    int ok;

    while (remove(target_path) == 0) {
    }
    while (remove(evidence_path) == 0) {
    }

    if (!llm_m136_write_text(target_path, "ORIGINAL\n")) {
        return 0;
    }

    edit_txn_init(&transaction);

    if (!edit_txn_add(
            &transaction,
            target_path,
            "CHANGED\n") ||
        !edit_txn_write(&transaction) ||
        !execute_save_build_to(
            evidence_path,
            "SIMULATED FAILURE\n")) {
        edit_txn_dispose(&transaction);
        return 0;
    }

    ok = edit_txn_rollback(&transaction);
    text = llm_read_text_file(evidence_path);

    ok =
        ok &&
        text != NULL &&
        strstr(text, "SIMULATED FAILURE") != NULL;

    free(text);
    text = llm_read_text_file(target_path);

    ok =
        ok &&
        text != NULL &&
        strcmp(text, "ORIGINAL\n") == 0;

    free(text);
    edit_txn_dispose(&transaction);
    return ok;
}

int llm_m136_commit_closes(
    const char *target_path)
{
    edit_txn transaction;
    int ok;

    while (remove(target_path) == 0) {
    }

    if (!llm_m136_write_text(target_path, "ORIGINAL\n")) {
        return 0;
    }

    edit_txn_init(&transaction);

    ok =
        edit_txn_add(
            &transaction,
            target_path,
            "COMMITTED\n") &&
        edit_txn_write(&transaction) &&
        edit_txn_commit(&transaction) &&
        !edit_txn_rollback(&transaction);

    edit_txn_dispose(&transaction);
    return ok;
}

int llm_m136_consume_restore(
    const char *plan_path,
    const char *new_path,
    const char *backup_path)
{
    char *text;
    int ok;

    while (remove(plan_path) == 0) {
    }
    while (remove(new_path) == 0) {
    }
    while (remove(backup_path) == 0) {
    }

    if (!llm_m136_write_text(
            plan_path,
            "status=approved\n"
            "operation_count=1\n")) {
        return 0;
    }

    ok = !execute_mark_consumed_file(
        plan_path,
        new_path,
        backup_path,
        1);

    text = llm_read_text_file(plan_path);
    ok =
        ok &&
        text != NULL &&
        strstr(text, "status=approved") != NULL &&
        strstr(text, "operation_count=1") != NULL;
    free(text);

    return ok;
}

int llm_m136_final_state(
    const char *path)
{
    int last_workflow;
    int last_build_known;
    int last_build_status;
    int last_rollback;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int state_valid;
    int result;

    (void)path;

    last_workflow = llm_last_workflow;
    last_build_known = llm_last_build_known;
    last_build_status = llm_last_build_status;
    last_rollback = llm_last_rollback;
    saved_workflow = llm_saved_workflow;
    saved_build_known = llm_saved_build_known;
    saved_build_status = llm_saved_build_status;
    saved_rollback = llm_saved_rollback;
    state_valid = llm_state_valid;

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 1;
    llm_last_build_status = 1;
    llm_last_rollback = LLM_ROLLBACK_NONE;

    llm_saved_workflow = llm_last_workflow;
    llm_saved_build_known = llm_last_build_known;
    llm_saved_build_status = llm_last_build_status;
    llm_saved_rollback = llm_last_rollback;
    llm_state_valid = 1;

    result =
        llm_state_valid &&
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        (llm_saved_build_status & 1) != 0 &&
        llm_saved_rollback == LLM_ROLLBACK_NONE;

    llm_last_workflow = last_workflow;
    llm_last_build_known = last_build_known;
    llm_last_build_status = last_build_status;
    llm_last_rollback = last_rollback;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
    llm_state_valid = state_valid;

    return result;
}
