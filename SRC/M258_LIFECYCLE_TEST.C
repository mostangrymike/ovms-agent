#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

int openai_test_mcp_bridge(const char *bridge,
                           const char *transport,
                           const char *target,
                           char *result,
                           size_t result_size);

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
}

static int write_text(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) return 0;
    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
            "M258 lifecycle regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

static void cleanup(void)
{
    remove_all("M258_BRIDGE_OK.COM");
    remove_all("M258_BRIDGE_FAIL.COM");
    remove_all("M258_BRIDGE_BIG.COM");
    remove_all("OVMS_AGENT_MCP_REQUEST.TMP");
    remove_all("OVMS_AGENT_MCP_RESPONSE.TMP");
}

int main(void)
{
    char result[4096];
    static char timeout_17[] = "OVMS_AGENT_MCP_TIMEOUT_SECONDS=17";
    static char output_256[] = "OVMS_AGENT_MCP_OUTPUT_BYTES=256";
    static char output_128[] = "OVMS_AGENT_MCP_OUTPUT_BYTES=128";
    const char *big_line =
        "$ OPEN/WRITE OUT 'P2'\n"
        "$ WRITE OUT \"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"\n"
        "$ CLOSE OUT\n"
        "$ EXIT 1\n";

    cleanup();
    if (!write_text(
            "M258_BRIDGE_OK.COM",
            "$ OPEN/READ IN 'P1'\n"
            "$ OPEN/WRITE OUT 'P2'\n"
            "$ LOOP:\n"
            "$ READ/END=DONE IN LINE\n"
            "$ WRITE OUT LINE\n"
            "$ GOTO LOOP\n"
            "$ DONE:\n"
            "$ CLOSE IN\n"
            "$ CLOSE OUT\n"
            "$ EXIT 1\n") ||
        !write_text("M258_BRIDGE_FAIL.COM", "$ EXIT %X10000002\n") ||
        !write_text("M258_BRIDGE_BIG.COM", big_line)) {
        (void)puts("M258 lifecycle failed: bridge fixture setup.");
        cleanup();
        return 1;
    }

    (void)putenv(timeout_17);
    (void)putenv(output_256);
    result[0] = '\0';
    if (!require_true(
            openai_test_mcp_bridge(
                "@M258_BRIDGE_OK", "http",
                "https://tools.example.test/mcp",
                result, sizeof(result)) &&
            strstr(result, "condition_status=00000001") != NULL &&
            strstr(result, "timeout_seconds=17") != NULL &&
            strstr(result, "output_limit=256") != NULL &&
            strstr(result, "transport=http") != NULL &&
            strstr(result, "target=https://tools.example.test/mcp") != NULL,
            "success metadata and request contract")) {
        cleanup();
        return 1;
    }

    result[0] = '\0';
    if (!require_true(
            !openai_test_mcp_bridge(
                "@M258_BRIDGE_FAIL", "http",
                "https://tools.example.test/mcp",
                result, sizeof(result)) &&
            strstr(result, "condition_status=10000002") != NULL &&
            strstr(result, "outcome=bridge_failed") != NULL,
            "even condition status preservation")) {
        cleanup();
        return 1;
    }

    (void)putenv(output_128);
    result[0] = '\0';
    if (!require_true(
            !openai_test_mcp_bridge(
                "@M258_BRIDGE_BIG", "sse",
                "https://tools.example.test/events",
                result, sizeof(result)) &&
            strstr(result, "condition_status=00000001") != NULL &&
            strstr(result, "output_limit=128") != NULL &&
            strstr(result, "outcome=output_limit_exceeded") != NULL,
            "bounded bridge response")) {
        cleanup();
        return 1;
    }

    cleanup();
    (void)puts("M258 MCP bridge lifecycle regression passed.");
    return 0;
}
