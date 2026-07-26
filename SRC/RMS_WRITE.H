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

#endif /* OVMS_AGENT_RMS_WRITE_H */
