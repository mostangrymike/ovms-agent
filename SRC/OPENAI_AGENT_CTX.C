#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "OPENAI_AGENT_CTX.H"
#include "OPENAI_JSON_WRITE.H"

#define OPENAI_AGENT_CTX_MAX 131072U

static char openai_agent_ctx_buf[OPENAI_AGENT_CTX_MAX];
static size_t openai_agent_ctx_used;

static int openai_ctx_append(const char *text)
{
    size_t length;

    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (length >= OPENAI_AGENT_CTX_MAX - openai_agent_ctx_used) {
        return 0;
    }

    (void)memcpy(openai_agent_ctx_buf + openai_agent_ctx_used,
                 text,
                 length);
    openai_agent_ctx_used += length;
    openai_agent_ctx_buf[openai_agent_ctx_used] = '\0';
    return 1;
}

static int openai_ctx_char(char ch)
{
    if (openai_agent_ctx_used + 1U >= OPENAI_AGENT_CTX_MAX) {
        return 0;
    }

    openai_agent_ctx_buf[openai_agent_ctx_used++] = ch;
    openai_agent_ctx_buf[openai_agent_ctx_used] = '\0';
    return 1;
}

static int openai_ctx_escape(const char *text)
{
    const unsigned char *scan;
    char small[7];

    if (text == NULL) {
        return 0;
    }

    scan = (const unsigned char *)text;
    while (*scan != '\0') {
        switch (*scan) {
        case '"':
            if (!openai_ctx_append("\\\"")) return 0;
            break;
        case '\\':
            if (!openai_ctx_append("\\\\")) return 0;
            break;
        case '\b':
            if (!openai_ctx_append("\\b")) return 0;
            break;
        case '\f':
            if (!openai_ctx_append("\\f")) return 0;
            break;
        case '\n':
            if (!openai_ctx_append("\\n")) return 0;
            break;
        case '\r':
            if (!openai_ctx_append("\\r")) return 0;
            break;
        case '\t':
            if (!openai_ctx_append("\\t")) return 0;
            break;
        default:
            if (*scan < 32U) {
                (void)sprintf(small, "\\u%04x", (unsigned int)*scan);
                if (!openai_ctx_append(small)) return 0;
            } else if (!openai_ctx_char((char)*scan)) {
                return 0;
            }
            break;
        }
        ++scan;
    }

    return 1;
}

static int openai_ctx_sep(void)
{
    if (openai_agent_ctx_used == 0U) {
        return 1;
    }

    return openai_ctx_char(',');
}

static int openai_ctx_write_base(FILE *file,
                                 const char *user_prompt)
{
    if (file == NULL || user_prompt == NULL) {
        return 0;
    }

    if (fputs("[{\"role\":\"user\",\"content\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\"}", file) == EOF) {
        return 0;
    }

    if (openai_agent_ctx_used != 0U) {
        if (fputc(',', file) == EOF ||
            fwrite(openai_agent_ctx_buf,
                   1U,
                   openai_agent_ctx_used,
                   file) != openai_agent_ctx_used) {
            return 0;
        }
    }

    return 1;
}

void openai_agent_ctx_reset(void)
{
    openai_agent_ctx_used = 0U;
    openai_agent_ctx_buf[0] = '\0';
}

int openai_agent_ctx_add_items(const char *items_json)
{
    size_t saved;

    if (items_json == NULL || *items_json == '\0') {
        return 0;
    }

    saved = openai_agent_ctx_used;
    if (!openai_ctx_sep() || !openai_ctx_append(items_json)) {
        openai_agent_ctx_used = saved;
        openai_agent_ctx_buf[saved] = '\0';
        return 0;
    }

    return 1;
}

int openai_agent_ctx_add_tool(const char *call_id,
                              const char *tool_output)
{
    size_t saved;

    if (call_id == NULL || *call_id == '\0' || tool_output == NULL) {
        return 0;
    }

    saved = openai_agent_ctx_used;
    if (!openai_ctx_sep() ||
        !openai_ctx_append("{\"type\":\"function_call_output\",\"call_id\":\"") ||
        !openai_ctx_escape(call_id) ||
        !openai_ctx_append("\",\"output\":\"") ||
        !openai_ctx_escape(tool_output) ||
        !openai_ctx_append("\"}")) {
        openai_agent_ctx_used = saved;
        openai_agent_ctx_buf[saved] = '\0';
        return 0;
    }

    return 1;
}

int openai_agent_ctx_write(FILE *file,
                           const char *user_prompt)
{
    if (!openai_ctx_write_base(file, user_prompt)) {
        return 0;
    }

    return fputc(']', file) != EOF;
}

int openai_agent_ctx_write_final(FILE *file,
                                 const char *user_prompt)
{
    if (!openai_ctx_write_base(file, user_prompt)) {
        return 0;
    }

    return fputc(']', file) != EOF;
}
