#include <stdio.h>
#include <string.h>

#include <descrip.h>
#include <iodef.h>
#include <starlet.h>

#include "secret_input.h"

typedef struct secret_iosb {
    unsigned short status;
    unsigned short count;
    unsigned int information;
} secret_iosb;

int secret_read(const char *prompt,
                char *buffer,
                size_t buffer_size)
{
    static char device_name[] = "SYS$COMMAND";
    struct dsc$descriptor_s device;
    secret_iosb iosb;
    unsigned short channel;
    unsigned long status;
    unsigned long prompt_length;
    unsigned long read_length;

    if (prompt == NULL || buffer == NULL || buffer_size < 2U) {
        return 0;
    }

    if (buffer_size > 65535U) {
        return 0;
    }

    prompt_length = (unsigned long)strlen(prompt);
    if (prompt_length > 65535UL) {
        return 0;
    }

    device.dsc$w_length = (unsigned short)strlen(device_name);
    device.dsc$b_dtype = DSC$K_DTYPE_T;
    device.dsc$b_class = DSC$K_CLASS_S;
    device.dsc$a_pointer = device_name;

    channel = 0;
    status = sys$assign(&device, &channel, 0, 0);
    if ((status & 1UL) == 0UL) {
        return 0;
    }

    (void)memset(buffer, 0, buffer_size);
    (void)memset(&iosb, 0, sizeof(iosb));
    read_length = (unsigned long)(buffer_size - 1U);

    status = sys$qiow(
        0,
        channel,
        IO$_READPROMPT | IO$M_NOECHO,
        &iosb,
        0,
        0,
        buffer,
        read_length,
        0,
        0,
        (void *)prompt,
        prompt_length
    );

    (void)sys$dassgn(channel);
    (void)putchar('\n');

    if ((status & 1UL) == 0UL ||
        ((unsigned long)iosb.status & 1UL) == 0UL) {
        (void)memset(buffer, 0, buffer_size);
        return 0;
    }

    if ((size_t)iosb.count >= buffer_size) {
        (void)memset(buffer, 0, buffer_size);
        return 0;
    }

    buffer[iosb.count] = '\0';
    return buffer[0] != '\0';
}
