#ifndef LLM_AUTO_H
#define LLM_AUTO_H

#include <stddef.h>

void llm_auto_begin(int workflow);
unsigned int llm_auto_turn_limit(int workflow);
void llm_auto_note_turn(void);
void llm_auto_note_tool(void);
int llm_auto_allow_write(void);
int llm_auto_partial_limit(void);
void llm_auto_finish(const char *reason);
int llm_auto_limits_text(char *output, size_t output_size);
int llm_auto_status_text(char *output, size_t output_size);
void llm_show_auto_limits(void);
void llm_show_auto_status(void);
void llm_auto_reset(void);
void llm_auto_test_limits(unsigned int turns, unsigned int writes);

/* M268 outer-loop budget scope.  Nested model/tool runs consume these same
 * counters while AGENT/AUTOPILOT is active.
 */
void llm_auto_outer_begin(void);
void llm_auto_outer_end(const char *reason);
int llm_auto_take_turns(unsigned int count);
int llm_auto_take_writes(unsigned int count);
int llm_auto_budget_exhausted(void);

/* Focused M268 regression hooks. */
int llm_m268_outer_active(void);
unsigned int llm_m268_turn_count(void);
unsigned int llm_m268_write_count(void);

#endif /* LLM_AUTO_H */
