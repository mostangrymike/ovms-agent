#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "M289_BUILD_PROFILE.H"

#define FIX_BADKEY  "M289_BADKEY.TMP"
#define FIX_DUPKEY  "M289_DUPKEY.TMP"
#define FIX_BADPH   "M289_BADPH.TMP"
#define FIX_MISSING "M289_MISSING.TMP"
#define FIX_OVERSIZE "M289_OVERSIZE.TMP"
#define FIX_INTERP_MISSING "M289_INTERP_MISSING.TMP"
#define FIX_INTERP_COMPILE "M289_INTERP_COMPILE.TMP"
#define FIX_INTERP_BADPH "M289_INTERP_BADPH.TMP"

static const char *profile_prefix =
    "language=MACRO32\n"
    "extensions=.MAR\n"
    "kind=compiled\n";

static const char *profile_suffix =
    "compile_options=\n"
    "link_command=LINK {link_options} {object}\n"
    "link_options=\n"
    "object_extension=.OBJ\n"
    "executable_extension=.EXE\n";

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_text(const char *path, const char *middle, const char *extra)
{
    FILE *file;
    int ok;

    remove_all(path);
    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }
    ok = fputs(profile_prefix, file) != EOF &&
         fputs(middle, file) != EOF &&
         fputs(profile_suffix, file) != EOF;
    if (ok && extra != NULL) {
        ok = fputs(extra, file) != EOF;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int write_interpreted(const char *path,
                             const char *run_line,
                             const char *extra)
{
    FILE *file;
    int ok;

    remove_all(path);
    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }
    ok = fputs("language=PYTHON\n", file) != EOF &&
         fputs("extensions=.PY\n", file) != EOF &&
         fputs("kind=interpreted\n", file) != EOF;
    if (ok && run_line != NULL) {
        ok = fputs(run_line, file) != EOF;
    }
    if (ok && extra != NULL) {
        ok = fputs(extra, file) != EOF;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int expect_rejected(const char *path)
{
    m289_build_profile profile;
    char error[M289_PROFILE_ERROR_MAX];

    return !m289_profile_load(path, &profile, error, sizeof(error));
}

static int test_success(void)
{
    m289_build_profile profile;
    char compile_command[M289_PROFILE_CMD_MAX];
    char link_command[M289_PROFILE_CMD_MAX];
    char error[M289_PROFILE_ERROR_MAX];
    int loaded;

    loaded = m289_profile_load("[.LANGUAGE]MACRO32.BUILD",
                               &profile, error, sizeof(error));
    if (!loaded) {
        loaded = m289_profile_load("LANGUAGE/MACRO32.BUILD",
                                   &profile, error, sizeof(error));
    }
    if (!loaded) {
        (void)printf("M289 failed: shipped profile load: %s\n", error);
        return 0;
    }
    if (strcmp(profile.language, "MACRO32") != 0 ||
        strcmp(profile.kind, "compiled") != 0) {
        (void)puts("M289 failed: shipped profile fields.");
        return 0;
    }
    if (!m289_profile_commands(&profile, "WC.MAR",
                               compile_command, sizeof(compile_command),
                               link_command, sizeof(link_command),
                               error, sizeof(error))) {
        (void)printf("M289 failed: command resolution: %s\n", error);
        return 0;
    }
    if (strcmp(compile_command, "MACRO/MIGRATION WC.MAR") != 0 ||
        strcmp(link_command, "LINK WC.OBJ") != 0) {
        (void)printf("M289 failed: resolved compile=[%s] link=[%s]\n",
                     compile_command, link_command);
        return 0;
    }
    return 1;
}

static int test_python_success(void)
{
    m289_build_profile profile;
    char run_command[M289_PROFILE_CMD_MAX];
    char error[M289_PROFILE_ERROR_MAX];
    int loaded;

    loaded = m289_profile_load("[.LANGUAGE]PYTHON.BUILD",
                               &profile, error, sizeof(error));
    if (!loaded) {
        loaded = m289_profile_load("LANGUAGE/PYTHON.BUILD",
                                   &profile, error, sizeof(error));
    }
    if (!loaded) {
        (void)printf("M289 failed: Python profile load: %s\n", error);
        return 0;
    }
    if (strcmp(profile.language, "PYTHON") != 0 ||
        strcmp(profile.kind, "interpreted") != 0) {
        (void)puts("M289 failed: Python profile fields.");
        return 0;
    }
    if (!m289_profile_run_command(&profile, "M289_PYTHON_FIXTURE.PY",
                                  run_command, sizeof(run_command),
                                  error, sizeof(error))) {
        (void)printf("M289 failed: Python command resolution: %s\n", error);
        return 0;
    }
    if (strcmp(run_command, "PYTHON M289_PYTHON_FIXTURE.PY") != 0) {
        (void)printf("M289 failed: resolved run=[%s]\n", run_command);
        return 0;
    }
    return 1;
}

static int test_unknown_key(void)
{
    if (!write_text(FIX_BADKEY,
                    "compile_command=MACRO/MIGRATION {compile_options} {source}\n",
                    "unexpected=value\n")) {
        return 0;
    }
    return expect_rejected(FIX_BADKEY);
}

static int test_duplicate_key(void)
{
    if (!write_text(FIX_DUPKEY,
                    "compile_command=MACRO/MIGRATION {compile_options} {source}\n",
                    "language=MACRO32\n")) {
        return 0;
    }
    return expect_rejected(FIX_DUPKEY);
}

static int test_unknown_placeholder(void)
{
    if (!write_text(FIX_BADPH,
                    "compile_command=MACRO/MIGRATION {unknown} {source}\n",
                    NULL)) {
        return 0;
    }
    return expect_rejected(FIX_BADPH);
}

static int test_missing_key(void)
{
    FILE *file;
    int ok;

    remove_all(FIX_MISSING);
    file = fopen(FIX_MISSING, "w");
    if (file == NULL) {
        return 0;
    }
    ok = fputs(profile_prefix, file) != EOF &&
         fputs("compile_command=MACRO/MIGRATION {compile_options} {source}\n",
               file) != EOF &&
         fputs("compile_options=\n", file) != EOF &&
         fputs("link_options=\n", file) != EOF &&
         fputs("object_extension=.OBJ\n", file) != EOF &&
         fputs("executable_extension=.EXE\n", file) != EOF;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok && expect_rejected(FIX_MISSING);
}

static int test_oversized(void)
{
    FILE *file;
    unsigned int index;
    int ok;

    remove_all(FIX_OVERSIZE);
    file = fopen(FIX_OVERSIZE, "w");
    if (file == NULL) {
        return 0;
    }
    ok = fputs(profile_prefix, file) != EOF &&
         fputs("compile_command=MACRO/MIGRATION {compile_options} {source}\n",
               file) != EOF &&
         fputs("compile_options=", file) != EOF;
    for (index = 0U; ok && index < 1200U; ++index) {
        if (fputc('A', file) == EOF) {
            ok = 0;
        }
    }
    if (ok && fputc('\n', file) == EOF) {
        ok = 0;
    }
    if (ok && fputs(profile_suffix, file) == EOF) {
        ok = 0;
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok && expect_rejected(FIX_OVERSIZE);
}

static int test_interpreted_missing_run(void)
{
    return write_interpreted(FIX_INTERP_MISSING, NULL, NULL) &&
           expect_rejected(FIX_INTERP_MISSING);
}

static int test_interpreted_rejects_compile(void)
{
    return write_interpreted(
               FIX_INTERP_COMPILE,
               "run_command=PYTHON {source}\n",
               "compile_command=PYTHON -m py_compile {source}\n") &&
           expect_rejected(FIX_INTERP_COMPILE);
}

static int test_interpreted_bad_placeholder(void)
{
    return write_interpreted(FIX_INTERP_BADPH,
                             "run_command=PYTHON {object}\n", NULL) &&
           expect_rejected(FIX_INTERP_BADPH);
}

int main(void)
{
    int ok;

    ok = test_success() &&
         test_python_success() &&
         test_unknown_key() &&
         test_duplicate_key() &&
         test_unknown_placeholder() &&
         test_missing_key() &&
         test_oversized() &&
         test_interpreted_missing_run() &&
         test_interpreted_rejects_compile() &&
         test_interpreted_bad_placeholder();

    remove_all(FIX_BADKEY);
    remove_all(FIX_DUPKEY);
    remove_all(FIX_BADPH);
    remove_all(FIX_MISSING);
    remove_all(FIX_OVERSIZE);
    remove_all(FIX_INTERP_MISSING);
    remove_all(FIX_INTERP_COMPILE);
    remove_all(FIX_INTERP_BADPH);

    if (!ok) {
        (void)puts("M289 build-profile parser/resolver test failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M289 build-profile parser/resolver test passed.");
    return EXIT_SUCCESS;
}
