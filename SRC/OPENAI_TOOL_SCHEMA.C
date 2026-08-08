#include <stdio.h>
#include "openai_tool_schema.h"

static int schema_read_tools(FILE *file)
{
    return fputs(
        "{\"type\":\"function\",\"name\":\"list_directory\","
        "\"description\":\"List entries in one project-relative directory. Use dot for the project root.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],\"additionalProperties\":false},\"strict\":true},"
        "{\"type\":\"function\",\"name\":\"read_file\","
        "\"description\":\"Read one project-relative text file.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"],\"additionalProperties\":false},\"strict\":true},"
        "{\"type\":\"function\",\"name\":\"read_file_range\","
        "\"description\":\"Read an inclusive line range from one project-relative text file.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"end_line\":{\"type\":\"integer\",\"minimum\":1}},"
        "\"required\":[\"path\",\"start_line\",\"end_line\"],"
        "\"additionalProperties\":false},\"strict\":true},"
        "{\"type\":\"function\",\"name\":\"search_file\","
        "\"description\":\"Search one project-relative text file for a literal string.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"pattern\":{\"type\":\"string\"}},"
        "\"required\":[\"path\",\"pattern\"],\"additionalProperties\":false},\"strict\":true}",
        file
    ) != EOF;
}

int write_agent_tools(FILE *file)
{
    if (file == NULL) return 0;
    if (fputs("\"tools\":[", file) == EOF) return 0;
    if (!schema_read_tools(file)) return 0;
    return fputc(']', file) != EOF;
}

int write_agent_tools_with_replace(FILE *file)
{
    if (file == NULL) return 0;
    if (fputs("\"tools\":[", file) == EOF) return 0;
    if (!schema_read_tools(file)) return 0;

    return fputs(
        ",{\"type\":\"function\",\"name\":\"replace_text\","
        "\"description\":\"Replace one exact, unique text block in a project-relative file.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"old_text\":{\"type\":\"string\"},\"new_text\":{\"type\":\"string\"}},"
        "\"required\":[\"path\",\"old_text\",\"new_text\"],"
        "\"additionalProperties\":false},\"strict\":true},"
        "{\"type\":\"function\",\"name\":\"replace_lines\","
        "\"description\":\"Replace one inclusive line range in a project-relative text file.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"first_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"last_line\":{\"type\":\"integer\",\"minimum\":1},"
        "\"new_text\":{\"type\":\"string\"}},"
        "\"required\":[\"path\",\"first_line\",\"last_line\",\"new_text\"],"
        "\"additionalProperties\":false},\"strict\":true},"
        "{\"type\":\"function\",\"name\":\"structured_patch\","
        "\"description\":\"Apply multiple exact non-overlapping hunks to one project-relative file. "
        "The patch string contains repeated @@OLD, @@NEW, @@END blocks. "
        "All hunks are validated against the original file before any write.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"patch\":{\"type\":\"string\"}},"
        "\"required\":[\"path\",\"patch\"],\"additionalProperties\":false},\"strict\":true}]",
        file
    ) != EOF;
}

int write_agent_tools_with_create(FILE *file)
{
    if (file == NULL) return 0;
    if (fputs("\"tools\":[", file) == EOF) return 0;
    if (!schema_read_tools(file)) return 0;
    return fputs(
        ",{\"type\":\"function\",\"name\":\"create_file\","
        "\"description\":\"Create one new project-relative text file. The destination must not already exist.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
        "\"content\":{\"type\":\"string\"}},"
        "\"required\":[\"path\",\"content\"],\"additionalProperties\":false},\"strict\":true}]",
        file
    ) != EOF;
}

int write_build_agent_tools(FILE *file)
{
    if (file == NULL) return 0;
    return fputs(
        "\"tools\":[{\"type\":\"function\",\"name\":\"run_build\","
        "\"description\":\"Run the fixed project BUILD.COM command procedure and return captured output and status.\","
        "\"parameters\":{\"type\":\"object\",\"properties\":{},\"required\":[],"
        "\"additionalProperties\":false},\"strict\":true}]",
        file
    ) != EOF;
}
