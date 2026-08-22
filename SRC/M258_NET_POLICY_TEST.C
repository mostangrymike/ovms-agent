#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

static const char *m258_policy_name = "read-only";
static unsigned int m258_log_calls;
static int m258_log_status;
static char m258_log_event[512];

const char *llm_approval_name(void)
{
    return m258_policy_name;
}

void llm_log_event(const char *workflow, const char *event, int status)
{
    if (workflow != NULL && strcmp(workflow, "NETWORK") == 0) {
        ++m258_log_calls;
        m258_log_status = status;
        (void)snprintf(m258_log_event, sizeof(m258_log_event), "%s",
                       event != NULL ? event : "");
    }
}

#include "LLM_NETWORK.C"

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M258 network policy regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    char detail[512];
    char policy[2048];
    static char allow_none[] = "OVMS_AGENT_NET_ALLOW=";
    static char deny_none[] = "OVMS_AGENT_NET_DENY=";
    static char allow_rules[] = "OVMS_AGENT_NET_ALLOW=tools.example.test,*.allowed.test";
    static char deny_rules[] = "OVMS_AGENT_NET_DENY=blocked.allowed.test,tools.example.test";

    (void)putenv(allow_none);
    (void)putenv(deny_none);
    llm_test_net_reset();
    m258_log_calls = 0U;
    m258_log_event[0] = '\0';

    if (!require_true(
            !llm_net_check("https://tools.example.test/mcp", detail, sizeof(detail)) &&
            strstr(detail, "decision=deny") != NULL &&
            strstr(detail, "default deny") != NULL &&
            m258_log_calls == 1U && m258_log_status == 2 &&
            strstr(m258_log_event, "decision=deny") != NULL,
            "default deny and logging")) return 1;

    (void)putenv(allow_rules);
    if (!require_true(
            llm_net_check("https://tools.example.test/mcp", detail, sizeof(detail)) &&
            strstr(detail, "allow list") != NULL &&
            m258_log_status == 1 &&
            strstr(m258_log_event, "decision=allow") != NULL,
            "exact allow and logging")) return 1;

    if (!require_true(
            llm_net_check("https://api.allowed.test/events", detail, sizeof(detail)) &&
            strstr(detail, "allow list") != NULL,
            "wildcard subdomain allow")) return 1;

    if (!require_true(
            !llm_net_check("https://allowed.test/events", detail, sizeof(detail)),
            "wildcard does not match apex")) return 1;

    (void)putenv(deny_rules);
    if (!require_true(
            !llm_net_check("https://tools.example.test/mcp", detail, sizeof(detail)) &&
            strstr(detail, "explicit deny") != NULL,
            "deny overrides allow")) return 1;

    if (!require_true(
            !llm_net_check("https://blocked.allowed.test/mcp", detail, sizeof(detail)) &&
            strstr(detail, "explicit deny") != NULL,
            "explicit deny over wildcard")) return 1;

    (void)putenv(allow_none);
    (void)putenv(deny_none);
    m258_policy_name = "workspace";
    if (!require_true(
            !llm_net_allow_once("once.example.test"),
            "exception requires full approval")) return 1;

    m258_policy_name = "full";
    if (!require_true(llm_net_allow_once("once.example.test"),
                      "full approval exception")) return 1;

    if (!require_true(
            llm_net_policy_text(policy, sizeof(policy)) &&
            strstr(policy, "Default: deny") != NULL &&
            strstr(policy, "once.example.test") != NULL,
            "inspectable pending exception")) return 1;

    if (!require_true(
            llm_net_check("https://once.example.test/tool", detail, sizeof(detail)) &&
            strstr(detail, "one-shot exception") != NULL,
            "one-shot exception use")) return 1;

    if (!require_true(
            !llm_net_check("https://once.example.test/tool", detail, sizeof(detail)) &&
            strstr(detail, "default deny") != NULL,
            "one-shot exception consumed")) return 1;

    if (!require_true(
            !llm_net_check("file://tools.example.test/mcp", detail, sizeof(detail)) &&
            m258_log_status == 2 &&
            strstr(m258_log_event, "invalid HTTP/SSE endpoint") != NULL,
            "non-network URL refused and logged")) return 1;

    llm_test_net_reset();
    (void)putenv(allow_none);
    (void)putenv(deny_none);
    (void)puts("M258 default-deny network policy regression passed.");
    return 0;
}
