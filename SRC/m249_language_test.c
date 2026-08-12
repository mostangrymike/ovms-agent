#include <stdio.h>
#include <string.h>
#include "openai_internal.h"
#include "command_internal.h"

int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 1;
}

int command_read_stream(FILE *stream, char *output, size_t output_size)
{
    (void)stream; (void)output; (void)output_size;
    return 0;
}

static int has(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

int main(void)
{
    char output[8192];
    const command_entry *entry;

    if (!openai_lang_list_text(output, sizeof(output)) ||
        !has(output, "MACRO-32") || !has(output, "FORTRAN") ||
        !has(output, "BASIC") || !has(output, "PASCAL") ||
        !has(output, "COBOL") || !has(output, "BLISS")) {
        (void)puts("M249 failed: language catalog.");
        return 2;
    }

    if (!openai_lang_detect_text("DISK:[SRC]LOGIN.COM;7", output, sizeof(output)) ||
        !has(output, "Language: DCL")) {
        (void)puts("M249 failed: DCL detection.");
        return 2;
    }
    if (!openai_lang_detect_text("SRC/DRIVER.MAR", output, sizeof(output)) ||
        !has(output, "Language: MACRO-32")) {
        (void)puts("M249 failed: MACRO detection.");
        return 2;
    }
    if (!openai_lang_detect_text("MODEL.FOR;1", output, sizeof(output)) ||
        !has(output, "Language: FORTRAN")) {
        (void)puts("M249 failed: Fortran detection.");
        return 2;
    }
    if (!openai_lang_detect_text("REPORT.BAS", output, sizeof(output)) ||
        !has(output, "Language: BASIC")) {
        (void)puts("M249 failed: BASIC detection.");
        return 2;
    }
    if (!openai_lang_detect_text("MODULE.PAS", output, sizeof(output)) ||
        !has(output, "Language: PASCAL")) {
        (void)puts("M249 failed: Pascal detection.");
        return 2;
    }
    if (!openai_lang_detect_text("PAYROLL.COB", output, sizeof(output)) ||
        !has(output, "Language: COBOL")) {
        (void)puts("M249 failed: COBOL detection.");
        return 2;
    }
    if (!openai_lang_detect_text("KERNEL.BLI", output, sizeof(output)) ||
        !has(output, "Language: BLISS")) {
        (void)puts("M249 failed: BLISS detection.");
        return 2;
    }
    if (!openai_lang_detect_text("THING.C", output, sizeof(output)) ||
        !has(output, "Language: C")) {
        (void)puts("M249 failed: C detection.");
        return 2;
    }

    if (!openai_lang_info_text("fortran", output, sizeof(output)) ||
        !has(output, "fixed-form") || !has(output, "FORTRAN source.FOR")) {
        (void)puts("M249 failed: language info.");
        return 2;
    }

    if (!openai_lang_policy_text(output, sizeof(output)) ||
        !has(output, "Do not assume C syntax") ||
        !has(output, "31 characters") ||
        !has(output, "COBOL/C interoperability") ||
        !has(output, "CALL ... GIVING PIC S9(9) COMP") ||
        !has(output, "BY VALUE PIC S9(9) COMP") ||
        !has(output, "BY REFERENCE PIC S9(9) COMP") ||
        !has(output, "RETURNING for GIVING") ||
        !has(output, "COBOL-to-COBOL") ||
                  !has(output, "source inspection alone") ||
          !has(output, "compiler documentation") ||
          !has(output, "external-name mapping") ||
          !has(output, "trailing underscores") ||
          !has(output, "linker-visible OpenVMS external symbol") ||
!has(output, "existing BUILD.COM") ||
        !has(output, "MMS/MMK") ||
        !has(output, "DESCRIP.MMS is authoritative") ||
        !has(output, "MMS suffix rules") ||
        !has(output, "project-demonstrated syntax") ||
        !has(output, "OpenVMS networking") ||
        !has(output, "socket descriptor") ||
        !has(output, "OpenVMS I/O channel") ||
        !has(output, "interchangeable") ||
        !has(output, "socket or channel handoff") ||
        !has(output, "working interface")) {
        (void)puts("M249 failed: agent language policy.");
        return 2;
    }

    entry = command_find("AGENT/LANG");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M249 failed: AGENT/LANG registration.");
        return 2;
    }
    entry = command_find("AGENT/LANG/DETECT");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M249 failed: AGENT/LANG/DETECT registration.");
        return 2;
    }
    entry = command_find("AGENT/LANG/INFO");
    if (entry == NULL || entry->handler == NULL) {
        (void)puts("M249 failed: AGENT/LANG/INFO registration.");
        return 2;
    }

    (void)puts("M249 multilingual language regression test passed.");
    return 1;
}
