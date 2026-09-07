#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PROJECT.H"

#define M305_FIXTURE "M305_PATCH_FIXTURE.TXT"
#define M305_INPUT   "M305_PATCH_INPUT.TXT"

static void m305_remove_versions(const char *path)
{
    while (remove(path) == 0) {
        /* Remove every OpenVMS version, newest first. */
    }
}

static int m305_seed_fixture(void)
{
    FILE *file;

    m305_remove_versions(M305_FIXTURE);
    file = fopen(M305_FIXTURE, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("ORIGINAL\n", file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return 1;
}

static int m305_seed_input(void)
{
    FILE *file;

    m305_remove_versions(M305_INPUT);
    file = fopen(M305_INPUT, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("y\ny\n", file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return freopen(M305_INPUT, "r", stdin) != NULL;
}

static int m305_one_record(const char *expected)
{
    FILE *file;
    char line[128];
    char extra[8];
    int ok;

    file = fopen(M305_FIXTURE, "r");
    if (file == NULL) {
        return 0;
    }

    ok = fgets(line, sizeof(line), file) != NULL &&
         strcmp(line, expected) == 0 &&
         fgets(extra, sizeof(extra), file) == NULL &&
         !ferror(file);

    if (fclose(file) != 0) {
        ok = 0;
    }

    return ok;
}

int main(void)
{
    agent_state state;
    int ok;

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    ok = m305_seed_fixture() && m305_seed_input();

    if (ok) {
        ok = project_patch(&state,
                           M305_FIXTURE,
                           "ORIGINAL",
                           "CHANGED");
    }

    if (ok) {
        ok = m305_one_record("CHANGED\n");
    }

    if (ok) {
        ok = project_patch(&state,
                           M305_FIXTURE,
                           "CHANGED",
                           "ORIGINAL");
    }

    if (ok) {
        ok = m305_one_record("ORIGINAL\n");
    }

    m305_remove_versions(M305_FIXTURE);
    m305_remove_versions(M305_INPUT);

    if (!ok) {
        (void)puts("M305 direct PATCH RMS regression failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M305 direct PATCH RMS regression passed.");
    return EXIT_SUCCESS;
}
