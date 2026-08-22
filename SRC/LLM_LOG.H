#ifndef LLM_LOG_H
#define LLM_LOG_H

void llm_log_event(const char *workflow,
                      const char *event,
                      int status);
void llm_show_log(void);
void llm_show_old_log(void);
void llm_clear_log(void);
void llm_show_metrics(void);

/* Temporary M267 source compatibility for remaining live consumers. */

#endif /* LLM_LOG_H */
