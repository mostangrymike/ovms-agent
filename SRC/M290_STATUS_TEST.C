#include <stdio.h>
#include <stdlib.h>

#include "OVMS_STATUS.H"

static int fail(const char *message)
{
    (void)printf("M290 status regression failed: %s\n", message);
    return EXIT_FAILURE;
}

int main(void)
{
    unsigned int code;

    code = 0U;
    if (!ovms_status_posix(0x0035A401UL, &code) || code != 128U) {
        return fail("Git exit 128 decode");
    }
    if (ovms_status_success(0x0035A401UL) ||
        ovms_status_normalize(0x0035A401UL) != 0x0035A401UL ||
        ovms_status_propagate(0x0035A401UL) != 2UL) {
        return fail("Git exit 128 reporting/propagation");
    }

    code = 0U;
    if (!ovms_status_posix(0x1035A00AUL, &code) || code != 1U) {
        return fail("POSIX exit 1 decode");
    }
    if (ovms_status_success(0x1035A00AUL) ||
        ovms_status_normalize(0x1035A00AUL) != 0x1035A00AUL ||
        ovms_status_propagate(0x1035A00AUL) != 2UL) {
        return fail("POSIX exit 1 reporting/propagation");
    }

    if (ovms_status_posix(0x00030001UL, &code)) {
        return fail("native status misdetected as POSIX");
    }
    if (!ovms_status_success(0x00000001UL) ||
        !ovms_status_success(0x00030001UL) ||
        ovms_status_success(0x00000002UL)) {
        return fail("native odd/even classification");
    }
    if (ovms_status_normalize(0x00030001UL) != 0x00030001UL ||
        ovms_status_propagate(0x00030001UL) != 0x00030001UL) {
        return fail("native status preservation");
    }

    (void)puts("M290 POSIX status regression passed.");
    return EXIT_SUCCESS;
}
