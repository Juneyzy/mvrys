#ifndef __VRS_SESSION_H__
#define __VRS_SESSION_H__

#include "vrs.h"

typedef struct
{
	uint64_t    voice_pkts;
	uint64_t    other_pkts;
	uint64_t    lost_pkts;
	uint64_t    over_pkts;
	uint64_t    onlines;
	uint64_t    end_num;
	uint64_t    star_num;
	uint64_t    stop_vdu_sig;
}st_count_t;

extern void vpw_init ();

extern void vpw_startup ();

/** @param packet: struct rt_packet_t */
extern void SGProc(void *packet);

extern void vrs_stat_read(st_count_t *total_count);

extern void vpw_modelist_load(struct vrs_trapper_t *rte, int flags);

extern void vpw_trapper_init (struct vrs_trapper_t *trapper);

#endif

