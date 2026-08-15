#ifndef OPENAI_REQUEST_BASIC_H
#define OPENAI_REQUEST_BASIC_H

int write_request(const char *model,
                  const char *prompt,
                  const char *previous_id);
int write_request_image(const char *model,
                        const char *prompt,
                        const char *previous_id,
                        const char *image_path);

#endif /* OPENAI_REQUEST_BASIC_H */
