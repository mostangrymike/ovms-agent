#include <stdio.h>
#include <string.h>

#include "LLM_STREAM_POLICY_M300.INC"

static int m300_expect(int condition, const char *message)
{
    if (!condition) {
        (void)printf("M300 stream policy regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char plain_request[] =
        "{\"model\":\"test\",\"input\":\"hello\"}";
    static const char tool_request[] =
        "{\"model\":\"test\",\"tools\":[],\"input\":\"hello\"}";
    static const char prompt_mentions_tools[] =
        "{\"model\":\"test\",\"input\":\"literal \\\"tools\\\": text\"}";

    if (!m300_expect(m300_stream_request_ok(plain_request),
                     "plain request should be eligible") ||
        !m300_expect(!m300_stream_request_ok(tool_request),
                     "tool request must be blocking") ||
        !m300_expect(m300_stream_request_ok(prompt_mentions_tools),
                     "escaped prompt text must not look like a tools field") ||
        !m300_expect(!m300_stream_request_ok(NULL),
                     "missing request must fail closed") ||
        !m300_expect(!m300_stream_request_ok(""),
                     "empty request must fail closed")) {
        return 2;
    }

    (void)puts("M300 stream policy regression passed.");
    return 1;
}
