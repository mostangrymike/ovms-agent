#define M291_PATCH_CONFIRM_TMP "OVMS_AGENT_CONFIRM_PATCH.TMP"

static int m291_patch_confirm_override = -1;

void llm_patch_test_confirm(int answer)
{
    if (answer < 0) {
        m291_patch_confirm_override = -1;
    } else {
        m291_patch_confirm_override = answer != 0 ? 1 : 0;
    }
}

static void m291_patch_remove_confirm_tmp(void)
{
    while (remove(M291_PATCH_CONFIRM_TMP) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static int m291_patch_prepare_json(
    const char *arguments,
    char *output,
    size_t output_size,
    char **path_out,
    char **body_out)
{
    char *path;
    char *body;
    FILE *file;
    size_t body_length;
    int ok;

    if (output == NULL || output_size == 0U ||
        path_out == NULL || body_out == NULL) {
        return 0;
    }

    *path_out = NULL;
    *body_out = NULL;
    path = patch_json_string(arguments, "path");
    body = patch_json_string(arguments, "patch");

    if (path == NULL || body == NULL) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: path and patch are required."
        );
        return 0;
    }

    if (!safe_target(path)) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unsafe or invalid target."
        );
        return 0;
    }

    m291_patch_remove_confirm_tmp();
    file = fopen(M291_PATCH_CONFIRM_TMP, "w");
    if (file == NULL) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unable to create confirmation spec."
        );
        return 0;
    }

    body_length = strlen(body);
    if (fprintf(file, "TARGET %s\n", path) < 0 ||
        fputs(body, file) == EOF ||
        (body_length > 0U && body[body_length - 1U] != '\n' &&
         fputc('\n', file) == EOF) ||
        fclose(file) != 0) {
        m291_patch_remove_confirm_tmp();
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unable to write confirmation spec."
        );
        return 0;
    }

    ok = llm_patch_validate(
        M291_PATCH_CONFIRM_TMP,
        output,
        output_size
    );

    if (ok && ansi_term_enabled()) {
        m273_patch_preview(M291_PATCH_CONFIRM_TMP);
    }

    m291_patch_remove_confirm_tmp();
    if (!ok) {
        free(path);
        free(body);
        return 0;
    }

    *path_out = path;
    *body_out = body;
    return 1;
}

int llm_patch_apply_json(
    const char *arguments,
    char *output,
    size_t output_size)
{
    char *path;
    char *body;
    char answer[32];
    int approved;
    int result;

    if (llm_last_workflow != LLM_WORKFLOW_WRITE) {
        return m291_patch_apply_json_core(
            arguments,
            output,
            output_size
        );
    }

    path = NULL;
    body = NULL;
    if (!m291_patch_prepare_json(
            arguments,
            output,
            output_size,
            &path,
            &body)) {
        return 0;
    }

    (void)puts(output);
    if (!ansi_term_enabled()) {
        (void)printf("Structured patch target: %s\n", path);
        (void)puts("Proposed structured patch:");
        (void)puts("--------------------------");
        (void)fputs(body, stdout);
        if (*body == '\0' || body[strlen(body) - 1U] != '\n') {
            (void)putchar('\n');
        }
        (void)puts("--------------------------");
    }

    approved = 0;
    if (m291_patch_confirm_override >= 0) {
        approved = m291_patch_confirm_override;
    } else {
        (void)printf("Apply patch [y/N]? ");
        (void)fflush(stdout);
        if (fgets(answer, sizeof(answer), stdin) != NULL &&
            (answer[0] == 'y' || answer[0] == 'Y')) {
            approved = 1;
        }
    }

    if (!approved) {
        (void)snprintf(
            output,
            output_size,
            "Structured patch declined by local user."
        );
        free(path);
        free(body);
        return 0;
    }

    free(path);
    free(body);
    result = m291_patch_apply_json_core(
        arguments,
        output,
        output_size
    );
    return result;
}
