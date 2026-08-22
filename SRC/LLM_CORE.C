#include "llm_internal.h"

char previous_response_id[LLM_RESPONSE_ID_SIZE];

int llm_last_workflow = LLM_WORKFLOW_NONE;
int llm_last_build_known = 0;
int llm_last_build_status = 0;
int llm_last_rollback = LLM_ROLLBACK_NONE;
int llm_saved_workflow = LLM_WORKFLOW_NONE;
int llm_saved_build_known = 0;
int llm_saved_build_status = 0;
int llm_saved_rollback = LLM_ROLLBACK_NONE;
int llm_state_loaded = 0;
int llm_state_valid = 0;
int llm_state_recovered = 0;
int llm_state_save_known = 0;
int llm_state_save_succeeded = 0;
