#ifndef LLM_STATE_H
#define LLM_STATE_H

void llm_state_save(void);
void llm_load_state(void);
void llm_show_state(void);
void llm_show_memory(void);
void llm_clear_state(void);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_state_save llm_state_save
#define openai_load_state llm_load_state
#define openai_show_state llm_show_state
#define openai_show_memory llm_show_memory
#define openai_clear_state llm_clear_state

#endif /* LLM_STATE_H */
