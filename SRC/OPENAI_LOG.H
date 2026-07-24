#ifndef OPENAI_LOG_H
#define OPENAI_LOG_H

void openai_log_event(const char *workflow,
                      const char *event,
                      int status);
void openai_show_log(void);
void openai_show_old_log(void);
void openai_clear_log(void);
void openai_show_metrics(void);

#endif /* OPENAI_LOG_H */
