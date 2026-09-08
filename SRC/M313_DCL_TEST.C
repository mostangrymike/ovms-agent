#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_KNOWLEDGE.C"

#define M313_DCL_PACK "[.KNOWLEDGE]OPENVMS_DCL.MD"

static char *read_pack(void)
{
    FILE *file;
    long length;
    char *text;
    size_t count;

    file = fopen(M313_DCL_PACK, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    count = fread(text, 1U, (size_t)length, file);
    (void)fclose(file);
    if (count != (size_t)length) {
        free(text);
        return NULL;
    }

    text[count] = '\0';
    return text;
}

int main(void)
{
    char *pack;

    if ((llm_knowledge_detect("Repair TEST.COM") &
         LLM_KNOWLEDGE_DCL) == 0U ||
        (llm_knowledge_detect("Create build.com with status handling") &
         LLM_KNOWLEDGE_DCL) == 0U) {
        (void)puts("M313 failed: .COM prompt did not select DCL knowledge.");
        return EXIT_FAILURE;
    }

    pack = read_pack();
    if (pack == NULL) {
        (void)puts("M313 failed: unable to read OpenVMS DCL knowledge pack.");
        return EXIT_FAILURE;
    }

    if (strstr(pack, "SET NOON") == NULL ||
        strstr(pack, "capture `$STATUS` immediately") == NULL ||
        strstr(pack, "does not close the DCL channel") == NULL ||
        strstr(pack, "close each open channel exactly once") == NULL ||
        strstr(pack, "DEASSIGN/USER SYS$OUTPUT") == NULL ||
        strstr(pack, "NOLOGNAM") == NULL ||
        strstr(pack, "trace the actual sequence") == NULL) {
        free(pack);
        (void)puts("M313 failed: required DCL lifecycle guidance is incomplete.");
        return EXIT_FAILURE;
    }

    free(pack);
    (void)puts("M313 DCL knowledge regression passed.");
    return EXIT_SUCCESS;
}
