/* M268 bounded AUTOPILOT orchestration.
 *
 * Keep this controller in its own object so legacy focused tests that link
 * only LLM_RETRY.OBJ retain the mature retry module's original dependency
 * surface.
 */
#include "llm_internal.h"
#include "LLM_AUTOPILOT_CORE.INC"
