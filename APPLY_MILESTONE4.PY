from pathlib import Path

files = {
"SRC/AGENT.H": r'''
#ifndef OVMS_AGENT_AGENT_H
#define OVMS_AGENT_AGENT_H

#define OVMS_AGENT_VERSION "0.4.0"
#define OVMS_AGENT_INPUT_SIZE 1024

typedef struct agent_state {
int running;
const char *project_root;
int api_key_defined;
int write_enabled;
int dcl_enabled;
} agent_state;

int agent_initialize(agent_state *state);
void agent_shutdown(agent_state *state);
int agent_is_running(const agent_state *state);
void agent_stop(agent_state *state);

#endif
''',

"SRC/PROJECT.H": r'''
#ifndef OVMS_AGENT_PROJECT_H
#define OVMS_AGENT_PROJECT_H

#include "agent.h"

void project_show_root(const agent_state *state);
void project_show_status(const agent_state *state);
void project_list(const agent_state *state,
const char *path,
int show_all);
void project_read(const agent_state *state,
const char *path);
void project_search(const agent_state *state,
const char *pattern,
const char *path);
void project_git_status(const agent_state *state);

#endif
''',

"SRC/PROJECT.C": r'''
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project.h"

#define READ_BUFFER_SIZE 1024
#define COMMAND_BUFFER_SIZE 2048

static int path_is_safe(const char *path)
{
if (path == NULL || *path == '\0') {
return 0;
}

```
if (strstr(path, "..") != NULL) {
    return 0;
}

if (strchr(path, ':') != NULL) {
    return 0;
}

if (*path == '/') {
    return 0;
}

if (strchr(path, '[') != NULL &&
    strncmp(path, "[.", 2U) != 0) {
    return 0;
}

return 1;
```

}

static int is_hidden_entry(const char *name)
{
if (name == NULL) {
return 1;
}

```
if (strcmp(name, ".") == 0 ||
    strcmp(name, "..") == 0) {
    return 1;
}

if (strncmp(name, "^.git", 5U) == 0 ||
    strncmp(name, ".git", 4U) == 0) {
    return 1;
}

return 0;
```

}

static int contains_ignore_case(const char *text,
const char *pattern)
{
const unsigned char *start;

```
if (text == NULL || pattern == NULL || *pattern == '\0') {
    return 0;
}

for (start = (const unsigned char *)text;
     *start != (unsigned char)'\0';
     ++start) {
    const unsigned char *left;
    const unsigned char *right;

    left = start;
    right = (const unsigned char *)pattern;

    while (*left != (unsigned char)'\0' &&
           *right != (unsigned char)'\0' &&
           tolower((int)*left) == tolower((int)*right)) {
        ++left;
        ++right;
    }

    if (*right == (unsigned char)'\0') {
        return 1;
    }
}

return 0;
```

}

void project_show_root(const agent_state *state)
{
if (state == NULL ||
state->project_root == NULL ||
*state->project_root == '\0') {
(void)puts("OVMS_AGENT_ROOT is not defined.");
return;
}

```
(void)printf("Project root: %s\n", state->project_root);
```

}

void project_show_status(const agent_state *state)
{
if (state == NULL) {
(void)puts("Agent state is unavailable.");
return;
}

```
(void)printf("Version:       %s\n", OVMS_AGENT_VERSION);
(void)printf("Project root:  %s\n",
    state->project_root != NULL &&
    *state->project_root != '\0'
        ? state->project_root
        : "not defined");
(void)printf("API key:       %s\n",
    state->api_key_defined ? "defined" : "not defined");
(void)printf("Write access:  %s\n",
    state->write_enabled ? "enabled" : "disabled");
(void)printf("DCL execution: %s\n",
    state->dcl_enabled ? "enabled" : "disabled");
```

}

void project_list(const agent_state *state,
const char *path,
int show_all)
{
const char *target;
DIR *directory;
struct dirent *entry;

```
if (state == NULL ||
    state->project_root == NULL ||
    *state->project_root == '\0') {
    (void)puts("OVMS_AGENT_ROOT is not defined.");
    return;
}

target = path;

if (target == NULL || *target == '\0') {
    target = ".";
} else if (!path_is_safe(target)) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

directory = opendir(target);

if (directory == NULL) {
    (void)printf("Unable to open directory: %s\n", target);
    (void)printf("Reason: %s\n", strerror(errno));
    return;
}

(void)printf("Directory: %s\n\n", target);

while ((entry = readdir(directory)) != NULL) {
    if (show_all || !is_hidden_entry(entry->d_name)) {
        (void)printf("%s\n", entry->d_name);
    }
}

if (closedir(directory) != 0) {
    (void)printf("Warning: unable to close directory: %s\n",
                 strerror(errno));
}
```

}

void project_read(const agent_state *state,
const char *path)
{
FILE *file;
char buffer[READ_BUFFER_SIZE];
unsigned long line_number;

```
if (state == NULL ||
    state->project_root == NULL ||
    *state->project_root == '\0') {
    (void)puts("OVMS_AGENT_ROOT is not defined.");
    return;
}

if (!path_is_safe(path)) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

file = fopen(path, "r");

if (file == NULL) {
    (void)printf("Unable to read %s\n", path);
    (void)printf("Reason: %s\n", strerror(errno));
    return;
}

line_number = 1UL;

while (fgets(buffer, sizeof(buffer), file) != NULL) {
    (void)printf("%6lu  %s", line_number, buffer);

    if (strchr(buffer, '\n') == NULL) {
        (void)putchar('\n');
    }

    ++line_number;
}

if (ferror(file)) {
    (void)printf("Read error: %s\n", strerror(errno));
}

if (fclose(file) != 0) {
    (void)printf("Warning: unable to close file: %s\n",
                 strerror(errno));
}
```

}

void project_search(const agent_state *state,
const char *pattern,
const char *path)
{
FILE *file;
char buffer[READ_BUFFER_SIZE];
unsigned long line_number;
unsigned long matches;

```
if (state == NULL ||
    state->project_root == NULL ||
    *state->project_root == '\0') {
    (void)puts("OVMS_AGENT_ROOT is not defined.");
    return;
}

if (pattern == NULL || *pattern == '\0') {
    (void)puts("Search pattern is empty.");
    return;
}

if (!path_is_safe(path)) {
    (void)puts("Unsafe or invalid project-relative path.");
    return;
}

file = fopen(path, "r");

if (file == NULL) {
    (void)printf("Unable to search %s\n", path);
    (void)printf("Reason: %s\n", strerror(errno));
    return;
}

line_number = 1UL;
matches = 0UL;

while (fgets(buffer, sizeof(buffer), file) != NULL) {
    if (contains_ignore_case(buffer, pattern)) {
        (void)printf("%s:%lu: %s",
                     path,
                     line_number,
                     buffer);

        if (strchr(buffer, '\n') == NULL) {
            (void)putchar('\n');
        }

        ++matches;
    }

    ++line_number;
}

if (ferror(file)) {
    (void)printf("Search error: %s\n", strerror(errno));
}

if (fclose(file) != 0) {
    (void)printf("Warning: unable to close file: %s\n",
                 strerror(errno));
}

(void)printf("%lu matching line%s.\n",
             matches,
             matches == 1UL ? "" : "s");
```

}

void project_git_status(const agent_state *state)
{
int status;

```
if (state == NULL ||
    state->project_root == NULL ||
    *state->project_root == '\0') {
    (void)puts("OVMS_AGENT_ROOT is not defined.");
    return;
}

(void)puts("Git status:");

status = system("git status --short");

if (status != 0) {
    (void)printf("Git returned status %d.\n", status);
    (void)puts("Ensure GIT$STARTUP has been executed.");
}
```

}
''',

"SRC/COMMAND.C": r'''
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "project.h"
#include "util.h"

typedef void (*command_handler)(agent_state *state,
const char *arguments);

typedef struct command_entry {
const char *name;
const char *description;
command_handler handler;
} command_entry;

static void command_help(agent_state *state,
const char *arguments);
static void command_version(agent_state *state,
const char *arguments);
static void command_root(agent_state *state,
const char *arguments);
static void command_status(agent_state *state,
const char *arguments);
static void command_list(agent_state *state,
const char *arguments);
static void command_read(agent_state *state,
const char *arguments);
static void command_search(agent_state *state,
const char *arguments);
static void command_git_status(agent_state *state,
const char *arguments);
static void command_quit(agent_state *state,
const char *arguments);

static const command_entry command_table[] = {
{ "HELP",      "Display command help", command_help },
{ "VERSION",   "Display agent version", command_version },
{ "ROOT",      "Display the project root", command_root },
{ "STATUS",    "Display agent status", command_status },
{ "LIST",      "List root or directory: LIST [path]", command_list },
{ "READ",      "Read a file: READ file", command_read },
{ "SEARCH",    "Search a file: SEARCH text file", command_search },
{ "GITSTATUS", "Display Git working-tree status", command_git_status },
{ "QUIT",      "Exit OVMS Agent", command_quit },
{ "EXIT",      "Exit OVMS Agent", command_quit },
{ NULL, NULL, NULL }
};

void command_prompt(void)
{
(void)fputs("OVMS-AGENT> ", stdout);
(void)fflush(stdout);
}

void command_execute(agent_state *state, char *input)
{
const command_entry *entry;
char *command;
char *arguments;
char *position;

```
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

if (strcmp(command, "LIST/ALL") == 0) {
    project_list(state, arguments, 1);
    return;
}

for (entry = command_table; entry->name != NULL; ++entry) {
    if (strcmp(command, entry->name) == 0) {
        entry->handler(state, arguments);
        return;
    }
}

(void)printf("Unknown command: %s\n", command);
(void)puts("Enter HELP for available commands.");
```

}

static void command_help(agent_state *state,
const char *arguments)
{
const command_entry *entry;

```
(void)state;
(void)arguments;
(void)puts("Commands:");

for (entry = command_table; entry->name != NULL; ++entry) {
    if (strcmp(entry->name, "EXIT") != 0) {
        (void)printf("  %-10s %s\n",
                     entry->name,
                     entry->description);
    }
}

(void)puts("  LIST/ALL   List all directory entries");
```

}

static void command_version(agent_state *state,
const char *arguments)
{
(void)state;
(void)arguments;
(void)printf("OVMS Agent %s\n", OVMS_AGENT_VERSION);
}

static void command_root(agent_state *state,
const char *arguments)
{
(void)arguments;
project_show_root(state);
}

static void command_status(agent_state *state,
const char *arguments)
{
(void)arguments;
project_show_status(state);
}

static void command_list(agent_state *state,
const char *arguments)
{
project_list(state, arguments, 0);
}

static void command_read(agent_state *state,
const char *arguments)
{
if (arguments == NULL || *arguments == '\0') {
(void)puts("Usage: READ project-relative-file");
return;
}

```
project_read(state, arguments);
```

}

static void command_search(agent_state *state,
const char *arguments)
{
char work[OVMS_AGENT_INPUT_SIZE];
char *pattern;
char *path;
char *separator;

```
if (arguments == NULL || *arguments == '\0') {
    (void)puts("Usage: SEARCH pattern file");
    return;
}

if (strlen(arguments) >= sizeof(work)) {
    (void)puts("Search arguments are too long.");
    return;
}

(void)strcpy(work, arguments);
pattern = util_skip_space(work);
separator = pattern;

while (*separator != '\0' &&
       *separator != ' ' &&
       *separator != '\t') {
    ++separator;
}

if (*separator == '\0') {
    (void)puts("Usage: SEARCH pattern file");
    return;
}

*separator = '\0';
path = util_skip_space(separator + 1);

if (*path == '\0') {
    (void)puts("Usage: SEARCH pattern file");
    return;
}

project_search(state, pattern, path);
```

}

static void command_git_status(agent_state *state,
const char *arguments)
{
(void)arguments;
project_git_status(state);
}

static void command_quit(agent_state *state,
const char *arguments)
{
(void)arguments;
agent_stop(state);
}
'''
}

for filename, content in files.items():
    path = Path(filename)
    path.parent.mkdir(parents=True, exist_ok=True)

```
    with path.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.write(content.lstrip())

    print(f"Created {filename}")
```


print("Milestone 4 source files created.")
