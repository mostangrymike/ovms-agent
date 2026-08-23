#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ANSI_TERM.H"

static int verify_no_escape(const char *path)
{
    FILE *file;
    int ch;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    while ((ch = fgetc(file)) != EOF) {
        if (ch == 27) {
            (void)fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static void emit_fixture(void)
{
    ansi_term_diff(ANSI_DIFF_CONTEXT, " context line\n");
    ansi_term_diff(ANSI_DIFF_DELETE, "-deleted line\n");
    ansi_term_diff(ANSI_DIFF_ADD, "+added line\n");
    ansi_term_stream("assistant response text\n");
    ansi_term_status("turn 1/4 | tokens 10");
    ansi_term_status_clear();
    ansi_term_puts("plain completion line");
    ansi_term_flush();
}

int main(int argc, char **argv)
{
    int enabled;

    if (argc == 3 && strcmp(argv[1], "VERIFY") == 0) {
        if (!verify_no_escape(argv[2])) {
            (void)fprintf(stderr,
                "M273 ANSI regression failed: ESC byte found or capture unreadable.\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    ansi_term_init();
    enabled = ansi_term_enabled();

    if (argc == 2 && strcmp(argv[1], "PROBE") == 0) {
        (void)printf(
            "ANSI terminal probe: enabled=%d forced_plain=%d\n",
            enabled,
            ansi_term_plain_forced()
        );
        return EXIT_SUCCESS;
    }

    emit_fixture();

    if (argc == 2 && strcmp(argv[1], "OVERRIDE") == 0) {
        if (!ansi_term_plain_forced() || enabled) {
            (void)fprintf(stderr,
                "M273 ANSI regression failed: plain override was not authoritative.\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "REDIRECT") == 0) {
        if (enabled) {
            (void)fprintf(stderr,
                "M273 ANSI regression failed: redirected SYS$OUTPUT was treated as a terminal.\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    (void)fprintf(stderr,
        "Usage: M273_ANSI_TERM_TEST PROBE | REDIRECT | OVERRIDE | VERIFY capture-file\n");
    return EXIT_FAILURE;
}
