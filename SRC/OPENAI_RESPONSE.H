#ifndef OPENAI_RESPONSE_H
#define OPENAI_RESPONSE_H

int save_response_id(const char *json);
int display_clean_response(void);
void display_raw_response(void);
int display_api_error_from_json(const char *json);
int display_output_text_from_json(const char *json);

#endif /* OPENAI_RESPONSE_H */
