#ifndef OVMS_STATUS_H
#define OVMS_STATUS_H

#define OVMS_POSIX_EXIT_BASE 0x0035A000UL
#define OVMS_POSIX_EXIT_MASK 0x0FFFF800UL

static int ovms_status_posix(unsigned long status, unsigned int *code_out)
{
    if ((status & OVMS_POSIX_EXIT_MASK) != OVMS_POSIX_EXIT_BASE) {
        return 0;
    }

    if (code_out != 0) {
        *code_out = (unsigned int)((status >> 3) & 255UL);
    }
    return 1;
}

static int ovms_status_success(unsigned long status)
{
    unsigned int code;

    if (ovms_status_posix(status, &code)) {
        return code == 0U;
    }
    return (status & 1UL) != 0UL;
}

static unsigned long ovms_status_normalize(unsigned long status)
{
    return status;
}

static unsigned long ovms_status_propagate(unsigned long status)
{
    unsigned int code;

    if (ovms_status_posix(status, &code)) {
        return code == 0U ? 1UL : 2UL;
    }
    return status;
}

#endif
