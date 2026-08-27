#ifndef LLM_RATE_LIMIT_H
#define LLM_RATE_LIMIT_H

int llm_rate_limit_delay(const char *json,
                         unsigned int *wait_seconds);

#endif /* LLM_RATE_LIMIT_H */
