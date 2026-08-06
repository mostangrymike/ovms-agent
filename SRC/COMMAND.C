#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "command_internal.h"
#include "edit_txn.h"
#include "util.h"



void openai_agent_repair(agent_state *state,
                         const char *goal);

static edit_txn command_changeset_transaction;
static int command_changeset_active = 0;

static void command_changeset_reset(void)
{
    if (command_changeset_active) {
        edit_txn_dispose(&command_changeset_transaction);
    }

    command_changeset_active = 0;
}

static void command_changeset_show(void)
{
    unsigned int index;

    if (!command_changeset_active) {
        (void)puts("No change set is active.");
        return;
    }

    (void)printf(
        "Change set contains %u operation%s.\n",
        command_changeset_transaction.file_count,
        command_changeset_transaction.file_count == 1U ? "" : "s"
    );

    for (index = 0U;
         index < command_changeset_transaction.file_count;
         ++index) {
        const edit_txn_file *file;

        file = &command_changeset_transaction.files[index];

        (void)printf(
            "%u. %s %s\n",
            index + 1U,
            file->existed_before ? "REPLACE" : "CREATE",
            file->path
        );
    }
}

void command_changeset(agent_state *state, const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *operation;
    char *path;
    char *text;
    char *extra;
    char answer[32];

    if (state == NULL) {
        return;
    }

    if (arguments == NULL || *arguments == '\0' ||
        strlen(arguments) >= sizeof(work)) {
        (void)puts(
            "Usage: CHANGESET BEGIN | ADD path \"text\" | "
            "SHOW | APPLY | CANCEL"
        );
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    operation = command_next_argument(&cursor);

    if (operation == NULL) {
        (void)puts(
            "Usage: CHANGESET BEGIN | ADD path \"text\" | "
            "SHOW | APPLY | CANCEL"
        );
        return;
    }

    util_uppercase(operation);

    if (strcmp(operation, "BEGIN") == 0) {
        extra = command_next_argument(&cursor);

        if (extra != NULL) {
            (void)puts("Usage: CHANGESET BEGIN");
            return;
        }

        if (command_changeset_active) {
            (void)puts(
                "A change set is already active. "
                "Use CHANGESET CANCEL first."
            );
            return;
        }

        edit_txn_init(&command_changeset_transaction);
        command_changeset_active = 1;
        (void)puts("Change set started.");
        return;
    }

    if (strcmp(operation, "ADD") == 0) {
        if (!command_changeset_active) {
            (void)puts(
                "No change set is active. "
                "Use CHANGESET BEGIN first."
            );
            return;
        }

        path = command_next_argument(&cursor);
        text = command_next_argument(&cursor);
        extra = command_next_argument(&cursor);

        if (path == NULL || text == NULL || extra != NULL) {
            (void)puts(
                "Usage: CHANGESET ADD path \"replacement text\""
            );
            return;
        }

        if (!command_decode_escapes(text)) {
            (void)puts(
                "Invalid escape sequence in replacement text."
            );
            return;
        }

        if (!edit_txn_add(
                &command_changeset_transaction,
                path,
                text)) {
            (void)puts(
                "Unable to add change-set operation. "
                "Check the path, duplicate targets, and file limit."
            );
            return;
        }

        (void)printf(
            "Added %s operation for %s.\n",
            command_changeset_transaction.
                files[command_changeset_transaction.file_count - 1U].
                existed_before ? "replacement" : "creation",
            path
        );
        return;
    }

    if (strcmp(operation, "SHOW") == 0) {
        extra = command_next_argument(&cursor);

        if (extra != NULL) {
            (void)puts("Usage: CHANGESET SHOW");
            return;
        }

        command_changeset_show();
        return;
    }

    if (strcmp(operation, "CANCEL") == 0) {
        extra = command_next_argument(&cursor);

        if (extra != NULL) {
            (void)puts("Usage: CHANGESET CANCEL");
            return;
        }

        if (!command_changeset_active) {
            (void)puts("No change set is active.");
            return;
        }

        command_changeset_reset();
        (void)puts("Change set cancelled.");
        return;
    }

    if (strcmp(operation, "APPLY") == 0) {
        extra = command_next_argument(&cursor);

        if (extra != NULL) {
            (void)puts("Usage: CHANGESET APPLY");
            return;
        }

        if (!command_changeset_active) {
            (void)puts("No change set is active.");
            return;
        }

        if (!state->write_enabled) {
            (void)puts("Write access is disabled.");
            return;
        }

        if (command_changeset_transaction.file_count == 0U) {
            (void)puts("The active change set is empty.");
            return;
        }

        command_changeset_show();
        (void)fputs("Apply complete change set [y/N]? ", stdout);
        (void)fflush(stdout);

        if (fgets(answer, sizeof(answer), stdin) == NULL ||
            (answer[0] != 'Y' && answer[0] != 'y')) {
            (void)puts("Change set application cancelled.");
            return;
        }

        if (!edit_txn_write(&command_changeset_transaction)) {
            (void)puts(
                "Change set failed. All completed operations "
                "were rolled back."
            );
            command_changeset_reset();
            return;
        }

        if (!edit_txn_commit(&command_changeset_transaction)) {
            (void)edit_txn_rollback(
                &command_changeset_transaction
            );
            (void)puts(
                "Unable to commit change set. "
                "All operations were rolled back."
            );
            command_changeset_reset();
            return;
        }

        (void)printf(
            "Applied %u change-set operation%s.\n",
            command_changeset_transaction.file_count,
            command_changeset_transaction.file_count == 1U ? "" : "s"
        );
        command_changeset_reset();
        return;
    }

    (void)puts(
        "Usage: CHANGESET BEGIN | ADD path \"text\" | "
        "SHOW | APPLY | CANCEL"
    );
}

#define RUN_OUTPUT_FILE "OVMS_AGENT_RUN_OUTPUT.TXT"
#define RUN_PROCEDURE_FILE "OVMS_AGENT_RUN.COM"
#define RUN_BUFFER_SIZE 1024U

static void command_remove_versions(const char *path)
{
    if (path == NULL) {
        return;
    }

    while (remove(path) == 0) {
        /* Remove all visible OpenVMS versions. */
    }
}

static int command_starts_with_word(const char *text,
                                    const char *word)
{
    size_t length;

    if (text == NULL || word == NULL) {
        return 0;
    }

    length = strlen(word);

    return strncmp(text, word, length) == 0 &&
           (text[length] == '\0' ||
            text[length] == ' ' ||
            text[length] == '\t' ||
            text[length] == '/');
}

static int command_git_is_read_only(const char *command)
{
    const char *position;
    char subcommand[32];
    size_t used;

    position = command + 3;
    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    used = 0U;
    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t' &&
           used + 1U < sizeof(subcommand)) {
        subcommand[used++] =
            (char)toupper((unsigned char)*position++);
    }
    subcommand[used] = '\0';

    return strcmp(subcommand, "STATUS") == 0 ||
           strcmp(subcommand, "DIFF") == 0 ||
           strcmp(subcommand, "LOG") == 0 ||
           strcmp(subcommand, "SHOW") == 0;
}

static int command_run_is_safe(const char *command)
{
    char upper[OVMS_AGENT_INPUT_SIZE];
    const char *position;
    size_t length;

    static const char *denied[] = {
        "DELETE", "PURGE", "RENAME", "COPY",
        "SET", "INSTALL", "MOUNT", "DISMOUNT",
        "INITIALIZE", "BACKUP", "AUTHORIZE", "RUN",
        "MCR", "SPAWN", "PIPE", "SUBMIT", "CREATE",
        "OPEN", "WRITE", "APPEND", "ASSIGN", "DEFINE",
        "DEASSIGN"
    };
    size_t index;

    if (command == NULL || *command == '\0') {
        return 0;
    }

    length = strlen(command);
    if (length >= sizeof(upper)) {
        return 0;
    }

    for (position = command; *position != '\0'; ++position) {
        if (*position == '\n' ||
            *position == '\r' ||
            *position == ';' ||
            *position == '|' ||
            *position == '<' ||
            *position == '>' ||
            *position == '@' ||
            *position == '&' ||
            *position == '!' ||
            *position == '\'') {
            return 0;
        }
    }

    (void)strcpy(upper, command);
    util_uppercase(upper);

    for (index = 0U;
         index < sizeof(denied) / sizeof(denied[0]);
         ++index) {
        if (command_starts_with_word(upper, denied[index])) {
            return 0;
        }
    }

    if (strstr(upper, "/OUTPUT") != NULL ||
        strstr(upper, "/PRINTER") != NULL) {
        return 0;
    }

    if (command_starts_with_word(upper, "GIT")) {
        return command_git_is_read_only(upper);
    }

    return command_starts_with_word(upper, "SHOW") ||
           command_starts_with_word(upper, "DIRECTORY") ||
           command_starts_with_word(upper, "SEARCH") ||
           command_starts_with_word(upper, "TYPE") ||
           command_starts_with_word(upper, "DIFFERENCES");
}

static void command_print_file(const char *path)
{
    FILE *file;
    char buffer[RUN_BUFFER_SIZE];

    file = fopen(path, "r");
    if (file == NULL) {
        (void)puts("No command output was captured.");
        return;
    }

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        (void)fputs(buffer, stdout);
        if (strchr(buffer, '\n') == NULL) {
            (void)putchar('\n');
        }
    }

    (void)fclose(file);
}

void command_run(agent_state *state, const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *command;
    char *extra;
    char answer[32];
    FILE *procedure;
    int status;

    if (state == NULL) {
        return;
    }

    if (!state->dcl_enabled) {
        (void)puts("DCL execution is disabled.");
        return;
    }

    if (arguments == NULL || *arguments == '\0' ||
        strlen(arguments) >= sizeof(work)) {
        (void)puts("Usage: RUN \"command\"");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;
    command = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (command == NULL || *command == '\0' || extra != NULL) {
        (void)puts("Usage: RUN \"command\"");
        return;
    }

    if (!command_decode_escapes(command)) {
        (void)puts("Invalid escape sequence in RUN command.");
        return;
    }

    if (!command_run_is_safe(command)) {
        (void)puts("RUN command refused by the safety policy.");
        return;
    }

    (void)printf("Command: %s\n", command);
    (void)fputs("Execute command [y/N]? ", stdout);
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL ||
        (answer[0] != 'Y' && answer[0] != 'y')) {
        (void)puts("Command cancelled.");
        return;
    }

    command_remove_versions(RUN_OUTPUT_FILE);
    command_remove_versions(RUN_PROCEDURE_FILE);

    procedure = fopen(RUN_PROCEDURE_FILE, "w");
    if (procedure == NULL) {
        (void)puts("Unable to create the temporary RUN procedure.");
        return;
    }

    (void)fprintf(procedure,
                  "$ DEFINE SYS$OUTPUT %s\n"
                  "$ DEFINE SYS$ERROR SYS$OUTPUT\n"
                  "$ %s\n"
                  "$ RUN_STATUS = $STATUS\n"
                  "$ EXIT 'RUN_STATUS'\n",
                  RUN_OUTPUT_FILE,
                  command);

    if (fclose(procedure) != 0) {
        command_remove_versions(RUN_PROCEDURE_FILE);
        (void)puts("Unable to finish the temporary RUN procedure.");
        return;
    }

    status = system("@" RUN_PROCEDURE_FILE);

    command_print_file(RUN_OUTPUT_FILE);
    (void)printf("OpenVMS completion status: %d\n", status);

    command_remove_versions(RUN_OUTPUT_FILE);
    command_remove_versions(RUN_PROCEDURE_FILE);
}

void command_prompt(void)
{
    (void)fputs("OVMS-AGENT> ", stdout);
    (void)fflush(stdout);
}



void command_agent_repair(agent_state *state,
                          const char *arguments)
{
    openai_agent_repair(state, arguments);
}


void command_execute(agent_state *state, char *input)
{
    const command_entry *entry;
    char *command;
    char *arguments;
    char *position;

    if (state == NULL || input == NULL) {
        return;
    }

    util_trim(input);
    command = util_skip_space(input);

    if (command == NULL || *command == '\0') {
        return;
    }

    position = command;

    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t') {
        ++position;
    }

    if (*position != '\0') {
        *position = '\0';
        arguments = util_skip_space(position + 1);
    } else {
        arguments = position;
    }

    util_uppercase(command);

    if (strcmp(command, "RUN") == 0) {
        command_run(state, arguments);
        return;
    }

    if (strcmp(command, "AGENT/REPAIR") == 0) {
        command_agent_repair(state, arguments);
        return;
    }

    if (strcmp(command, "CHANGESET") == 0) {
        command_changeset(state, arguments);
        return;
    }

    entry = command_find(command);

    if (entry != NULL) {
        entry->handler(state, arguments);
        return;
    }

    if (strcmp(command, "LIST/ALL") == 0) {
        extern void project_list(agent_state *,
                                 const char *,
                                 int);
        project_list(state, arguments, 1);
        return;
    }

    (void)printf("Unknown command: %s\n", command);
    (void)puts("Enter HELP for available commands.");
}

int command_decode_escapes(char *text)
{
    char *source;
    char *destination;

    if (text == NULL) {
        return 0;
    }

    source = text;
    destination = text;

    while (*source != '\0') {
        if ((unsigned char)*source != 92U) {
            *destination++ = *source++;
            continue;
        }

        ++source;

        if (*source == '\0') {
            return 0;
        }

        switch (*source) {
        case 'n':
            *destination++ = '\n';
            break;
        case 'r':
            *destination++ = '\r';
            break;
        case 't':
            *destination++ = '\t';
            break;
        case '"':
            *destination++ = '"';
            break;
        default:
            if ((unsigned char)*source == 92U) {
                *destination++ = (char)92;
            } else {
                return 0;
            }
            break;
        }

        ++source;
    }

    *destination = '\0';
    return 1;
}

char *command_next_argument(char **cursor)
{
    char *start;
    char *end;

    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    start = util_skip_space(*cursor);

    if (start == NULL || *start == '\0') {
        *cursor = start;
        return NULL;
    }

    if ((unsigned char)*start == 34U) {
        ++start;
        end = start;

        while (*end != '\0') {
            if ((unsigned char)*end == 92U &&
                end[1] != '\0') {
                end += 2;
                continue;
            }

            if ((unsigned char)*end == 34U) {
                break;
            }

            ++end;
        }

        if ((unsigned char)*end != 34U) {
            return NULL;
        }

        *end = '\0';
        *cursor = end + 1;
        return start;
    }

    end = start;

    while (*end != '\0' &&
           *end != ' ' &&
           *end != '\t') {
        ++end;
    }

    if (*end != '\0') {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = end;
    }

    return start;
}

#include "command_m147_guard.inc"
