#include <stdarg.h>

#include "ANSI_TERM.H"

static int m273_plan_puts(const char *text)
{
    ansi_term_puts(text);
    return 0;
}

static int m273_plan_printf(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    ansi_term_vprintf(format, arguments);
    va_end(arguments);
    return 0;
}

static int m273_plan_fputs(const char *text, FILE *stream)
{
    if (stream == stdout) {
        ansi_term_write(text);
        return 0;
    }

    return fputs(text, stream);
}

static int m273_plan_putchar(int character)
{
    char text[1];

    text[0] = (char)character;
    ansi_term_write_n(text, 1U);
    return character;
}

static int m273_plan_fflush(FILE *stream)
{
    if (stream == stdout) {
        ansi_term_flush();
        return 0;
    }

    return fflush(stream);
}

#define puts m273_plan_puts
#define printf m273_plan_printf
#define fputs m273_plan_fputs
#define putchar m273_plan_putchar
#define fflush m273_plan_fflush
#include "LLM_PLAN.C"
#undef fflush
#undef putchar
#undef fputs
#undef printf
#undef puts
