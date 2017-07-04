#include "sysdefs.h"
#include "vrs.h"
#include "vrs_rule.h"
#include "vrs_model.h"
#include "vrs_session.h"

static int         stage_time[3] = {3, 6, 9};

typedef struct pkt_head 
{
    unsigned char version;
    unsigned char reply;
    uint16_t cmd;
    uint16_t no;
    uint16_t length;
}PKT_HEAD;

struct stats_t{
	atomic_t sg_frames;
	atomic_t wqe_size, pool_size, pool_pop_counter, pool_push_counter;
	atomic_t alive_cnt, aged_cnt, matching_cnt, matched_cnt;
};

static struct stats_t SGstats = {
    .sg_frames = ATOMIC_INIT(0),
    .wqe_size = ATOMIC_INIT(0),
    .pool_size = ATOMIC_INIT(0),
    .pool_pop_counter = ATOMIC_INIT(0),
    .pool_push_counter = ATOMIC_INIT(0),
    .alive_cnt = ATOMIC_INIT(0),
    .aged_cnt = ATOMIC_INIT(0),
    .matching_cnt = ATOMIC_INIT(0),
    .matched_cnt = ATOMIC_INIT(0),
};

#define V_OFFSET	37
#define V_TMDOUT_INTERVAL	10

/** x: 1, 2, 3*/
#define sizeof_stagex(x)  (VOICE_LEN * 8 * stage_time[(x-1)%3])
#define slotof_stream(x) ((x-1)%2)
#define strof_stream_direction(x) (x == V_STREAM_UP ? "up" : (x == V_STREAM_DWN ? "down" : "unknown"))

LIST_HEAD(online_list);
INIT_MUTEX(online_lock);

LIST_HEAD(matcher_list);
INIT_MUTEX(matcher_lock);

struct vrs_trapper_t{
	struct rt_pool_t *vsess_pool;
	uint32_t tmr;
	int s;

	char *vdu_config;
	struct vdu_t    *vdu;

	char *vpm_config;
	struct vpm_t    *vpm;

	char	*vpw_config;
	struct vpw_t	*vpw;
	
	int (*model_match)(void *voice_data, size_t size);

	uint32_t	perform_tmr;
	
	struct list_head online_list;
};

struct vrs_trapper_t vrsTrapper = {
	.vsess_pool = NULL,
    	.vdu = &vdu,
	.vpm = &vpm,
    	.vpw = &vpw,
};

int __stop_injection(uint8_t *buffer, 
				size_t __attribute__((__unused__))s, uint64_t callid)
{

	int l = 0;
	static uint16_t no;
	PKT_HEAD *head = (PKT_HEAD *)buffer;

	head->version = 1;
	head->reply = 0;
	head->cmd = htons(40);
	head->no = htons(no ++);
	head->length = htons(8 + 4 + 8);
	l += sizeof(PKT_HEAD);

	*(uint16_t *)(buffer + l) = htons(1);
	l += 2;

	*(uint16_t *)(buffer + l) = htons(8);
	l += 2;

	*(uint16_t *)(buffer + l) = htobe64(callid);
	l += 8;

	return l;
};

static inline int vpu_stop_injection(uint64_t callid)
{

	int  s = 0;
	uint8_t buff[1024];
	struct sockaddr_in  sockaddr;
	int socklen = sizeof(struct sockaddr_in);
	struct vdu_t *_vdu;
	int xret = -1;

	_vdu = vrsTrapper.vdu;
	s = __stop_injection(buff, 1024, callid);
	if(_vdu->sock > 0) {
		atomic_inc(&_vdu->stop_times);
		s = sendto(_vdu->sock, buff, s, 0, (struct sockaddr *)&sockaddr, socklen);
		if(s > 0) {
			rt_log_info("stopping call %lu",  callid);
			xret = 0;
		}
	}

	return xret;
}

static inline struct call_data_t *curstream(struct call_session_t *vsess, int dir)
{
	return &vsess->stream[(dir - 1) % 2];
}

static inline int cursize(struct call_data_t *v)
{
	return v->cur_size;
}

static inline int cursize_set(struct call_data_t *v, int s)
{
	return v->cur_size = s;
}

static inline int curstage(struct call_data_t *v)
{
	return v->stages;
}

static inline void curstage_set(struct call_data_t *v,  int stage)
{
	v->stages = stage;
}

static inline void curstage_append(struct call_data_t *v,  int stage)
{
	v->stages |= stage;
}

static inline void payload_append2_memory(uint8_t *val,  int s,
                    struct call_data_t *v)
{
	int cur_size = cursize(v);
	memcpy(v->data + cur_size, (uint8_t *)val, s);
	cursize_set(v,  (cur_size + s));
}

static inline void payload_append2_file(const char *filename, struct call_match_data_t *md)
{
	FILE *fp;
	int  len = 0;

	fp = fopen (filename, "a+");
	if (likely(fp)) {
		len = fwrite(md->data, 1, md->cur_size, fp);
		if (len != md->cur_size) {
			rt_log_warning(ERRNO_WARNING, 
				"(%d ?= %d), %s",  md->cur_size, len, strerror(errno));
		}
		fclose(fp);
	}
}

static inline void __matcher_append(struct call_match_data_t *b)
{
	rt_mutex_lock(&matcher_lock);
	list_add_tail(&b->list, &matcher_list);
	rt_mutex_unlock(&matcher_lock);
}

/**
	Add one stage to Matcher.
*/
static inline void matcher_append(struct call_data_t *_this, union call_frame_snapshot_t *snap, int stage)
{
	struct call_match_data_t *md;
	
	if (likely(_this)) {
		/** need mempool here */
		md = (struct call_match_data_t *)malloc(sizeof(struct call_match_data_t));
		if (likely(md)) {
			INIT_LIST_HEAD(&md->list);
			md->stages = stage;
			md->cur_size = _this->cur_size;
			md->data = _this->data;
			md->call.data = snap->call.data;
			md->dir = snap->dir;
			__matcher_append(md);
			atomic_inc(&SGstats.matching_cnt);
		}
	}
}

static inline void curstage_detect(void *p, union call_frame_snapshot_t *snapshot, struct call_data_t *v)
{
	int stage = 0;

	/** This session ended */
	if(likely(snapshot->mark == V_SESS_FIN)) {
		stage |= V_STAGE_FIN;
		/** Send a 'STOP' signal to VDU */
		vpu_stop_injection(snapshot->call.data);
		matcher_append(v, snapshot, stage);
	}else {
		/** not safety here. */
		payload_append2_memory((uint8_t *)(p + V_OFFSET), snapshot->payload_size, v);
		
		if(cursize(v) == sizeof_stagex(1)) {
			stage |= V_STAGE_1;
			matcher_append(v, snapshot, 1);
		}else {
			if(cursize(v) == sizeof_stagex(2)) {
				stage |= V_STAGE_2;
				matcher_append(v, snapshot, 2);
			}
			else if (cursize(v) >= sizeof_stagex(3)) {
	                	stage |= V_STAGE_3;
				matcher_append(v, snapshot, 3);
			}
		}
	}

	curstage_append(v, stage);
}

static inline void curttl_reset(struct call_session_t *vsess)
{
	vsess->ttl_live = vsess->ttl;
}

static inline int curttl_dec(struct call_session_t *vsess)
{
	return (--vsess->ttl_live);
}

static void *__session_alloc()
{

	struct call_session_t *p = NULL;

	p = (struct call_session_t *)kmalloc(sizeof(struct call_session_t), MPF_CLR, -1);
	if(unlikely(!p))
		goto finish;

	memset64(p, 0, sizeof(struct call_session_t));
	rt_mutex_init(&p->lock, NULL);
	
	p->stream[0].data = kmalloc((VOICE_LEN * 8 * stage_time[2]), MPF_CLR, -1);
	if (unlikely(!p->stream[0].data)) {
		kfree(p);
		p = NULL;
		goto finish;
	}
	memset64(p->stream[0].data, 0, (VOICE_LEN * 8 * stage_time[2]));
	
	p->stream[1].data = kmalloc((VOICE_LEN * 8 * stage_time[2]), MPF_CLR, -1);
	if (unlikely(!p->stream[1].data)) {
		kfree(p->stream[0].data);
		kfree(p);
		p = NULL;
		goto finish;
	}
	memset64(p->stream[1].data, 0, (VOICE_LEN * 8 * stage_time[2]));
	
	p->stream[0].bsize =  (VOICE_LEN * 8 * stage_time[2]);
	p->stream[1].bsize =  (VOICE_LEN * 8 * stage_time[2]);
	atomic_inc(&SGstats.pool_size);
	
finish:
	return p;
}


static void __session_free(void *priv_data)
{

	struct call_session_t *p = (struct call_session_t *)priv_data;

	kfree(p->stream[0].data);
	kfree(p->stream[1].data);
	kfree(p);
}

static inline void session_clone(struct call_session_t *src, 
			struct call_session_t **dst)
{

	struct rt_pool_bucket_t *bucket = NULL;
	struct call_session_t *vs;
	
	bucket = rt_pool_bucket_get_new(vrsTrapper.vsess_pool, NULL);
	if(likely(bucket)){
		vs = (struct call_session_t *)bucket->priv_data;
		INIT_LIST_HEAD(&bucket->list);
		memcpy64(vs, src, sizeof(struct call_session_t));
		*dst =  vs;
	}
}

static inline void link_copy(union call_link_t *dst, const union call_link_t *src)
{
	dst->data = src->data;
}

static inline uint64_t session_callid(struct call_session_t *sess)
{
	return sess->call.data;
}


/** Add a new session to online list */
static inline void online_append(struct rt_pool_bucket_t *b)
{
	rt_mutex_lock(&online_lock);
	list_add_tail(&b->list, &online_list);
	rt_mutex_unlock(&online_lock);
}

/** Online session find */
static inline struct call_session_t * online_find(union call_frame_snapshot_t *snapshot)
{
	struct call_session_t *vsess = NULL, *find = NULL;
	struct rt_pool_bucket_t *_this, *p;
	
	rt_mutex_lock(&online_lock);
	list_for_each_entry_safe(_this, p, &online_list, list) {
		vsess = (struct call_session_t *)_this->priv_data;
		if (likely(vsess)) {
			if (session_callid(vsess) == snapshot->data[0]) {
				find = vsess;
				/**rt_log_debug("[FIND_SESSION]: callid=%llu  [%s, up:%d, down:%d]",
					snapshot->data[0], strof_stream_direction(snapshot->dir), cursize(&vsess->stream[slotof_stream(V_STREAM_UP)]), cursize(&vsess->stream[slotof_stream(V_STREAM_DWN)]));
					*/
				goto finish;
			}
		}
	}
	
finish:
	rt_mutex_unlock(&online_lock);

	return find;
}

static void * SGAger (void *args)
{

	struct vrs_trapper_t *fs = (struct vrs_trapper_t *)args;
	struct rt_pool_t *pool = (struct rt_pool_t *)fs->vsess_pool;
	struct rt_pool_bucket_t *_this, *p;
	struct call_session_t  *vsess;
		
	FOREVER {
		sleep (1);
		rt_mutex_lock(&online_lock);
		list_for_each_entry_safe(_this, p, &online_list, list) {
			vsess = (struct call_session_t *)_this->priv_data;
			if (likely(vsess)) {
				if (curttl_dec(vsess) <= 0) {
					list_del(&_this->list);
					rt_log_debug("[AGING_SESSION]: callid=%lu  [up:%d, down:%d]",
						vsess->call.data, cursize(&vsess->stream[slotof_stream(V_STREAM_UP)]), cursize(&vsess->stream[slotof_stream(V_STREAM_DWN)]));
					rt_pool_bucket_push(pool, _this);
					
					atomic_inc(&SGstats.aged_cnt);
					atomic_dec(&SGstats.alive_cnt);
				}
			}
		}
		rt_mutex_unlock(&online_lock);
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static inline int convert_to_cdr (const char *filename, struct call_match_data_t *_this)
{
	float *score = NULL;
	int m = 0;
	int index = -1;

	_this = _this;
	
	index = modelist_match (filename);
	if (index < 0){
		remove(filename);
		return -1;
	}

	modelist_score((float **)&score, &m);
	
	if ((index < m) &&
		score[index] > 80){

	}
		
	return 0;
}

static inline void * SGMatcher(void __attribute__((__unused__))*args)
{

	struct call_match_data_t *_this, *p;
	char filename[256] = {0};
	
	FOREVER {
		rt_mutex_lock(&matcher_lock);
		list_for_each_entry_safe(_this, p, &matcher_list, list) {
			if(likely(_this)) {
				list_del(&_this->list);
				snprintf(filename, 255, "./vrs/%lu-%d-%d", _this->call.data, _this->dir, _this->stages);
				rt_log_debug("[MATCHER_STAGE_%d] callid=%lu, [\"%s\", %d], %s", 
						_this->stages, _this->call.data, strof_stream_direction(_this->dir), _this->cur_size, filename);
				payload_append2_file (filename, _this);
				atomic_inc(&SGstats.matched_cnt);
				//convert_to_cdr(filename, _this);
				kfree(_this);
			}
		}
		rt_mutex_unlock(&matcher_lock);
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static inline struct call_session_t * __allocate_session(struct rt_packet_snapshot_t *intro)
{
	struct rt_pool_bucket_t *bucket = NULL;
	union call_frame_snapshot_t *snapshot = &intro->call_snapshot;
	struct call_session_t *vsess = NULL;
	
	do{
		/** alloc a new session for current call */
		bucket = rt_pool_bucket_get_new(vrsTrapper.vsess_pool, NULL);
		if(likely(bucket)) {
		    	vsess = (struct call_session_t *)bucket->priv_data;
			if(unlikely(!vsess)) {
				rt_log_warning(ERRNO_NO_ELEMENT, 
					"Can not allocate a voice session for call=%lu, direction=%u\n", snapshot->call.data, snapshot->dir);
				rt_pool_bucket_push(vrsTrapper.vsess_pool, bucket);
				continue;
			}
			
		    	rt_log_debug("[ALLOCATE_SESSION]: callid=%lu  [\"%s\", %d/%d]",
				snapshot->call.data, strof_stream_direction(snapshot->dir), snapshot->payload_size, (int)intro->size);
			
			link_copy(&vsess->call, &snapshot->call);
			vsess->ttl = V_TMDOUT_INTERVAL;

			online_append(bucket);
			atomic_inc(&SGstats.alive_cnt);

		}
	}while(!bucket);

	return vsess;
}

#define curstage_at(v, s) (curstage(v) & s)

void SGProc(struct rt_packet_t *packet)
{

	struct call_session_t *vsess = NULL;
	union call_frame_snapshot_t *snapshot = NULL;
	struct call_data_t	*v;
	struct rt_packet_snapshot_t *intro;
	
	atomic_inc(&SGstats.sg_frames);
	intro = &packet->intro;
	snapshot = &intro->call_snapshot;
	
	/** find by callid from online table. */
	vsess = online_find(snapshot);
	if(unlikely(!vsess)) {
		/**  
			DO NOT alloc session if there's no such call in online table.
			This situation shows that it's a vgrant frame. 
		*/
		if(likely(snapshot->mark == V_SESS_FIN)) {
			rt_log_debug("[**FIN]: \"%s\": %lu",
					strof_stream_direction(snapshot->dir), snapshot->call.data);
			return;
		}
		vsess = __allocate_session(intro);
		/** this case is not necessary. */
		if(unlikely(!vsess)){
			rt_log_warning(ERRNO_MEM_ALLOC, 
					"Can not allocate a voice session for call=%lu, direction=%u\n", snapshot->call.data, snapshot->dir);
			return;
		}
	}

	rt_mutex_lock(&vsess->lock);
	
	curttl_reset(vsess);

	v = curstream(vsess, snapshot->dir);
	if (curstage_at(v, V_STAGE_FIN) ||
		curstage_at(v, V_STAGE_3)) {
		//rt_log_debug("[STAGE_ENOUGH]: callid=%lu, stages=%08x", session_callid(vsess), curstage(v));
		goto finish;
	}
	
	/** append payload and check stages */
	curstage_detect(packet->buffer, snapshot, v);
	
	/** 2016-4-14, What should I do here ? */
	/** Ahh, 2016-4-14, I know what should I do here !!! */
	/** @1. data enough @2. FIN proactively, otherwise, make a aging period */
	if(curstage_at(v, V_STAGE_FIN)) {
		rt_log_debug("[***FIN_SESSION]: callid=%lu  [\"%s\", up:%d, down:%d]",
				session_callid(vsess), strof_stream_direction(snapshot->dir), 
				cursize(&vsess->stream[slotof_stream(V_STREAM_UP)]), cursize(&vsess->stream[slotof_stream(V_STREAM_DWN)]));
	} else {
		if(curstage_at(v, V_STAGE_3)) {
			;
		}else {
			if(curstage_at(v, V_STAGE_2)) {
				;//matcher_append(v);
			} else {
				if(curstage_at(v, V_STAGE_1)) {
					;//matcher_append(v);
				}
			}
		}
	}

finish:
	rt_mutex_unlock(&vsess->lock);
	
}

static void SGScanner(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char __attribute__((__unused__))**argv)
{
		rt_log_debug("SG_frames_%d, alive_%d, aged_%d, matching_%d, matched_%d",
			atomic_add(&SGstats.sg_frames, 0),
			atomic_add(&SGstats.alive_cnt, 0), atomic_add(&SGstats.aged_cnt, 0), atomic_add(&SGstats.matching_cnt, 0), atomic_add(&SGstats.matched_cnt, 0));
}

static struct rt_task_t SGSessionAgingTask = {
    .module = THIS,
    .name = "SG Aging task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGAger ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t SGMatcherTask = {
    .module = THIS,
    .name = "SG Matcher task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGMatcher,
    .recycle = FORBIDDEN,
};

void sg_init()
{
	struct vrs_trapper_t *vTrapper = &vrsTrapper;

	vTrapper->perform_tmr = tmr_create(SG, 
				"Performence Monitor for SG", TMR_PERIODIC,
				SGScanner, 1, (char **)vTrapper, 10);
				
	vTrapper->vsess_pool  = rt_pool_initialize(2048, 
                   		 __session_alloc, __session_free, 0);
	if(likely(vTrapper->vsess_pool)){
		task_registry(&SGSessionAgingTask);
		task_registry(&SGMatcherTask);
		tmr_start(vTrapper->perform_tmr);
	}
}

