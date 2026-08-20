#ifndef LLM_JSON_PARSE_H
#define LLM_JSON_PARSE_H

#include <stddef.h>

char *read_entire_file(const char *path, size_t *length_out);
const char *skip_space(const char *position);
int hex_value(int ch);
int append_utf8(char **destination,
                size_t *remaining,
                unsigned int codepoint);
char *json_decode_string(const char *start,
                         const char **after);
int json_key_matches(const char *position,
                     const char *key,
                     const char **value_start);
const char *find_string_value(const char *start,
                              const char *key);
const char *find_output_text_object(const char *json);
char *extract_top_level_id(const char *json);
const char *find_function_call_object(const char *json);
int extract_function_call(const char *json,
                          char **name,
                          char **call_id,
                          char **arguments);
char *extract_path_argument(const char *arguments);
char *extract_string_argument(const char *arguments,
                              const char *name);
int extract_integer_argument(const char *arguments,
                             const char *name,
                             long *value_out);
char *extract_output_items_json(const char *json);
char *extract_output_text_from_json(const char *json);

#endif /* LLM_JSON_PARSE_H */
