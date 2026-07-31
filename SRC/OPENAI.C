#include "openai_internal.h"

char previous_response_id[OPENAI_RESPONSE_ID_SIZE];

int openai_last_workflow = OPENAI_WORKFLOW_NONE;
int openai_last_build_known = 0;
int openai_last_build_status = 0;
int openai_last_rollback = OPENAI_ROLLBACK_NONE;
int openai_saved_workflow = OPENAI_WORKFLOW_NONE;
int openai_saved_build_known = 0;
int openai_saved_build_status = 0;
int openai_saved_rollback = OPENAI_ROLLBACK_NONE;
int openai_state_loaded = 0;
int openai_state_valid = 0;
int openai_state_recovered = 0;
int openai_state_save_known = 0;
int openai_state_save_succeeded = 0;
 
