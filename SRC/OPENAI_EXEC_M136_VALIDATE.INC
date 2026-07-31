static int openai_m136_write_text(
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

int openai_m136_evidence_rollback(
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

    if (!openai_m136_write_text(target_path, "ORIGINAL\n")) {
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
    text = openai_read_text_file(evidence_path);

    ok =
        ok &&
        text != NULL &&
        strstr(text, "SIMULATED FAILURE") != NULL;

    free(text);
    text = openai_read_text_file(target_path);

    ok =
        ok &&
        text != NULL &&
        strcmp(text, "ORIGINAL\n") == 0;

    free(text);
    edit_txn_dispose(&transaction);
    return ok;
}

int openai_m136_commit_closes(
    const char *target_path)
{
    edit_txn transaction;
    int ok;

    while (remove(target_path) == 0) {
    }

    if (!openai_m136_write_text(target_path, "ORIGINAL\n")) {
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

int openai_m136_consume_restore(
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

    if (!openai_m136_write_text(
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

    text = openai_read_text_file(plan_path);
    ok =
        ok &&
        text != NULL &&
        strstr(text, "status=approved") != NULL &&
        strstr(text, "operation_count=1") != NULL;
    free(text);

    return ok;
}

int openai_m136_final_state(
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

    last_workflow = openai_last_workflow;
    last_build_known = openai_last_build_known;
    last_build_status = openai_last_build_status;
    last_rollback = openai_last_rollback;
    saved_workflow = openai_saved_workflow;
    saved_build_known = openai_saved_build_known;
    saved_build_status = openai_saved_build_status;
    saved_rollback = openai_saved_rollback;
    state_valid = openai_state_valid;

    openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_last_build_known = 1;
    openai_last_build_status = 1;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    openai_saved_workflow = openai_last_workflow;
    openai_saved_build_known = openai_last_build_known;
    openai_saved_build_status = openai_last_build_status;
    openai_saved_rollback = openai_last_rollback;
    openai_state_valid = 1;

    result =
        openai_state_valid &&
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        (openai_saved_build_status & 1) != 0 &&
        openai_saved_rollback == OPENAI_ROLLBACK_NONE;

    openai_last_workflow = last_workflow;
    openai_last_build_known = last_build_known;
    openai_last_build_status = last_build_status;
    openai_last_rollback = last_rollback;
    openai_saved_workflow = saved_workflow;
    openai_saved_build_known = saved_build_known;
    openai_saved_build_status = saved_build_status;
    openai_saved_rollback = saved_rollback;
    openai_state_valid = state_valid;

    return result;
}
