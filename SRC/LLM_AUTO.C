#include "LLM_AUTO.H"

#define openai_auto_turn_override llm_auto_turn_override
#define openai_auto_write_override llm_auto_write_override
#define openai_auto_cur_turns llm_auto_cur_turns
#define openai_auto_cur_tools llm_auto_cur_tools
#define openai_auto_cur_writes llm_auto_cur_writes
#define openai_auto_cur_workflow llm_auto_cur_workflow
#define openai_auto_write_blocked llm_auto_write_blocked
#define openai_auto_last_turns llm_auto_last_turns
#define openai_auto_last_tools llm_auto_last_tools
#define openai_auto_last_writes llm_auto_last_writes
#define openai_auto_last_workflow llm_auto_last_workflow
#define openai_auto_last_reason llm_auto_last_reason
#define openai_auto_env_limit llm_auto_env_limit
#define openai_auto_turn_cfg llm_auto_turn_cfg
#define openai_auto_write_cfg llm_auto_write_cfg
#define openai_auto_begin llm_auto_begin
#define openai_auto_turn_limit llm_auto_turn_limit
#define openai_auto_note_turn llm_auto_note_turn
#define openai_auto_note_tool llm_auto_note_tool
#define openai_auto_allow_write llm_auto_allow_write
#define openai_auto_partial_limit llm_auto_partial_limit
#define openai_auto_finish llm_auto_finish
#define openai_auto_limits_text llm_auto_limits_text
#define openai_auto_status_text llm_auto_status_text
#define openai_show_auto_limits llm_show_auto_limits
#define openai_show_auto_status llm_show_auto_status
#define openai_auto_reset llm_auto_reset
#define openai_auto_test_limits llm_auto_test_limits

#include "LLM_AUTO_CORE.INC"

#undef openai_auto_test_limits
#undef openai_auto_reset
#undef openai_show_auto_status
#undef openai_show_auto_limits
#undef openai_auto_status_text
#undef openai_auto_limits_text
#undef openai_auto_finish
#undef openai_auto_partial_limit
#undef openai_auto_allow_write
#undef openai_auto_note_tool
#undef openai_auto_note_turn
#undef openai_auto_turn_limit
#undef openai_auto_begin
#undef openai_auto_write_cfg
#undef openai_auto_turn_cfg
#undef openai_auto_env_limit
#undef openai_auto_last_reason
#undef openai_auto_last_workflow
#undef openai_auto_last_writes
#undef openai_auto_last_tools
#undef openai_auto_last_turns
#undef openai_auto_write_blocked
#undef openai_auto_cur_workflow
#undef openai_auto_cur_writes
#undef openai_auto_cur_tools
#undef openai_auto_cur_turns
#undef openai_auto_write_override
#undef openai_auto_turn_override

/* Temporary compatibility wrappers for broad legacy consumers. */
void openai_auto_begin(int w) { llm_auto_begin(w); }
unsigned int openai_auto_turn_limit(int w) { return llm_auto_turn_limit(w); }
void openai_auto_note_turn(void) { llm_auto_note_turn(); }
void openai_auto_note_tool(void) { llm_auto_note_tool(); }
int openai_auto_allow_write(void) { return llm_auto_allow_write(); }
int openai_auto_partial_limit(void) { return llm_auto_partial_limit(); }
void openai_auto_finish(const char *r) { llm_auto_finish(r); }
int openai_auto_limits_text(char *o, size_t n) { return llm_auto_limits_text(o, n); }
int openai_auto_status_text(char *o, size_t n) { return llm_auto_status_text(o, n); }
void openai_show_auto_limits(void) { llm_show_auto_limits(); }
void openai_show_auto_status(void) { llm_show_auto_status(); }
void openai_auto_reset(void) { llm_auto_reset(); }
void openai_auto_test_limits(unsigned int t, unsigned int w)
{ llm_auto_test_limits(t, w); }
