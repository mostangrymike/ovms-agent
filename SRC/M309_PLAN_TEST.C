#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LLM_PLAN_PATH_SIZE 256U
#define LLM_PLAN_FILE "M309_PLAN.TXT"
#define LLM_EXECUTE_LINE_SIZE 4096U
#define LLM_RESPONSE_FILE "M309_RESPONSE.JSON"
#define LLM_WORKFLOW_PLAN 309

int llm_last_workflow = LLM_WORKFLOW_PLAN;
static int save_calls = 0;
static const char *response_text_fixture = NULL;

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

char *read_entire_file(const char *path, size_t *size_out)
{
    FILE *file;
    long length;
    char *text;
    size_t count;

    if (size_out != NULL) {
        *size_out = 0U;
    }

    file = fopen(path, "r");
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
    if (fclose(file) != 0 || count != (size_t)length) {
        free(text);
        return NULL;
    }

    text[count] = '\0';
    if (size_out != NULL) {
        *size_out = count;
    }
    return text;
}

char *extract_output_text_from_json(const char *json)
{
    char *copy;
    size_t length;

    (void)json;
    if (response_text_fixture == NULL) {
        return NULL;
    }

    length = strlen(response_text_fixture);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    (void)memcpy(copy, response_text_fixture, length + 1U);
    return copy;
}

int llm_response_token_exhausted(const char *json,
                                 long *output_tokens,
                                 long *reasoning_tokens)
{
    int exhausted;

    if (output_tokens != NULL) {
        *output_tokens = 0L;
    }
    if (reasoning_tokens != NULL) {
        *reasoning_tokens = 0L;
    }

    exhausted = json != NULL &&
        strstr(json, "\"status\":\"incomplete\"") != NULL &&
        strstr(json, "\"reason\":\"max_output_tokens\"") != NULL;

    if (exhausted) {
        if (output_tokens != NULL) {
            *output_tokens = 4096L;
        }
        if (reasoning_tokens != NULL) {
            *reasoning_tokens = 128L;
        }
    }
    return exhausted;
}

#include "LLM_EXECUTE_READ_TEXT_BLOCK.INC"
#define llm_plan_save llm_m309_integrity_save
#include "LLM_PLAN_M138_INTEGRITY.INC"
#undef llm_plan_save

int llm_plan_save(const char *goal, const char *plan_text)
{
    (void)goal;
    (void)plan_text;
    ++save_calls;
    return 1;
}

#include "LLM_PLAN_M309.H"

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
                      "M309 saved-plan regression failed: %s\n",
                      message);
        return 0;
    }
    return 1;
}

static int write_response(const char *text)
{
    FILE *file;
    int result;

    file = fopen(LLM_RESPONSE_FILE, "w");
    if (file == NULL) {
        return 0;
    }
    result = fputs(text, file) != EOF && fclose(file) == 0;
    if (!result) {
        (void)remove(LLM_RESPONSE_FILE);
    }
    return result;
}

static const char *complete_prefix(void)
{
    return
        "Goal\n"
        "Files to inspect\n"
        "Files to modify\n"
        "Files to create\n"
        "Ordered edits\n"
        "Validation\n"
        "Risks\n"
        "Authority required\n";
}

static int markdown_round_trip(void)
{
    FILE *file;
    char decoded[512];
    int result;

    file = fopen("M309_FENCE.TMP", "w");
    if (file == NULL) {
        return 0;
    }

    result =
        fputs("\\\\```text\n"
              "inside\n"
              "\\\\  ```\n"
              "\\\\\\```raw\n"
              "END_NEW_TEXT\n",
              file) != EOF &&
        fclose(file) == 0;
    if (!result) {
        (void)remove("M309_FENCE.TMP");
        return 0;
    }

    file = fopen("M309_FENCE.TMP", "r");
    if (file == NULL) {
        (void)remove("M309_FENCE.TMP");
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
    (void)remove("M309_FENCE.TMP");

    return result &&
        strcmp(decoded,
               "```text\n"
               "inside\n"
               "  ```\n"
               "\\```raw") == 0;
}

int main(void)
{
    char structured[2048];
    char advisory[2048];
    char missing[2048];
    char *normalized;
    const char *fenced_plan;
    const char *progress;
    int before;

    progress = "I'll inspect the files first.\n";

    (void)snprintf(
        structured,
        sizeof(structured),
        "%soperation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M309.DAT\n"
        "BEGIN_NEW_TEXT\n"
        "X\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n",
        complete_prefix());

    (void)snprintf(
        advisory,
        sizeof(advisory),
        "%sPlan mode: advisory-only\n",
        complete_prefix());

    (void)snprintf(
        missing,
        sizeof(missing),
        "Goal\nFiles to inspect\nFiles to modify\nFiles to create\n"
        "Ordered edits\nValidation\nRisks\n"
        "operation_count=1\nBEGIN_OPERATION\nEND_OPERATION\n");

    if (!require_true(llm_m309_plan_complete(structured),
                      "complete structured plan recognized") ||
        !require_true(llm_m309_plan_complete(advisory),
                      "complete advisory plan recognized") ||
        !require_true(!llm_m309_plan_complete(progress),
                      "progress-only text rejected") ||
        !require_true(!llm_m309_plan_complete(missing),
                      "missing mandatory heading rejected")) {
        return EXIT_FAILURE;
    }

    fenced_plan =
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=create_file\n"
        "path=M309_MD.DAT\n"
        "BEGIN_NEW_TEXT\n"
        "\\\\```text\n"
        "hello\n"
        "\\\\```\n"
        "END_NEW_TEXT\n"
        "END_OPERATION\n";

    normalized = llm_m195_normalize_count(fenced_plan);
    if (!require_true(
            normalized != NULL && strcmp(normalized, fenced_plan) == 0,
            "two-slash framed Markdown accepted by serializer")) {
        free(normalized);
        return EXIT_FAILURE;
    }
    free(normalized);

    if (!require_true(markdown_round_trip(),
                      "framed Markdown decoder round trip")) {
        return EXIT_FAILURE;
    }

    response_text_fixture = structured;
    if (!require_true(write_response(
            "{\"status\":\"incomplete\","
            "\"incomplete_details\":{\"reason\":\"max_output_tokens\"}}"),
            "write incomplete response fixture")) {
        return EXIT_FAILURE;
    }

    before = save_calls;
    if (!require_true(!llm_m309_plan_save("goal", structured),
                      "incomplete response rejected before save") ||
        !require_true(save_calls == before,
                      "incomplete response never reached underlying save")) {
        (void)remove(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    if (!require_true(write_response("{\"status\":\"completed\"}"),
                      "write completed response fixture")) {
        return EXIT_FAILURE;
    }

    response_text_fixture = structured;
    before = save_calls;
    if (!require_true(llm_m309_plan_save("goal", structured),
                      "complete structured response saved") ||
        !require_true(save_calls == before + 1,
                      "complete structured response reached underlying save")) {
        (void)remove(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    response_text_fixture = progress;
    before = save_calls;
    if (!require_true(!llm_m309_plan_save("goal", progress),
                      "progress-only response rejected before save") ||
        !require_true(save_calls == before,
                      "progress-only response never reached underlying save")) {
        (void)remove(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    response_text_fixture = advisory;
    before = save_calls;
    if (!require_true(llm_m309_plan_save("goal", advisory),
                      "explicit advisory plan saved") ||
        !require_true(save_calls == before + 1,
                      "advisory response reached underlying save")) {
        (void)remove(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    response_text_fixture = NULL;
    before = save_calls;
    if (!require_true(llm_m309_plan_save("internal", "INVALID"),
                      "unrelated internal save bypasses live-response checks") ||
        !require_true(save_calls == before + 1,
                      "bypass reached underlying save")) {
        (void)remove(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    (void)remove(LLM_RESPONSE_FILE);
    (void)puts("M309 saved-plan completion/Markdown regression passed.");
    return EXIT_SUCCESS;
}
