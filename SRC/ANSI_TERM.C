#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <dcdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <starlet.h>

#include "ANSI_TERM.H"

#define ANSI_GIT_DIFF_FILE "OVMS_AGENT_ANSI_DIFF.TMP"

typedef struct ansi_dvi_item {
    unsigned short length;
    unsigned short code;
    void *buffer;
    unsigned short *return_length;
} ansi_dvi_item;

static int ansi_initialized = 0;
static int ansi_enabled = 0;
static int ansi_forced_plain = 0;

static int ansi_logical_defined(const char *name)
{
    const char *value;

    if (name == NULL || *name == '\0') {
        return 0;
    }

    value = getenv(name);
    return value != NULL;
}

static int ansi_output_is_terminal(void)
{
    unsigned int device_class;
    unsigned short class_length;
    unsigned long status;
    ansi_dvi_item items[2];
    $DESCRIPTOR(output_name, "SYS$OUTPUT");

    device_class = 0U;
    class_length = 0U;
    (void)memset(items, 0, sizeof(items));

    items[0].length = (unsigned short)sizeof(device_class);
    items[0].code = DVI$_DEVCLASS;
    items[0].buffer = &device_class;
    items[0].return_length = &class_length;

    status = sys$getdviw(
        0,
        0,
        &output_name,
        items,
        0,
        0,
        0,
        0
    );

    if ((status & 1UL) == 0UL) {
        return 0;
    }

    return device_class == (unsigned int)DC$_TERM;
}

void ansi_term_init(void)
{
    ansi_forced_plain =
        ansi_logical_defined("OVMS_AGENT_PLAIN") ||
        ansi_logical_defined("OVMS_AGENT_NOCOLOR");

    ansi_enabled = 0;

    if (!ansi_forced_plain) {
        ansi_enabled = ansi_output_is_terminal();
    }

    ansi_initialized = 1;
}

static void ansi_term_ensure(void)
{
    if (!ansi_initialized) {
        ansi_term_init();
    }
}

int ansi_term_enabled(void)
{
    ansi_term_ensure();
    return ansi_enabled;
}

int ansi_term_plain_forced(void)
{
    ansi_term_ensure();
    return ansi_forced_plain;
}

void ansi_term_puts(const char *text)
{
    (void)puts(text != NULL ? text : "");
}

void ansi_term_write(const char *text)
{
    if (text != NULL) {
        (void)fputs(text, stdout);
    }
}

void ansi_term_write_n(const char *text, size_t length)
{
    if (text != NULL && length != 0U) {
        (void)fwrite(text, 1U, length, stdout);
    }
}

void ansi_term_vprintf(const char *format, va_list arguments)
{
    if (format != NULL) {
        (void)vprintf(format, arguments);
    }
}

void ansi_term_printf(const char *format, ...)
{
    va_list arguments;

    if (format == NULL) {
        return;
    }

    va_start(arguments, format);
    ansi_term_vprintf(format, arguments);
    va_end(arguments);
}

void ansi_term_stream(const char *text)
{
    ansi_term_write(text);
    (void)fflush(stdout);
}

void ansi_term_flush(void)
{
    (void)fflush(stdout);
}

static const char *ansi_diff_style(int kind)
{
    if (kind == ANSI_DIFF_ADD) {
        return "\033[32m";
    }

    if (kind == ANSI_DIFF_DELETE) {
        return "\033[31m";
    }

    return "\033[2m";
}

void ansi_term_diff_n(int kind, const char *text, size_t length)
{
    if (text == NULL || length == 0U) {
        return;
    }

    ansi_term_ensure();

    if (!ansi_enabled) {
        (void)fwrite(text, 1U, length, stdout);
        return;
    }

    (void)fputs(ansi_diff_style(kind), stdout);
    (void)fwrite(text, 1U, length, stdout);
    (void)fputs("\033[0m", stdout);
}

void ansi_term_diff(int kind, const char *text)
{
    if (text != NULL) {
        ansi_term_diff_n(kind, text, strlen(text));
    }
}

static void ansi_remove_all_versions(const char *path)
{
    if (path == NULL || *path == '\0') {
        return;
    }

    while (remove(path) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static int ansi_git_diff_kind(const char *line)
{
    if (line == NULL) {
        return ANSI_DIFF_CONTEXT;
    }

    if (line[0] == '+' && strncmp(line, "+++", 3U) != 0) {
        return ANSI_DIFF_ADD;
    }

    if (line[0] == '-' && strncmp(line, "---", 3U) != 0) {
        return ANSI_DIFF_DELETE;
    }

    return ANSI_DIFF_CONTEXT;
}

int ansi_term_git_diff(void)
{
    FILE *file;
    char line[2048];
    int status;

    ansi_term_ensure();

    if (!ansi_enabled) {
        return system("git diff --");
    }

    ansi_remove_all_versions(ANSI_GIT_DIFF_FILE);

    status = system(
        "git diff --no-color --output=OVMS_AGENT_ANSI_DIFF.TMP --"
    );

    if ((status & 1) == 0) {
        ansi_remove_all_versions(ANSI_GIT_DIFF_FILE);
        return status;
    }

    file = fopen(ANSI_GIT_DIFF_FILE, "r");
    if (file == NULL) {
        ansi_remove_all_versions(ANSI_GIT_DIFF_FILE);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        ansi_term_diff(ansi_git_diff_kind(line), line);
    }

    if (ferror(file) || fclose(file) != 0) {
        ansi_remove_all_versions(ANSI_GIT_DIFF_FILE);
        return 0;
    }

    ansi_remove_all_versions(ANSI_GIT_DIFF_FILE);
    return status;
}

void ansi_term_status(const char *text)
{
    ansi_term_ensure();

    if (!ansi_enabled) {
        return;
    }

    (void)fputs("\r\033[2K\033[2m", stdout);
    if (text != NULL) {
        (void)fputs(text, stdout);
    }
    (void)fputs("\033[0m", stdout);
    (void)fflush(stdout);
}

void ansi_term_status_turn(unsigned int turn, unsigned int limit)
{
    char text[96];

    if (limit == 0U) {
        return;
    }

    (void)snprintf(
        text,
        sizeof(text),
        "turn %u/%u",
        turn,
        limit
    );
    ansi_term_status(text);
}

void ansi_term_status_clear(void)
{
    ansi_term_ensure();

    if (!ansi_enabled) {
        return;
    }

    (void)fputs("\r\033[2K", stdout);
    (void)fflush(stdout);
}
