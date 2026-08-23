#ifndef ANSI_TERM_H
#define ANSI_TERM_H

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

#define ANSI_DIFF_CONTEXT 0
#define ANSI_DIFF_ADD 1
#define ANSI_DIFF_DELETE 2

void ansi_term_init(void);
int ansi_term_enabled(void);
int ansi_term_plain_forced(void);

void ansi_term_puts(const char *text);
void ansi_term_write(const char *text);
void ansi_term_write_n(const char *text, size_t length);
void ansi_term_vprintf(const char *format, va_list arguments);
void ansi_term_printf(const char *format, ...);
void ansi_term_stream(const char *text);
void ansi_term_flush(void);

void ansi_term_diff(int kind, const char *text);
void ansi_term_diff_n(int kind, const char *text, size_t length);
int ansi_term_git_diff(void);

void ansi_term_status(const char *text);
void ansi_term_status_turn(unsigned int turn, unsigned int limit);
void ansi_term_status_clear(void);

#endif
