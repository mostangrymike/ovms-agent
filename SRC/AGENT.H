#ifndef OVMS_AGENT_AGENT_H
#define OVMS_AGENT_AGENT_H

#define OVMS_AGENT_VERSION "0.3.0"
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
