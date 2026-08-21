/* M267 STATE transition boundary: compile the preserved core with neutral
 * public entry points without exporting global compatibility aliases. */
#define openai_save_state llm_save_state
#define openai_state_save llm_state_save
#define openai_load_state llm_load_state
#define openai_show_state llm_show_state
#define openai_show_memory llm_show_memory
#define openai_clear_state llm_clear_state
#include "LLM_STATE_CORE.C"
#undef openai_clear_state
#undef openai_show_memory
#undef openai_show_state
#undef openai_load_state
#undef openai_state_save
#undef openai_save_state
