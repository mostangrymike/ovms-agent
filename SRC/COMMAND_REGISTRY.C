#include <stdio.h>
#include <string.h>

#include "command_internal.h"

#define COMMAND_REGISTRY_MAX 256U

void command_register_provider(void);
void command_register_settings(void);
void command_register_github(void);

static command_entry command_registry[COMMAND_REGISTRY_MAX];
static size_t command_registry_used;
static int command_registry_ready;

int command_registry_add(const command_entry *entries,
                         size_t entry_count)
{
    size_t index;
    size_t existing;

    if (entries == NULL ||
        entry_count == 0U ||
        command_registry_used + entry_count >
            COMMAND_REGISTRY_MAX) {
        return 0;
    }

    for (index = 0U; index < entry_count; ++index) {
        if (entries[index].name == NULL ||
            entries[index].description == NULL ||
            entries[index].handler == NULL) {
            return 0;
        }

        for (existing = 0U;
             existing < command_registry_used;
             ++existing) {
            if (strcmp(entries[index].name,
                       command_registry[existing].name) == 0) {
                (void)printf(
                    "Duplicate command registration: %s\n",
                    entries[index].name
                );
                return 0;
            }
        }

        command_registry[command_registry_used++] =
            entries[index];
    }

    return 1;
}

void command_registry_initialize(void)
{
    if (command_registry_ready) {
        return;
    }

    command_registry_used = 0U;

    command_register_core();
    command_register_project();
    command_register_symbol();
    command_register_agent();
    command_register_provider();
    command_register_settings();
    command_register_github();

    command_registry_ready = 1;
}

const command_entry *command_find(const char *name)
{
    size_t index;

    if (name == NULL) {
        return NULL;
    }

    command_registry_initialize();

    for (index = 0U;
         index < command_registry_used;
         ++index) {
        if (strcmp(name, command_registry[index].name) == 0) {
            return &command_registry[index];
        }
    }

    return NULL;
}

const command_entry *command_registry_get(size_t index)
{
    command_registry_initialize();

    if (index >= command_registry_used) {
        return NULL;
    }

    return &command_registry[index];
}

size_t command_registry_count(void)
{
    command_registry_initialize();
    return command_registry_used;
}