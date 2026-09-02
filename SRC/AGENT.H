#ifndef OVMS_AGENT_AGENT_H
#define OVMS_AGENT_AGENT_H

#define OVMS_AGENT_VERSION "1.0.0"
#define OVMS_AGENT_INPUT_SIZE 8192
#define OVMS_AGENT_MULTILINE_SIZE 32768U
#define OVMS_AGENT_ROOT_SIZE 1024

typedef struct agent_state {
    int running;
    const char *project_root;
    char project_root_text[OVMS_AGENT_ROOT_SIZE];
    int api_key_defined;
    int write_enabled;
    int dcl_enabled;
} agent_state;

int agent_api_key_present(const char *api_key);
int agent_root_matches(const char *root);
int agent_initialize(agent_state *state);
void agent_shutdown(agent_state *state);
int agent_is_running(const agent_state *state);
void agent_stop(agent_state *state);

#endif
