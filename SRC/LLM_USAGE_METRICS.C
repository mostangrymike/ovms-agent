#include <stdio.h>

#include "LLM_LOG.H"
#include "LLM_USAGE.H"

void m273_show_metrics(void)
{
    llm_usage_stats stats;

    llm_show_metrics();
    llm_usage_get(&stats);

    (void)puts("");
    (void)puts("Provider token usage (current process)");
    (void)puts("--------------------------------------");

    if (stats.last_known) {
        (void)printf(
            "Latest response: input=%lu output=%lu total=%lu\n",
            stats.last_input,
            stats.last_output,
            stats.last_total
        );
    } else {
        (void)puts("Latest response: unavailable");
    }

    (void)printf(
        "Responses with usage: %lu\n",
        stats.requests
    );
    (void)printf(
        "Session tokens: input=%lu output=%lu total=%lu\n",
        stats.session_input,
        stats.session_output,
        stats.session_total
    );
    (void)puts(
        "Monetary cost: unavailable (no trusted provider cost captured)"
    );
}
