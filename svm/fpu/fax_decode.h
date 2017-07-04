#ifndef __FPU_DECODE_H__
#define __FPU_DECODE_H__

#include "sndfile.h"
#include "spandsp.h"
#include "stdform.h"

struct fax_context_t {
	t30_state_t	t30_dummy;
	t4_rx_state_t	t4_rx_state;
	int	t4_up;
	hdlc_rx_state_t	hdlcrx;
	int	fast_trained;
	uint8_t ecm_data[256][260];
	int16_t ecm_len[256];
	int	line_encoding;
	int	x_resolution, y_resolution;
	int	image_width;
	int	octets_per_ecm_frame;
	int	error_correcting_mode;
	int	current_fallback;

	fsk_rx_state_t *fsk;
	v17_rx_state_t *v17;
	v29_rx_state_t *v29;
	v27ter_rx_state_t *v27ter;
	
};

extern int fax_decode_alaw (struct fax_context_t *fctx, const char *file_real_path);
extern int fax_context_init (struct fax_context_t *fctx);
extern int fax_converto_alaw (const char *src, const char *alaw_file);
extern void audio_head (const char *desc, void *xhead);

#endif
