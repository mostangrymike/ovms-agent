#ifndef _VMS_WAIT
#define _VMS_WAIT
#endif

#include <stdio.h>
#include <stdlib.h>

#include "OVMS_STATUS.H"

#define M314_OK_CHILD "@[.BUILD]M314_PCLOSE_OK.COM"
#define M314_FAIL_CHILD "@[.BUILD]M314_PCLOSE_FAIL.COM"

static int run_child(const char *command, int expect_success)
{
    FILE *stream;
    char line[128];
    int status;
    int success;

    stream = popen(command, "r");
    if (stream == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        /* Drain child output before pclose(). */
    }

    status = pclose(stream);
    if (status == -1) {
        return 0;
    }

    success = ovms_status_success((unsigned long)(unsigned int)status);
    if (success != expect_success) {
        (void)printf("M314 unexpected child status: %%X%08lX\n",
                     (unsigned long)(unsigned int)status);
        return 0;
    }

    return 1;
}

int main(void)
{
    if (!run_child(M314_OK_CHILD, 1)) {
        (void)puts("M314 failed: successful child was not successful.");
        return EXIT_FAILURE;
    }

    if (!run_child(M314_FAIL_CHILD, 0)) {
        (void)puts("M314 failed: failing child was not failure.");
        return EXIT_FAILURE;
    }

    (void)puts("M314 native pclose completion regression passed.");
    return EXIT_SUCCESS;
}
