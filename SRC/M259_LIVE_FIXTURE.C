#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char png[] = {
        0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU,
        0x00U, 0x00U, 0x00U, 0x0dU, 0x49U, 0x48U, 0x44U, 0x52U,
        0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x02U,
        0x08U, 0x02U, 0x00U, 0x00U, 0x00U, 0xfdU, 0xd4U, 0x9aU,
        0x73U, 0x00U, 0x00U, 0x00U, 0x10U, 0x49U, 0x44U, 0x41U,
        0x54U, 0x78U, 0x9cU, 0x63U, 0xf8U, 0xcfU, 0xc0U, 0x00U,
        0x44U, 0x0cU, 0x10U, 0x0aU, 0x00U, 0x1fU, 0xeeU, 0x03U,
        0xfdU, 0x8bU, 0x5fU, 0x14U, 0xd4U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x49U, 0x45U, 0x4eU, 0x44U, 0xaeU, 0x42U, 0x60U,
        0x82U
    };
    unsigned char verify[sizeof(png)];
    FILE *file;
    size_t count;

    file = fopen("M259_LIVE.PNG", "wb");
    if (file == NULL) {
        (void)puts("Unable to create M259_LIVE.PNG.");
        return 2;
    }

    if (fwrite(png, 1U, sizeof(png), file) != sizeof(png) ||
        fclose(file) != 0) {
        (void)puts("Unable to write complete M259_LIVE.PNG.");
        return 2;
    }

    file = fopen("M259_LIVE.PNG", "rb");
    if (file == NULL) {
        (void)puts("Unable to reopen M259_LIVE.PNG.");
        return 2;
    }
    count = fread(verify, 1U, sizeof(verify), file);
    if (count != sizeof(png) ||
        fgetc(file) != EOF ||
        fclose(file) != 0 ||
        memcmp(verify, png, sizeof(png)) != 0) {
        (void)puts("M259_LIVE.PNG byte verification failed.");
        return 2;
    }

    (void)printf("Created and verified M259_LIVE.PNG (%lu bytes, 2x2 solid red).\n",
                 (unsigned long)sizeof(png));
    return 1;
}
