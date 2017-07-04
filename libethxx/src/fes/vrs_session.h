#ifndef __RT_FPOOL_H__
#define __RT_FPOOL_H__

#include "rt_atomic.h"
#include "rt_sync.h"
#include "rt_ethxx_packet.h"

#define V_STREAM_UP  1
#define V_STREAM_DWN 2
#define V_SESS_FIN    0x02

#define VOICE_LEN       1024


#define  V_STAGE_FIN (1 << 0)
#define  V_STAGE_1  (1 << 1)
#define  V_STAGE_2  (1 << 2)
#define  V_STAGE_3  (1 << 3)
#define  V_STAGE_X  (1 << 4)

struct call_match_data_t {
    	uint8_t     *data;	/** data pointer */
    	int    	cur_size;   /** payload size of data */
	int		stages, dir;
	struct list_head list;	/** match list */
	union call_link_t	call;
};

struct call_data_t {
    	uint8_t     *data;
	int		bsize;        /** buffer size of data */
    	int    	cur_size;   /** payload size of data */
	int		stages;
};

struct call_session_t {
	union call_link_t	call;
	struct call_data_t	stream[2];	/** upstream(0) & downstream(1) */
	rt_mutex lock;
	int    ttl, ttl_live;
	int	flags;
} ;

#define V_STREAM_DIR_CHK(dir) if(dir != V_STREAM_UP && \
                                                    dir != V_STREAM_DWN)\
                                                        return -1;

extern void sg_init();
extern void SGProc(struct rt_packet_t *packet);

#endif

