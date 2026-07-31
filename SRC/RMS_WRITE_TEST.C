#include <stdio.h>
#include <stdlib.h>

#include "rms_write.h"

int main(int argc, char **argv)
{
    const char *content;

    if (argc != 2) {
        (void)puts(
            "Usage: RMS_WRITE_TEST output.opt"
        );
        return EXIT_FAILURE;
    }

    content =
        "[.BUILD]COMMAND.OBJ\n"
        "[.BUILD]COMMAND_CORE.OBJ\n"
        "[.BUILD]COMMAND_SYMBOL.OBJ\n";

    if (!rms_replace_text_file(
            argv[1],
            content)) {
        (void)puts("RMS write test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("RMS write test passed.");
    return EXIT_SUCCESS;
}
