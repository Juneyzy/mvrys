#ifndef __STD_AUDIO_FILE_H__
#define __STD_AUDIO_FILE_H__

void * rt_open_telephony_write (const char *name, int channels, int sample_rate, int form);
void *rt_open_telephony_read(const char *name, int channels);
int 	rt_close_telephony (SNDFILE *handle);

#endif
