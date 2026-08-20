#include "llm_internal.h"

#define M258_NET_HOST_MAX 256U
#define M258_NET_RULE_MAX 1024U

static char m258_net_once[M258_NET_HOST_MAX];

static int m258_ci_char(int ch)
{
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
    return ch;
}

static int m258_ci_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) return 0;
    while (*left != '\0' && *right != '\0') {
        if (m258_ci_char((unsigned char)*left) !=
            m258_ci_char((unsigned char)*right)) return 0;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int m258_host_from_url(const char *url,
                              char *host,
                              size_t host_size)
{
    const char *start;
    const char *end;
    const char *colon;
    size_t length;

    if (url == NULL || host == NULL || host_size == 0U) return 0;
    if (strncmp(url, "https://", 8) == 0) start = url + 8;
    else if (strncmp(url, "http://", 7) == 0) start = url + 7;
    else return 0;

    if (*start == '\0' || strchr(start, '@') != NULL) return 0;
    end = start;
    while (*end != '\0' && *end != '/' && *end != '?' && *end != '#') ++end;
    if (end == start) return 0;

    colon = start;
    while (colon < end && *colon != ':') ++colon;
    if (colon < end) end = colon;
    length = (size_t)(end - start);
    if (length == 0U || length >= host_size) return 0;

    (void)memcpy(host, start, length);
    host[length] = '\0';
    return 1;
}

static int m258_rule_match(const char *host,
                           const char *rule,
                           size_t rule_length)
{
    size_t host_length;
    size_t suffix_length;
    size_t index;

    if (host == NULL || rule == NULL || rule_length == 0U) return 0;

    if (rule_length > 2U && rule[0] == '*' && rule[1] == '.') {
        const char *suffix = rule + 1;
        suffix_length = rule_length - 1U;
        host_length = strlen(host);
        if (host_length <= suffix_length) return 0;
        for (index = 0U; index < suffix_length; ++index) {
            if (m258_ci_char((unsigned char)host[host_length - suffix_length + index]) !=
                m258_ci_char((unsigned char)suffix[index])) return 0;
        }
        return 1;
    }

    if (strlen(host) != rule_length) return 0;
    for (index = 0U; index < rule_length; ++index) {
        if (m258_ci_char((unsigned char)host[index]) !=
            m258_ci_char((unsigned char)rule[index])) return 0;
    }
    return 1;
}

static int m258_list_match(const char *host, const char *list)
{
    const char *cursor;

    if (host == NULL || list == NULL) return 0;
    cursor = list;
    while (*cursor != '\0') {
        const char *start;
        size_t length;

        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == ',' || *cursor == ';') ++cursor;
        start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' &&
               *cursor != ',' && *cursor != ';') ++cursor;
        length = (size_t)(cursor - start);
        if (length > 0U && m258_rule_match(host, start, length)) return 1;
    }
    return 0;
}

int llm_net_allow_once(const char *domain)
{
    size_t length;

    if (domain == NULL || *domain == '\0' ||
        strcmp(openai_approval_name(), "full") != 0) return 0;
    length = strlen(domain);
    if (length >= sizeof(m258_net_once) || strchr(domain, '/') != NULL ||
        strchr(domain, ':') != NULL || strchr(domain, '@') != NULL ||
        strchr(domain, '*') != NULL) return 0;
    (void)strcpy(m258_net_once, domain);
    return 1;
}

int llm_net_check(const char *url, char *detail, size_t detail_size)
{
    const char *allow;
    const char *deny;
    char host[M258_NET_HOST_MAX];
    const char *reason;
    int allowed;
    int written;

    if (detail == NULL || detail_size == 0U) return 0;
    if (!m258_host_from_url(url, host, sizeof(host))) {
        written = snprintf(detail, detail_size,
            "Network policy refused invalid HTTP/SSE endpoint.");
        if (written >= 0 && (size_t)written < detail_size) {
            openai_log_event("NETWORK", detail, 2);
        }
        return 0;
    }

    allow = getenv("OVMS_AGENT_NET_ALLOW");
    deny = getenv("OVMS_AGENT_NET_DENY");
    allowed = 0;
    reason = "default deny";

    if (m258_list_match(host, deny)) {
        reason = "explicit deny";
    } else if (m258_list_match(host, allow)) {
        allowed = 1;
        reason = "allow list";
    } else if (m258_net_once[0] != '\0' &&
               m258_ci_equal(host, m258_net_once)) {
        allowed = 1;
        reason = "one-shot exception";
        m258_net_once[0] = '\0';
    }

    written = snprintf(detail, detail_size,
        "Network policy: host=%s decision=%s reason=%s",
        host, allowed ? "allow" : "deny", reason);
    if (written < 0 || (size_t)written >= detail_size) return 0;
    openai_log_event("NETWORK", detail, allowed ? 1 : 2);
    return allowed;
}

int llm_net_policy_text(char *output, size_t output_size)
{
    const char *allow;
    const char *deny;
    int written;

    if (output == NULL || output_size == 0U) return 0;
    allow = getenv("OVMS_AGENT_NET_ALLOW");
    deny = getenv("OVMS_AGENT_NET_DENY");
    written = snprintf(output, output_size,
        "OVMS Agent network policy\n"
        "-------------------------\n"
        "Default: deny\n"
        "Allow:   %s\n"
        "Deny:    %s\n"
        "One-shot exception: %s\n",
        allow != NULL && *allow != '\0' ? allow : "(none)",
        deny != NULL && *deny != '\0' ? deny : "(none)",
        m258_net_once[0] != '\0' ? m258_net_once : "(none)");
    return written >= 0 && (size_t)written < output_size;
}

void llm_test_net_reset(void)
{
    m258_net_once[0] = '\0';
}
