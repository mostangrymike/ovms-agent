#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_AGENT_CTX.H"
#include "LLM_JSON_PARSE.H"
#include "LLM_JSON_WRITE.H"

#define LLM_AGENT_CTX_MAX 131072U

static char llm_agent_ctx_buf[LLM_AGENT_CTX_MAX];
static size_t llm_agent_ctx_used;
static size_t llm_agent_ctx_recent_saved;
static size_t llm_agent_ctx_recent_start;
static size_t llm_agent_ctx_recent_end;
static int llm_agent_ctx_recent_valid;

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

static int llm_ctx_item_keep(char *start,
                             char *end,
                             const char *call_id,
                             int *matched)
{
    char saved;
    const char *value;
    char *type;
    char *item_call_id;
    int keep;

    if (start == NULL || end == NULL || call_id == NULL ||
        matched == NULL || end < start) {
        return 0;
    }

    saved = *end;
    *end = '\0';
    value = find_string_value(start, "type");
    type = value != NULL ? json_decode_string(value, NULL) : NULL;

    if (type == NULL || strcmp(type, "function_call") != 0) {
        keep = 1;
    } else {
        value = find_string_value(start, "call_id");
        item_call_id = value != NULL ?
            json_decode_string(value, NULL) : NULL;
        keep = item_call_id != NULL &&
            strcmp(item_call_id, call_id) == 0;
        if (keep) {
            ++(*matched);
        }
        free(item_call_id);
    }

    free(type);
    *end = saved;
    return keep;
}

static int llm_ctx_filter_recent(const char *call_id)
{
    size_t source;
    size_t destination;
    unsigned int kept;
    int matched;

    if (!llm_agent_ctx_recent_valid ||
        call_id == NULL || *call_id == '\0') {
        return 0;
    }

    source = llm_agent_ctx_recent_start;
    destination = llm_agent_ctx_recent_saved;
    kept = 0U;
    matched = 0;

    while (source < llm_agent_ctx_recent_end) {
        size_t item_start;
        size_t item_end;
        int object_depth;
        int array_depth;
        int in_string;
        int escaped;
        int keep;

        while (source < llm_agent_ctx_recent_end &&
               (llm_agent_ctx_buf[source] == ' ' ||
                llm_agent_ctx_buf[source] == '\t' ||
                llm_agent_ctx_buf[source] == '\r' ||
                llm_agent_ctx_buf[source] == '\n' ||
                llm_agent_ctx_buf[source] == ',')) {
            ++source;
        }

        if (source >= llm_agent_ctx_recent_end) {
            break;
        }

        item_start = source;
        object_depth = 0;
        array_depth = 0;
        in_string = 0;
        escaped = 0;

        while (source < llm_agent_ctx_recent_end) {
            char ch;

            ch = llm_agent_ctx_buf[source];
            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (ch == '\\') {
                    escaped = 1;
                } else if (ch == '"') {
                    in_string = 0;
                }
            } else if (ch == '"') {
                in_string = 1;
            } else if (ch == '{') {
                ++object_depth;
            } else if (ch == '}') {
                --object_depth;
            } else if (ch == '[') {
                ++array_depth;
            } else if (ch == ']') {
                --array_depth;
            } else if (ch == ',' &&
                       object_depth == 0 &&
                       array_depth == 0) {
                break;
            }
            ++source;
        }

        item_end = source;
        keep = llm_ctx_item_keep(
            llm_agent_ctx_buf + item_start,
            llm_agent_ctx_buf + item_end,
            call_id,
            &matched
        );

        if (keep) {
            size_t length;

            if (destination > 0U) {
                llm_agent_ctx_buf[destination++] = ',';
            }
            length = item_end - item_start;
            if (length > 0U && destination != item_start) {
                (void)memmove(
                    llm_agent_ctx_buf + destination,
                    llm_agent_ctx_buf + item_start,
                    length
                );
            }
            destination += length;
            ++kept;
        }

        if (source < llm_agent_ctx_recent_end &&
            llm_agent_ctx_buf[source] == ',') {
            ++source;
        }
    }

    llm_agent_ctx_recent_valid = 0;

    if (matched != 1 || kept == 0U) {
        llm_agent_ctx_used = llm_agent_ctx_recent_saved;
        llm_agent_ctx_buf[llm_agent_ctx_used] = '\0';
        return 0;
    }

    llm_agent_ctx_used = destination;
    llm_agent_ctx_buf[llm_agent_ctx_used] = '\0';
    return 1;
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
    llm_agent_ctx_recent_saved = 0U;
    llm_agent_ctx_recent_start = 0U;
    llm_agent_ctx_recent_end = 0U;
    llm_agent_ctx_recent_valid = 0;
}

int llm_agent_ctx_add_items(const char *items_json)
{
    size_t saved;
    size_t start;

    if (items_json == NULL || *items_json == '\0') {
        return 0;
    }

    saved = llm_agent_ctx_used;
    if (!llm_ctx_sep()) {
        return 0;
    }
    start = llm_agent_ctx_used;

    if (!llm_ctx_append(items_json)) {
        llm_agent_ctx_used = saved;
        llm_agent_ctx_buf[saved] = '\0';
        return 0;
    }

    llm_agent_ctx_recent_saved = saved;
    llm_agent_ctx_recent_start = start;
    llm_agent_ctx_recent_end = llm_agent_ctx_used;
    llm_agent_ctx_recent_valid = 1;
    return 1;
}

int llm_agent_ctx_add_tool(const char *call_id,
                           const char *tool_output)
{
    size_t saved;

    if (call_id == NULL || *call_id == '\0' || tool_output == NULL) {
        return 0;
    }

    if (!llm_ctx_filter_recent(call_id)) {
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
