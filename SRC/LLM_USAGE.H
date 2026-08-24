#ifndef LLM_USAGE_H
#define LLM_USAGE_H

#include <stddef.h>

typedef struct llm_usage_stats {
    int last_known;
    unsigned long requests;
    unsigned long last_input;
    unsigned long last_output;
    unsigned long last_total;
    unsigned long session_input;
    unsigned long session_output;
    unsigned long session_total;
} llm_usage_stats;

int llm_usage_record_json(const char *json);
int llm_usage_record_file(const char *path);
void llm_usage_get(llm_usage_stats *stats);
int llm_usage_status(char *output,
                     size_t output_size,
                     unsigned int turn,
                     unsigned int limit);
void m273_show_metrics(void);

#endif /* LLM_USAGE_H */
