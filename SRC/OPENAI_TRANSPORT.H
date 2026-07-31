#ifndef OPENAI_TRANSPORT_H
#define OPENAI_TRANSPORT_H

int write_headers(const char *api_key);
void remove_temporary_files(void);
int perform_openai_request(void);

#endif /* OPENAI_TRANSPORT_H */
