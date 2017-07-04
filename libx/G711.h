#ifndef __G711_H__
#define __G711_H__

#include "spandsp.h"

#define SAMPLES_PER_CHUNK           160

extern uint8_t rt_g711_alaw_to_ulaw (uint8_t alaw);

extern uint8_t rt_g711_ulaw_to_alaw (uint8_t ulaw);

extern struct g711_state_s * rt_g711_init (struct g711_state_s *s, int mode);

extern void rt_g711_release (struct g711_state_s *s);

extern int rt_g711_decode (struct g711_state_s *s,
                              int16_t amp[],
                              const uint8_t g711_data[],
                              int g711_bytes);

extern int rt_g711_encode (struct g711_state_s *s,
                              uint8_t g711_data[],
                              const int16_t amp[],
                              int len);

extern int rt_g711_transcode (struct g711_state_s *s,
                                 uint8_t g711_out[],
                                 const uint8_t g711_in[],
                                 int g711_bytes);


extern void G711_test ();

#endif
