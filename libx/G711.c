
#include "sysdefs.h"
#include "sndfile.h"
#include "stdform.h"
#include "spandsp.h"
#include "std_audio.h"
#include "G711.h"

uint8_t rt_g711_alaw_to_ulaw (uint8_t alaw)
{
    return alaw_to_ulaw(alaw);
}

uint8_t rt_g711_ulaw_to_alaw (uint8_t ulaw)
{
    return ulaw_to_alaw(ulaw);
}

int rt_g711_decode (struct g711_state_s *s,
                              int16_t amp[],
                              const uint8_t g711_data[],
                              int g711_bytes)
{
	return g711_decode (s, amp, g711_data, g711_bytes);
}

int rt_g711_encode (struct g711_state_s *s,
                              uint8_t g711_data[],
                              const int16_t amp[],
                              int len)
{
	return g711_encode (s, g711_data, amp, len);
}

int rt_g711_transcode (struct g711_state_s *s,
                                 uint8_t g711_out[],
                                 const uint8_t g711_in[],
                                 int g711_bytes)
{
	return g711_transcode (s, g711_out, g711_in, g711_bytes);
}

struct g711_state_s * rt_g711_init (struct g711_state_s *s, int mode)
{
	return g711_init (s, mode);
}

void rt_g711_release (struct g711_state_s *s)
{
	kfree (s);
}

int rt_g711_converto_pcm (const char *alaw_file)
{	
	SNDFILE *handle;
	int16_t amp[SAMPLES_PER_CHUNK];
	uint8_t g711data[SAMPLES_PER_CHUNK];
	char	dst[256] = {0};
	int file, len2 = 0, len3 = 0;
	int outframes;
	struct g711_state_s *g711;
	
	if ((file = open(alaw_file, O_RDONLY)) < 0) {
		fprintf(stderr, "    Failed to open '%s'\n", alaw_file);
		exit(2);
	}
	
	sprintf (dst, "%s.pcm.wav", alaw_file);
	if ((handle = rt_open_telephony_write (dst, 1, SAMPLE_RATE, SF_FORMAT_WAV | SF_FORMAT_PCM_16)) == NULL) {
		fprintf(stderr, "    Cannot create audio file '%s'\n", dst);
		exit(2);
	}

	g711 = rt_g711_init (NULL, G711_ALAW);
	for (;;) {
		
		len2 = read (file, g711data, SAMPLES_PER_CHUNK);
		if (len2 <= 0)
			break;

		len3 = rt_g711_decode (g711, amp, g711data, len2);
		outframes = sf_writef_short (handle, amp, len3);
		if (outframes != len3) {
			fprintf(stderr, "    Error writing audio file\n");
			exit(2);
		}

	}

	if (rt_close_telephony(handle)) {
		fprintf(stderr, "    Cannot close audio file '%s'\n", dst);
		exit(2);
	}
	 
	close (file);
	rt_g711_release (g711);
	
	return 0;
}

int rt_g711_converto_ulaw (const char * alaw_file)
{	
	SNDFILE *handle;
	uint8_t g711_out[SAMPLES_PER_CHUNK];
	uint8_t g711_in[SAMPLES_PER_CHUNK];
	char	dst[256] = {0};
	int file, len2 = 0, len3 = 0;
	int outframes;
	struct g711_state_s *g711;
	
	if ((file = open(alaw_file, O_RDONLY)) < 0) {
		fprintf(stderr, "    Failed to open '%s'\n", alaw_file);
		exit(2);
	}
	
	sprintf (dst, "%s.ulaw.wav", alaw_file);
	if ((handle = rt_open_telephony_write(dst, 1, SAMPLE_RATE, SF_FORMAT_WAV | SF_FORMAT_ULAW)) == NULL) {
		fprintf(stderr, "    Cannot create audio file '%s'\n", dst);
		exit(2);
	}

	g711 = rt_g711_init (NULL, G711_ALAW);
	for (;;) {
		
		len2 = read (file, g711_in, SAMPLES_PER_CHUNK);
		if (len2 <= 0)
			break;

		len3 = rt_g711_transcode (g711, g711_out, g711_in, len2);
		outframes = sf_write_raw (handle, g711_out, len3);
		if (outframes != len3) {
			fprintf(stderr, "    Error writing audio file, (%d, %d)\n", outframes, len3);
			exit(2);
		}

	}

	if (rt_close_telephony (handle)) {
		fprintf(stderr, "    Cannot close audio file '%s'\n", dst);
		exit(2);
	}
	 
	close (file);

	rt_g711_release (g711);
	
	return 0;
}

int rt_g711_encode_xlaw (const char *pcm_file, int type)
{	
	SNDFILE *handle;
	uint8_t g711_in[SAMPLES_PER_CHUNK], g711_out[SAMPLES_PER_CHUNK];
	char	dst[256] = {0};
	int file, len2 = 0, len3 = 0;
	int outframes;
	struct g711_state_s *g711;
	int mode = SF_FORMAT_WAV;
	
	if ((file = open(pcm_file, O_RDONLY)) < 0) {
		fprintf(stderr, "    Failed to open '%s'\n", pcm_file);
		exit(2);
	}

	if (type == G711_ALAW) {
		mode |= SF_FORMAT_ALAW;
		sprintf (dst, "%s.alaw.wav", pcm_file);
	}
	else {
		mode |= SF_FORMAT_ULAW;
		sprintf (dst, "%s.ulaw.wav", pcm_file);
	}
	
	if ((handle = rt_open_telephony_write (dst, 1, SAMPLE_RATE, mode)) == NULL) {
		fprintf(stderr, "    Cannot create audio file '%s'\n", dst);
		exit(2);
	}

	g711 = rt_g711_init (NULL, type);
	for (;;) {
		
		len2 = read (file, g711_in, SAMPLES_PER_CHUNK);
		if (len2 <= 0)
			break;

		len3 = rt_g711_encode (g711, g711_out, (short *)g711_in, len2/2);
		outframes = sf_write_raw (handle, g711_out, len3);
		if (outframes != len3) {
			fprintf(stderr, "    Error writing audio file\n");
			exit(2);
		}

	}

	if (rt_close_telephony (handle)) {
		fprintf(stderr, "    Cannot close audio file '%s'\n", dst);
		exit(2);
	}
	 
	close (file);
	rt_g711_release (g711);
	
	return 0;
}


void G711_test ()
{
#define	ALAW_FILE		"test_data/alaw/alaw.wav"

	rt_g711_converto_pcm (ALAW_FILE);
	rt_g711_converto_ulaw (ALAW_FILE);

#define 	PCM_FILE		"test_data/pcm/pcm.wav"

	rt_g711_encode_xlaw (PCM_FILE, G711_ALAW);
	rt_g711_encode_xlaw (PCM_FILE, G711_ULAW);
}

