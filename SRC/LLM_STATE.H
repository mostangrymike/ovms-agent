#ifndef LLM_STATE_H
#define LLM_STATE_H

void llm_save_state(void);
void llm_state_save(void);
void llm_load_state(void);
void llm_show_state(void);
void llm_show_memory(void);
void llm_clear_state(void);

/* Temporary M267 source compatibility for remaining live consumers. */

#endif /* LLM_STATE_H */
