/*
 * M300 stream eligibility policy.
 *
 * Streaming is presentation-only and must never be required for tool-bearing
 * agent workflows.  The persistent streaming setting is evaluated by the
 * transport wrapper; this helper decides only whether the serialized request
 * itself is eligible for optional SSE delivery.
 */
static int m300_stream_request_ok(const char *request)
{
    if (request == NULL || *request == '\0') {
        return 0;
    }

    return strstr(request, "\"tools\":") == NULL;
}
