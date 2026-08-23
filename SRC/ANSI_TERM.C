#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <dcdef.h>
#include <descrip.h>
#include <devdef.h>
#include <dvidef.h>
#include <starlet.h>

#include "ANSI_TERM.H"

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
    unsigned int device_char;
    unsigned short class_length;
    unsigned short char_length;
    unsigned long status;
    ansi_dvi_item items[3];
    $DESCRIPTOR(output_name, "SYS$OUTPUT");

    device_class = 0U;
    device_char = 0U;
    class_length = 0U;
    char_length = 0U;
    (void)memset(items, 0, sizeof(items));

    items[0].length = (unsigned short)sizeof(device_class);
    items[0].code = DVI$_DEVCLASS;
    items[0].buffer = &device_class;
    items[0].return_length = &class_length;

    items[1].length = (unsigned short)sizeof(device_char);
    items[1].code = DVI$_DEVCHAR;
    items[1].buffer = &device_char;
    items[1].return_length = &char_length;

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

    if (device_class != (unsigned int)DC$_TERM) {
        return 0;
    }

    if ((device_char & (unsigned int)DEV$M_ODV) == 0U) {
        return 0;
    }

    if ((device_char & (unsigned int)DEV$M_DET) != 0U) {
        return 0;
    }

    return 1;
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

void ansi_term_printf(const char *format, ...)
{
    va_list arguments;

    if (format == NULL) {
        return;
    }

    va_start(arguments, format);
    (void)vprintf(format, arguments);
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

void ansi_term_diff(int kind, const char *text)
{
    const char *style;

    if (text == NULL) {
        return;
    }

    ansi_term_ensure();

    if (!ansi_enabled) {
        (void)fputs(text, stdout);
        return;
    }

    style = "\033[2m";
    if (kind == ANSI_DIFF_ADD) {
        style = "\033[32m";
    } else if (kind == ANSI_DIFF_DELETE) {
        style = "\033[31m";
    }

    (void)fputs(style, stdout);
    (void)fputs(text, stdout);
    (void)fputs("\033[0m", stdout);
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

void ansi_term_status_clear(void)
{
    ansi_term_ensure();

    if (!ansi_enabled) {
        return;
    }

    (void)fputs("\r\033[2K", stdout);
    (void)fflush(stdout);
}
