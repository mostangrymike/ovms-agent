/* M267 neutral compatibility boundary for the mature M258 parity body. */
/* M268 overlays a scoped AUTOPILOT session policy without changing the
 * mature approval engine.  The base engine is deliberately kept at
 * WORKSPACE while AUTOPILOT is active, so every FULL gate remains closed.
 */
#define llm_approval_name llm_approval_name_base
#define llm_approval_text llm_approval_text_base
#define llm_show_approval llm_show_approval_base
#define llm_set_approval llm_set_approval_base
#define llm_set_approval_cmd llm_set_approval_cmd_base
#define llm_reset_approval llm_reset_approval_base
#include "LLM_PARITY_M258_CORE.C"
#undef llm_approval_name
#undef llm_approval_text
#undef llm_show_approval
#undef llm_set_approval
#undef llm_set_approval_cmd
#undef llm_reset_approval

#include "LLM_CREATE_CONTEXT_M277.INC"
#include "LLM_AUTOPILOT_POLICY.INC"
