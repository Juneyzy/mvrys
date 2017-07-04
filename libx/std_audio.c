
#include "sysdefs.h"
#include "sndfile.h"
#include "stdform.h"
#include "G711.h"

int rt_close_telephony (SNDFILE *handle)
{
	return    sf_close(handle);
}

void *rt_open_telephony_write (const char *name, int channels, int sample_rate, int form)
	{
	SNDFILE *handle;
	SF_INFO info;

	memset(&info, 0, sizeof(info));
	info.frames = 0;
	info.samplerate = sample_rate;
	info.channels = channels;
	info.format = form;
	info.sections = 1;
	info.seekable = 1;

	if ((handle = sf_open(name, SFM_WRITE, &info)) == NULL) {
		rt_log_warning (ERRNO_NO_ELEMENT, "    Cannot open audio file '%s' for writing", name);
		return NULL;
	}

	return handle;
}

void *rt_open_telephony_read(const char *name, int channels)
{
	SNDFILE *handle;
	SF_INFO info;

	memset (&info, 0, sizeof(info));

	if ((handle = sf_open(name, SFM_READ, &info)) == NULL) {
		rt_log_warning (ERRNO_NO_ELEMENT, "    Cannot open audio file '%s' for reading", name);
		return NULL;
	}
	
	if (info.samplerate != SAMPLE_RATE) {
		rt_log_warning (ERRNO_INVALID_VAL, "    Unexpected sample rate in audio file '%s'", name);
		rt_close_telephony (handle);
		return NULL;
	}
	
	if (info.channels != channels) {
		rt_log_warning (ERRNO_INVALID_VAL, "    Unexpected number of channels in audio file '%s'\n", name);
		rt_close_telephony (handle);
		return NULL;
	}

    return handle;
}



