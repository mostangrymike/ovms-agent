#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "LLM_AGENT_CTX.H"
#include "LLM_JSON_WRITE.H"

#define LLM_AGENT_CTX_MAX 131072U

static char llm_agent_ctx_buf[LLM_AGENT_CTX_MAX];
static size_t llm_agent_ctx_used;

static int llm_ctx_append(const char *text)
{
    size_t length;

    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (length >= LLM_AGENT_CTX_MAX - llm_agent_ctx_used) {
        return 0;
    }

    (void)memcpy(llm_agent_ctx_buf + llm_agent_ctx_used,
                 text,
                 length);
    llm_agent_ctx_used += length;
    llm_agent_ctx_buf[llm_agent_ctx_used] = '\0';
    return 1;
}

static int llm_ctx_char(char ch)
{
    if (llm_agent_ctx_used + 1U >= LLM_AGENT_CTX_MAX) {
        return 0;
    }

    llm_agent_ctx_buf[llm_agent_ctx_used++] = ch;
    llm_agent_ctx_buf[llm_agent_ctx_used] = '\0';
    return 1;
}

static int llm_ctx_escape(const char *text)
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
            if (!llm_ctx_append("\\\"")) return 0;
            break;
        case '\\':
            if (!llm_ctx_append("\\\\")) return 0;
            break;
        case '\b':
            if (!llm_ctx_append("\\b")) return 0;
            break;
        case '\f':
            if (!llm_ctx_append("\\f")) return 0;
            break;
        case '\n':
            if (!llm_ctx_append("\\n")) return 0;
            break;
        case '\r':
            if (!llm_ctx_append("\\r")) return 0;
            break;
        case '\t':
            if (!llm_ctx_append("\\t")) return 0;
            break;
        default:
            if (*scan < 32U) {
                (void)sprintf(small, "\\u%04x", (unsigned int)*scan);
                if (!llm_ctx_append(small)) return 0;
            } else if (!llm_ctx_char((char)*scan)) {
                return 0;
            }
            break;
        }
        ++scan;
    }

    return 1;
}

static int llm_ctx_sep(void)
{
    if (llm_agent_ctx_used == 0U) {
        return 1;
    }

    return llm_ctx_char(',');
}

static int llm_ctx_write_base(FILE *file,
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

    if (llm_agent_ctx_used != 0U) {
        if (fputc(',', file) == EOF ||
            fwrite(llm_agent_ctx_buf,
                   1U,
                   llm_agent_ctx_used,
                   file) != llm_agent_ctx_used) {
            return 0;
        }
    }

    return 1;
}

void llm_agent_ctx_reset(void)
{
    llm_agent_ctx_used = 0U;
    llm_agent_ctx_buf[0] = '\0';
}

int llm_agent_ctx_add_items(const char *items_json)
{
    size_t saved;

    if (items_json == NULL || *items_json == '\0') {
        return 0;
    }

    saved = llm_agent_ctx_used;
    if (!llm_ctx_sep() || !llm_ctx_append(items_json)) {
        llm_agent_ctx_used = saved;
        llm_agent_ctx_buf[saved] = '\0';
        return 0;
    }

    return 1;
}

int llm_agent_ctx_add_tool(const char *call_id,
                           const char *tool_output)
{
    size_t saved;

    if (call_id == NULL || *call_id == '\0' || tool_output == NULL) {
        return 0;
    }

    saved = llm_agent_ctx_used;
    if (!llm_ctx_sep() ||
        !llm_ctx_append("{\"type\":\"function_call_output\",\"call_id\":\"") ||
        !llm_ctx_escape(call_id) ||
        !llm_ctx_append("\",\"output\":\"") ||
        !llm_ctx_escape(tool_output) ||
        !llm_ctx_append("\"}")) {
        llm_agent_ctx_used = saved;
        llm_agent_ctx_buf[saved] = '\0';
        return 0;
    }

    return 1;
}

int llm_agent_ctx_write(FILE *file,
                        const char *user_prompt)
{
    if (!llm_ctx_write_base(file, user_prompt)) {
        return 0;
    }

    return fputc(']', file) != EOF;
}

int llm_agent_ctx_write_final(FILE *file,
                              const char *user_prompt)
{
    if (!llm_ctx_write_base(file, user_prompt)) {
        return 0;
    }

    return fputc(']', file) != EOF;
}
