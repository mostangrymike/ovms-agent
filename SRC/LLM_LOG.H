#ifndef LLM_LOG_H
#define LLM_LOG_H

void openai_log_event(const char *workflow,
                      const char *event,
                      int status);
void llm_show_log(void);
void llm_show_old_log(void);
void llm_clear_log(void);
void llm_show_metrics(void);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_show_log llm_show_log
#define openai_show_old_log llm_show_old_log
#define openai_clear_log llm_clear_log
#define openai_show_metrics llm_show_metrics

#endif /* LLM_LOG_H */
