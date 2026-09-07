#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LLM_PLAN_PATH_SIZE 256U
#define LLM_PLAN_FILE "M308_PLAN.TXT"
#define LLM_EXECUTE_LINE_SIZE 4096U

void llm_plan_approval_clear(void)
{
}

void llm_plan_recovery_sync(const char *plan_path)
{
    (void)plan_path;
}

int llm_plan_save_legacy(const char *goal,
                         const char *plan_text)
{
    (void)goal;
    (void)plan_text;
    return 0;
}

int llm_plan_file_current_legacy(const char *plan_path,
                                 int verbose)
{
    (void)plan_path;
    (void)verbose;
    return 0;
}

#include "LLM_EXECUTE_READ_TEXT_BLOCK.INC"
#include "LLM_PLAN_M138_INTEGRITY.INC"

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
                      "M308 saved-plan regression failed: %s\n",
                      message);
        return 0;
    }
    return 1;
}

static char *make_operations(unsigned int count)
{
    char *text;
    size_t capacity;
    size_t used;
    unsigned int index;

    capacity = 8192U;
    text = (char *)malloc(capacity);
    if (text == NULL) {
        return NULL;
    }

    used = 0U;
    text[0] = '\0';

    used += (size_t)snprintf(
        text + used,
        capacity - used,
        "operation_count=%u\n",
        count);

    for (index = 0U; index < count; ++index) {
        int written;

        written = snprintf(
            text + used,
            capacity - used,
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=M308_%u.DAT\n"
            "BEGIN_NEW_TEXT\n"
            "X\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n",
            index);

        if (written <= 0 ||
            (size_t)written >= capacity - used) {
            free(text);
            return NULL;
        }

        used += (size_t)written;
    }

    return text;
}

static char *make_payload(size_t length)
{
    const char *prefix =
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M308_PAYLOAD.DAT\n"
        "BEGIN_NEW_TEXT\n";
    const char *suffix =
        "\nEND_NEW_TEXT\n"
        "END_OPERATION\n";
    size_t total;
    char *text;

    total = strlen(prefix) + length + strlen(suffix) + 1U;
    text = (char *)malloc(total);
    if (text == NULL) {
        return NULL;
    }

    (void)strcpy(text, prefix);
    (void)memset(text + strlen(prefix), 'A', length);
    (void)strcpy(text + strlen(prefix) + length, suffix);
    return text;
}

static int decode_escape_test(void)
{
    FILE *file;
    char decoded[512];
    int result;

    file = fopen("M308_ESCAPE.TMP", "w");
    if (file == NULL) {
        return 0;
    }

    result =
        fputs("\\BEGIN_OPERATION\n"
              "\\operation_count=999\n"
              "\\\\END_OPERATION\n"
              "END_NEW_TEXT\n",
              file) != EOF &&
        fclose(file) == 0;

    if (!result) {
        (void)remove("M308_ESCAPE.TMP");
        return 0;
    }

    file = fopen("M308_ESCAPE.TMP", "r");
    if (file == NULL) {
        (void)remove("M308_ESCAPE.TMP");
        return 0;
    }

    result = execute_read_text_block(
        file,
        "END_NEW_TEXT",
        decoded,
        sizeof(decoded),
        1);

    if (fclose(file) != 0) {
        result = 0;
    }
    (void)remove("M308_ESCAPE.TMP");

    return result &&
        strcmp(decoded,
               "BEGIN_OPERATION\n"
               "operation_count=999\n"
               "\\END_OPERATION") == 0;
}

int main(void)
{
    char *plan32;
    char *plan33;
    char *payload_ok;
    char *payload_bad;
    char *normalized;
    const char *escaped_plan;
    unsigned int count;

    plan32 = make_operations(32U);
    plan33 = make_operations(33U);
    payload_ok = make_payload(4094U);
    payload_bad = make_payload(4095U);

    if (!require_true(
            plan32 != NULL && plan33 != NULL &&
            payload_ok != NULL && payload_bad != NULL,
            "fixture allocation")) {
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }

    count = 0U;
    if (!require_true(
            llm_m308_count_blocks(plan32, &count) && count == 32U,
            "32-operation count") ||
        !require_true(
            llm_m308_count_blocks(plan33, &count) && count == 33U,
            "33-operation count")) {
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }

    normalized = llm_m195_normalize_count(plan32);
    if (!require_true(normalized != NULL,
                      "32-operation plan accepted")) {
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }
    free(normalized);

    normalized = llm_m195_normalize_count(plan33);
    if (!require_true(normalized == NULL,
                      "33-operation plan rejected as over capacity")) {
        free(normalized);
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }

    escaped_plan =
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M308_ESCAPED.DAT\n"
        "BEGIN_NEW_TEXT\n"
        "\\operation_count=999\n"
        "\\BEGIN_OPERATION\n"
        "\\END_OPERATION\n"
        "\\BEGIN_OLD_TEXT\n"
        "\\END_OLD_TEXT\n"
        "\\BEGIN_NEW_TEXT\n"
        "\\END_NEW_TEXT\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n";

    normalized = llm_m195_normalize_count(escaped_plan);
    if (!require_true(
            normalized != NULL &&
            strcmp(normalized, escaped_plan) == 0,
            "escaped structured-control payload accepted")) {
        free(normalized);
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }
    free(normalized);

    normalized = llm_m195_normalize_count(payload_ok);
    if (!require_true(normalized != NULL,
                      "4094-byte payload accepted")) {
        free(normalized);
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }
    free(normalized);

    normalized = llm_m195_normalize_count(payload_bad);
    if (!require_true(normalized == NULL,
                      "4095-byte payload rejected")) {
        free(normalized);
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }

    if (!require_true(decode_escape_test(),
                      "escaped payload decoder round trip")) {
        free(plan32);
        free(plan33);
        free(payload_ok);
        free(payload_bad);
        return EXIT_FAILURE;
    }

    free(plan32);
    free(plan33);
    free(payload_ok);
    free(payload_bad);

    (void)puts("M308 saved-plan framing/capacity regression passed.");
    return EXIT_SUCCESS;
}
