#include "LLM_AUTO.H"

/* M268 keeps the mature counter implementation and wraps only its public
 * lifecycle entry points so nested AUTOPILOT work shares one hard budget.
 */
#define llm_auto_begin llm_auto_begin_base
#define llm_auto_turn_limit llm_auto_turn_limit_base
#define llm_auto_note_turn llm_auto_note_turn_base
#define llm_auto_note_tool llm_auto_note_tool_base
#define llm_auto_allow_write llm_auto_allow_write_base
#define llm_auto_finish llm_auto_finish_base
#include "LLM_AUTO_CORE.INC"
#undef llm_auto_begin
#undef llm_auto_turn_limit
#undef llm_auto_note_turn
#undef llm_auto_note_tool
#undef llm_auto_allow_write
#undef llm_auto_finish

#include "LLM_AUTOPILOT_AUTO.INC"
