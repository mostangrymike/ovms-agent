/* M268 scoped approval overlay.
 *
 * AUTOPILOT is intentionally implemented on top of the mature WORKSPACE
 * policy.  The base policy therefore never satisfies a FULL check while the
 * overlay is active.  Only AGENT/AUTOPILOT interprets this overlay as
 * permission to suppress local plan approval prompts.
 */
static int m268_autopilot_policy = 0;

static int m268_policy_arg_auto(const char *arguments)
{
    const char *value;

    value = llm_skip_ws(arguments);
    return *value != '\0' &&
           strchr(value, ' ') == NULL &&
           strchr(value, '\t') == NULL &&
           llm_equal_ci(value, "AUTOPILOT");
}

const char *llm_approval_name(void)
{
    if (m268_autopilot_policy) {
        return "autopilot";
    }

    return llm_approval_name_base();
}

int llm_approval_text(char *output, size_t output_size)
{
    int written;

    if (!m268_autopilot_policy) {
        return llm_approval_text_base(output, output_size);
    }

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent approval policy\n"
        "--------------------------\n"
        "Policy: autopilot\n"
        "Source: session override\n"
        "Environment: OVMS_AGENT_APPROVAL_POLICY\n"
        "Policies: read-only, workspace, autopilot, full\n"
        "Autopilot: local workspace writes and controlled rebuilds only.\n"
        "Safety floor: PUSH, GitHub writes, MCP/external operations, and "
        "all FULL-gated actions remain manual.\n"
    );

    return written >= 0 && (size_t)written < output_size;
}

void llm_show_approval(void)
{
    char output[2048];

    if (!llm_approval_text(output, sizeof(output))) {
        (void)puts("Unable to show approval policy.");
        return;
    }

    (void)fputs(output, stdout);
}

int llm_set_approval(const char *arguments)
{
    int ok;

    if (m268_policy_arg_auto(arguments)) {
        /* Hard floor: the mature policy engine sees WORKSPACE, never FULL. */
        ok = llm_set_approval_base("workspace");
        if (ok) {
            m268_autopilot_policy = 1;
        }
        return ok;
    }

    m268_autopilot_policy = 0;
    return llm_set_approval_base(arguments);
}

void llm_set_approval_cmd(const char *arguments)
{
    if (!llm_set_approval(arguments)) {
        (void)puts(
            "Usage: AGENT/APPROVAL/SET "
            "<read-only|workspace|autopilot|full>"
        );
        return;
    }

    (void)printf(
        "Session approval policy set to %s.\n",
        llm_approval_name()
    );

    if (m268_autopilot_policy) {
        (void)puts(
            "AUTOPILOT may auto-approve local writes and controlled rebuilds; "
            "FULL-gated operations remain manual."
        );
    }
}

void llm_reset_approval(void)
{
    m268_autopilot_policy = 0;
    llm_reset_approval_base();
}

/* Focused M268 regression hook.  A true result proves that the overlay is
 * represented to the mature engine as WORKSPACE rather than FULL.
 */
int llm_m268_policy_floor(void)
{
    int saved;
    int ok;

    saved = m268_autopilot_policy;
    m268_autopilot_policy = 1;
    ok = strcmp(llm_approval_name(), "autopilot") == 0 &&
         strcmp(llm_approval_name_base(), "full") != 0;
    m268_autopilot_policy = saved;
    return ok;
}
