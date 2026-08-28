/* M267 neutral compatibility boundary for the mature M258 parity body. */
/* M268 overlays a scoped AUTOPILOT session policy without changing the
 * mature approval engine.  The base engine is deliberately kept at
 * WORKSPACE while AUTOPILOT is active, so every FULL gate remains closed.
 */
#include "SETTINGS.H"

#define llm_approval_name llm_approval_name_base
#define llm_approval_text llm_approval_text_base
#define llm_show_approval llm_show_approval_base
#define llm_set_approval llm_set_approval_base
#define llm_set_approval_cmd llm_set_approval_cmd_base
#define llm_reset_approval llm_reset_approval_base
#include "LLM_PARITY_M258_CORE.C"
#undef llm_approval_name
#undef llm_approval_text
#undef llm_show_approval
#undef llm_set_approval
#undef llm_set_approval_cmd
#undef llm_reset_approval

static int m280_approval_base_default(void)
{
    char output[1024];

    if (!llm_approval_text_base(output, sizeof(output))) {
        return 0;
    }

    return strstr(output, "\nSource: default\n") != NULL;
}

static const char *m280_saved_approval(void)
{
    const char *value;

    if (!m280_approval_base_default() ||
        !settings_is_saved("approval_policy")) {
        return NULL;
    }

    value = settings_get("approval_policy");
    if (value == NULL) {
        return NULL;
    }

    if (strcmp(value, "read-only") == 0 ||
        strcmp(value, "workspace") == 0 ||
        strcmp(value, "full") == 0) {
        return value;
    }

    return NULL;
}

static const char *m280_approval_name_base(void)
{
    const char *saved;

    saved = m280_saved_approval();
    return saved != NULL ? saved : llm_approval_name_base();
}

static int m280_approval_text_base(char *output, size_t output_size)
{
    const char *saved;
    int written;

    saved = m280_saved_approval();
    if (saved == NULL) {
        return llm_approval_text_base(output, output_size);
    }

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent approval policy\n"
        "--------------------------\n"
        "Policy: %s\n"
        "Source: saved settings\n"
        "Environment: OVMS_AGENT_APPROVAL_POLICY\n"
        "Policies: read-only, workspace, full\n"
        "Note: write and DCL gates remain authoritative.\n",
        saved
    );

    return written >= 0 && (size_t)written < output_size;
}

#include "LLM_CREATE_CONTEXT_M277.INC"
#include "LLM_AUTOPILOT_POLICY.INC"
