#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "LLM.H"
#include "SETTINGS.H"

#define SET_APPROVAL_MAX 32U

static char set_approval_pending[SET_APPROVAL_MAX];
static char set_approval_return[SET_APPROVAL_MAX];

static int set_appr_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)toupper((unsigned char)*left++);
        b = (unsigned char)toupper((unsigned char)*right++);
        if (a != b) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static const char *set_appr_canonical(const char *value)
{
    if (value == NULL || *value == '\0') {
        return NULL;
    }

    if (set_appr_equal_ci(value, "read-only") ||
        set_appr_equal_ci(value, "read_only") ||
        set_appr_equal_ci(value, "read")) {
        return "read-only";
    }

    if (set_appr_equal_ci(value, "workspace") ||
        set_appr_equal_ci(value, "write")) {
        return "workspace";
    }

    if (set_appr_equal_ci(value, "full")) {
        return "full";
    }

    if (set_appr_equal_ci(value, "autopilot")) {
        return "autopilot";
    }

    return NULL;
}

int settings_approval_apply(const char *value)
{
    const char *canonical;
    const char *logical;

    set_approval_pending[0] = '\0';
    canonical = set_appr_canonical(value);
    if (canonical == NULL) {
        return 0;
    }

    logical = getenv("OVMS_AGENT_APPROVAL_POLICY");
    if (logical != NULL && *logical != '\0') {
        (void)strncpy(set_approval_pending, canonical,
                      sizeof(set_approval_pending) - 1U);
        set_approval_pending[sizeof(set_approval_pending) - 1U] = '\0';
        return 1;
    }

    return llm_set_approval(canonical);
}

const char *settings_approval_name(void)
{
    if (set_approval_pending[0] != '\0') {
        (void)strncpy(set_approval_return, set_approval_pending,
                      sizeof(set_approval_return) - 1U);
        set_approval_return[sizeof(set_approval_return) - 1U] = '\0';
        set_approval_pending[0] = '\0';
        return set_approval_return;
    }

    return llm_approval_name();
}