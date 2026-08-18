#ifndef OVMS_AGENT_RMS_WRITE_H
#define OVMS_AGENT_RMS_WRITE_H

int rms_path_requires_record_writer(
    const char *path);

int rms_write_text_file(
    const char *path,
    const char *text);

int rms_replace_text_file(
    const char *path,
    const char *text);

void rms_run_begin(void);
void rms_run_commit(void);
int rms_run_rollback(void);
int rms_run_has_writes(void);

#endif /* OVMS_AGENT_RMS_WRITE_H */
