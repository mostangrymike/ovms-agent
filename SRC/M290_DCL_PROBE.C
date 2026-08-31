#include <stdio.h>
#include <stdlib.h>

#define PROBE_COM "SYS$SCRATCH:M290_DCL_PROBE.COM"
#define PROBE_OUT "SYS$SCRATCH:M290_DCL_PROBE.OUT"
#define PROBE_STS "SYS$SCRATCH:M290_DCL_PROBE.STS"

static void remove_versions(const char *path)
{
    while (remove(path) == 0) {
    }
}

static void show_file(const char *label, const char *path)
{
    FILE *file;
    char line[512];

    (void)printf("%s (%s):\n", label, path);
    file = fopen(path, "r");
    if (file == NULL) {
        (void)puts("  [not readable]");
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)printf("  %s", line);
    }
    (void)fclose(file);
}

int main(void)
{
    FILE *procedure;
    FILE *status_file;
    unsigned long raw_status;
    int system_status;
    int parsed;

    remove_versions(PROBE_COM);
    remove_versions(PROBE_OUT);
    remove_versions(PROBE_STS);

    procedure = fopen(PROBE_COM, "w");
    if (procedure == NULL) {
        (void)puts("M290 DCL probe failed: cannot create procedure.");
        return EXIT_FAILURE;
    }

    (void)fprintf(
        procedure,
        "$ DEFINE SYS$OUTPUT %s\n"
        "$ DEFINE SYS$ERROR SYS$OUTPUT\n"
        "$ SHOW DEFAULT\n"
        "$ OVMS_DCL_STATUS = $STATUS\n"
        "$ OPEN/WRITE OVMS_DCL_RAW %s\n"
        "$ WRITE OVMS_DCL_RAW F$FAO(\"!8XL\",OVMS_DCL_STATUS)\n"
        "$ CLOSE OVMS_DCL_RAW\n"
        "$ OVMS_DCL_MSG = F$EDIT(F$MESSAGE(OVMS_DCL_STATUS),\"UPCASE\")\n"
        "$ OVMS_DCL_NORM = OVMS_DCL_STATUS\n"
        "$ IF F$LOCATE(\"%%C-S-EXIT\",OVMS_DCL_MSG) .LT. F$LENGTH(OVMS_DCL_MSG) THEN OVMS_DCL_NORM = %%X00000002\n"
        "$ EXIT 'OVMS_DCL_NORM'\n",
        PROBE_OUT,
        PROBE_STS);

    if (fclose(procedure) != 0) {
        (void)puts("M290 DCL probe failed: cannot close procedure.");
        return EXIT_FAILURE;
    }

    system_status = system("@SYS$SCRATCH:M290_DCL_PROBE.COM");
    (void)printf("system() status: %%X%08X\n", (unsigned int)system_status);

    raw_status = 0UL;
    parsed = 0;
    status_file = fopen(PROBE_STS, "r");
    if (status_file != NULL) {
        if (fscanf(status_file, "%lx", &raw_status) == 1) {
            parsed = 1;
        }
        (void)fclose(status_file);
    }

    (void)printf("sidecar parsed: %s\n", parsed ? "yes" : "no");
    if (parsed) {
        (void)printf("sidecar status: %%X%08lX\n", raw_status);
    }

    show_file("Captured output", PROBE_OUT);
    show_file("Raw sidecar", PROBE_STS);

    return EXIT_SUCCESS;
}
