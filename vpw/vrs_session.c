#include <json/json.h>
#include "sysdefs.h"
#include "capture.h"
#include "rt_ethxx_packet.h"
#include "vrs_session.h"
#include "vpm_boost.h"
#include "vrs_senior.h"

static struct vpw_t    *current_vpw;

static LIST_HEAD(matcher_list);
static INIT_MUTEX(matcher_lock);



/** Unused now */
//#define    ENABLE_MATCHER
//#define    ENABLE_MATCHER_THRDPOOL
#define    ENABLE_MATCHERS

/** payload offset of each voice frame */
#define V_PL_OFFSET    37

/** Interval for session aging. */
#define V_TMDOUT_TTL    5

#define V_STREAM_UP  1
#define V_STREAM_DWN 2

/** Type of each voice frame. */
#define V_FRAME_TYPE_FIN   0x02
#define V_FRAME_TYPE_DAT  0x03

#define V_FRAME_LENGTH       1024

#define  V_STAGE_FIN (1 << 0)
#define  V_STAGE_1  (1 << 1)
#define  V_STAGE_2  (1 << 2)
#define  V_STAGE_3  (1 << 3)
#define  V_STAGE_X  (1 << 4)

typedef struct pkt_head
{
    unsigned char version;
    unsigned char reply;
    uint16_t cmd;
    uint16_t no;
    uint16_t length;
}PKT_HEAD;


struct call_data_t {
    uint8_t    *data;
    int        bsize;        /** buffer size of data */
    int        cur_size;   /** payload size of data */
    int        stages;
};

struct cm_entry_t {
        uint8_t     *data;    /** data pointer, point to call_data_t->data in CS entry  */
        int        cur_size;   /** payload size of data */
    int        stages, dir, is_case;
    union call_link_t    call;
};

/** call session entry */
struct cs_entry_t {
    union call_link_t    call;
    struct call_data_t    stream[2];    /** upstream(0) & downstream(1) */
    rt_mutex lock;
    int    ttl, ttl_live;
    int    flags;
    uint8_t dir;
    uint8_t case_id;
#ifdef CALL_ID_HASHED
    struct hlist_node call_hlist;
#endif
} ;

/** cdr report entry */
struct cdr_entry_t {
    struct __cdr_t cdr;
};

struct cbp_dir {
    atomic_t sig;
    char    root[256];
    struct tm tms;
};

struct cbp_dir cbp = {
    .sig = ATOMIC_INIT(0),
    .root = {0},
};

struct stats_t{
    atomic_t sg_frames;
    atomic_t wqe_size, pool_size, pool_pop_counter, pool_push_counter;
    atomic_t alive_cnt, aged_cnt, matching_enq, matching_deq, matched_fail, matched_cnt;
    atomic_t cdr_enq_cnt, cdr_deq_cnt, cdr_report_cnt, cdr_real_cnt;
    atomic_t topn_send, counter_send;
    atomic_t session_cnt, hitted_cnt; //用于命中统计上报
};

static struct stats_t SGstats = {
    .sg_frames = ATOMIC_INIT(0),
    .wqe_size = ATOMIC_INIT(0),
    .pool_size = ATOMIC_INIT(0),
    .pool_pop_counter = ATOMIC_INIT(0),
    .pool_push_counter = ATOMIC_INIT(0),
    .alive_cnt = ATOMIC_INIT(0),
    .aged_cnt = ATOMIC_INIT(0),
    .matching_enq = ATOMIC_INIT(0),
    .matching_deq = ATOMIC_INIT(0),
    .matched_fail = ATOMIC_INIT(0),
    .matched_cnt = ATOMIC_INIT(0),
    .cdr_enq_cnt = ATOMIC_INIT(0),
    .cdr_deq_cnt = ATOMIC_INIT(0),
    .cdr_report_cnt = ATOMIC_INIT(0),
    .cdr_real_cnt = ATOMIC_INIT(0),
    .topn_send = ATOMIC_INIT(0),
    .counter_send = ATOMIC_INIT(0),
    .session_cnt = ATOMIC_INIT(0),
    .hitted_cnt = ATOMIC_INIT(0),
};

void vrs_stat_read(st_count_t *total_count)
{
    struct vdu_t *_vdu = NULL;
    struct vrs_trapper_t *cur_trapper = vrs_default_trapper();

    _vdu = cur_trapper->vdu;
    total_count->voice_pkts = atomic_read(&SGstats.sg_frames);
    total_count->other_pkts = 0;
    total_count->lost_pkts = 0;
    total_count->over_pkts = atomic_read(&SGstats.aged_cnt);
    total_count->onlines = atomic_read(&SGstats.alive_cnt);
    total_count->end_num = atomic_read(&SGstats.matching_deq);
    total_count->star_num = atomic_read(&(SGstats.matching_enq));
    total_count->stop_vdu_sig = atomic_read(&_vdu->stop_times);
}

/** x: 1, 2, 3*/
#define sizeof_stagex(x)  (V_FRAME_LENGTH * 8 * atomic_read(&current_vpw->stage_time[(x-1)%3]))
#define secsof_stagex(x)  (atomic_read(&current_vpw->stage_time[(x-1)%3]))
#define slotof_stream(x) ((x-1)%2)
#define strof_stream_direction(x) (x == V_STREAM_UP ? "up" : (x == V_STREAM_DWN ? "down" : "unknown"))

static LIST_HEAD(online_list);
static INIT_MUTEX(online_lock);

#define V_HASH_BUCKETS        1024

static __rt_always_inline__ int __vdu_stop_injection(uint8_t *buffer,
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

static __rt_always_inline__ struct call_data_t *curstream(struct cs_entry_t *vsess, int dir)
{
    return &vsess->stream[(dir - 1) % 2];
}

static __rt_always_inline__ int cursize(struct call_data_t *v)
{
    return v->cur_size;
}

static __rt_always_inline__ int cursize_set(struct call_data_t *v, int s)
{
    return v->cur_size = s;
}

static __rt_always_inline__ int curstage(struct call_data_t *v)
{
    return v->stages;
}
#define curstage_at(v, s) (curstage(v) & s)

static __rt_always_inline__ void curstage_set(struct call_data_t *v,  int stage)
{
    v->stages = stage;
}

static __rt_always_inline__ void curstage_append(struct call_data_t *v,  int stage)
{
    v->stages |= stage;
}

static __rt_always_inline__ void curttl_reset(struct cs_entry_t *vsess)
{
    vsess->ttl_live = vsess->ttl;
}

static __rt_always_inline__ void curttl_init(struct cs_entry_t *vsess)
{
    vsess->ttl = V_TMDOUT_TTL;
    curttl_reset (vsess);
}

static __rt_always_inline__ int curttl_dec(struct cs_entry_t *vsess)
{
    return (--vsess->ttl_live);
}

static __rt_always_inline__ int curttl (struct cs_entry_t *vsess)
{
    return vsess->ttl_live;
}

static __rt_always_inline__ void __cs_entry_data_init(struct cs_entry_t *_this)
{
    int i = 0;
    for (i = 0; i < 2; i ++) {
        _this->stream[i].cur_size = 0;
        _this->stream[i].stages = 0;
    }
}

static void *__cs_entry_alloc()
{
    struct cs_entry_t *p = NULL;

    int s = sizeof_stagex(3);

    p = (struct cs_entry_t *)kmalloc(sizeof(struct cs_entry_t), MPF_CLR, -1);
    if(unlikely(!p))
        goto finish;

    memset64(p, 0, sizeof(struct cs_entry_t));
    rt_mutex_init(&p->lock, NULL);

    p->stream[0].data = kmalloc(s, MPF_CLR, -1);
    if (unlikely(!p->stream[0].data)) {
        kfree(p);
        p = NULL;
        goto finish;
    }

    p->stream[1].data = kmalloc(s, MPF_CLR, -1);
    if (unlikely(!p->stream[1].data)) {
        kfree(p->stream[0].data);
        kfree(p);
        p = NULL;
        goto finish;
    }

    memset64 (p->stream[0].data, 0, s);
    memset64 (p->stream[1].data, 0, s);
    p->stream[0].bsize    =    s;
    p->stream[1].bsize    =    s;
    p->stream[0].cur_size    =    0;
    p->stream[1].cur_size    =    0;


    atomic_inc(&SGstats.pool_size);

finish:
    return p;
}


static void __cs_entry_free(void *priv_data)
{

    struct cs_entry_t *p = (struct cs_entry_t *)priv_data;

    kfree(p->stream[0].data);
    kfree(p->stream[1].data);
    kfree(p);
}


static void *__cm_entry_alloc()
{

    struct cm_entry_t *p = NULL;

    p = (struct cm_entry_t *)kmalloc(sizeof(struct cm_entry_t), MPF_CLR, -1);
    if (likely(p)) {
        memset64(p, 0, sizeof(struct cm_entry_t));
    }

    return p;
}

static void __cm_entry_free(void *priv_data)
{
    struct cm_entry_t *p = (struct cm_entry_t *)priv_data;
    kfree(p);
}

static void *__cdr_entry_alloc()
{

    struct cdr_entry_t *p = NULL;

    p = (struct cdr_entry_t *)kmalloc(sizeof(struct cdr_entry_t), MPF_CLR, -1);
    if(unlikely(!p))
        goto finish;

    memset64(p, 0, sizeof(struct cdr_entry_t));

finish:
    return p;
}

static void __cdr_entry_free(void *priv_data)
{
    struct cdr_entry_t *p = (struct cdr_entry_t *)priv_data;
    kfree(p);
}


static __rt_always_inline__ void link_copy(union call_link_t *dst, const union call_link_t *src)
{
    dst->data = src->data;
}

static __rt_always_inline__ uint64_t cs_callid(struct cs_entry_t *sess)
{
    return sess->call.data;
}

#ifdef CALL_ID_HASHED
static struct hlist_head *cstable_create_hash(void)
{
    int i;
    struct hlist_head *hash;

    hash = (struct hlist_head *)kmalloc(sizeof(*hash) * V_HASH_BUCKETS, MPF_CLR, -1);
    if (hash != NULL)
        for (i = 0; i < V_HASH_BUCKETS; i++)
            INIT_HLIST_HEAD(&hash[i]);

    return hash;
}

static struct cdr_hlist *cdr_create_hash(void)
{
    int i;
    struct cdr_hlist *hash;

    hash = (struct cdr_hlist *)kmalloc(sizeof(*hash) * V_HASH_BUCKETS, MPF_CLR, -1);
    if (hash != NULL){
        for (i = 0; i < V_HASH_BUCKETS; i++){
            INIT_HLIST_HEAD(&(hash[i].head));
            rt_mutex_init(&(hash[i].mtx), NULL);
        }
    }

    return hash;
}


static __rt_always_inline__ struct hlist_head *cst_hash(struct vrs_trapper_t *rte, uint64_t callid)
{

    return &rte->call_hlist_head[callid & (V_HASH_BUCKETS - 1)];
}

static __rt_always_inline__ struct cdr_hlist *cdr_hash(struct vrs_trapper_t *rte, uint64_t callid)
{

    return &(rte->cdr_hlist_head[callid & (V_HASH_BUCKETS - 1)]);
}


static __rt_always_inline__ struct cs_entry_t *cs_get_by_call(struct vrs_trapper_t *rte, uint64_t callid)
{
    struct hlist_node *p, *_this;
    struct cs_entry_t *s;
    struct hlist_head *head = cst_hash(rte, callid);

    hlist_for_each_entry_safe(s, _this, p, head, call_hlist)
        if (s->call.data == callid) {
            return s;
        }

    return NULL;
}

static __rt_always_inline__ struct rt_pool_bucket_t  *cdr_get_by_call(struct vrs_trapper_t *rte, uint64_t callid)
{
    struct hlist_node *p, *_this;
    struct rt_pool_bucket_t *s = NULL;
    struct cdr_entry_t *entry = NULL;
    //struct hlist_head *head = cdr_hash(rte, callid);
    struct cdr_hlist *cdr_head = cdr_hash(rte, callid);

    hlist_for_each_entry_safe(s, _this, p, &cdr_head->head, call_hlist){
        entry = (struct cdr_entry_t *)s->priv_data;
        if (entry->cdr.call.data == callid) {
            rt_mutex_unlock(&cdr_head->mtx);
            return s;
        }
    }

    return NULL;
}


#endif

static __rt_always_inline__ int __sg_check_and_mkdir(const char *root)
{
    char cmd[256] = {0};
    if(!rt_dir_exsit(root)) {
        sprintf(cmd, "mkdir -p %s", root);
        do_system(cmd);
        return 1;
    }

    return 0;
}

static __rt_always_inline__ void sg_tm_convert(uint64_t callid, struct tm *tms)
{
    time_t time;
    time =  (callid >> 32) & 0xffffffff;

    localtime_r(&time, tms);
}

static __rt_always_inline__ void cs_init_entry (struct cs_entry_t *_this, union call_link_t *link)
{
#ifdef CALL_ID_HASHED
    INIT_HLIST_NODE(&_this->call_hlist);
#endif
    rt_mutex_init (&_this->lock, NULL);
    _this->flags = 0;
    link_copy(&_this->call, link);
    curttl_init (_this);
    __cs_entry_data_init (_this);
}

/** Add a new session to online list */
static __rt_always_inline__ void cs_append_alive(struct vrs_trapper_t *rte, struct rt_pool_bucket_t *b)
{
#ifdef CALL_ID_HASHED
    struct cs_entry_t *s;

    s = (struct cs_entry_t *)b->priv_data;
#endif

    rt_mutex_lock(&online_lock);
    list_add_tail(&b->list, &online_list);
#ifdef CALL_ID_HASHED
    hlist_add_head(&s->call_hlist, cst_hash(rte, s->call.data));
#endif
    rt_mutex_unlock(&online_lock);
}


/** Online session find */
static __rt_always_inline__ struct cs_entry_t * cs_find_alive(struct vrs_trapper_t *rte, union vrs_fsnapshot_t *snapshot)
{
    struct cs_entry_t *find = NULL;

#ifdef CALL_ID_HASHED
    rt_mutex_lock(&online_lock);
    find = cs_get_by_call(rte, snapshot->data[0]);
    goto finish;
#else
    struct cs_entry_t *vsess = NULL;
    struct rt_pool_bucket_t *_this, *p;
    rt_mutex_lock(&online_lock);
    list_for_each_entry_safe(_this, p, &online_list, list) {
        vsess = (struct cs_entry_t *)_this->priv_data;
        if (likely(vsess)) {
            if (cs_callid(vsess) == snapshot->data[0]) {
                find = vsess;
                goto finish;
            }
        }
    }
#endif
finish:
    rt_mutex_unlock(&online_lock);

    return find;
}

static __rt_always_inline__ struct cs_entry_t * cs_allocate_from_pool(struct vrs_trapper_t *rte, struct rt_packet_snapshot_t *intro)
{
    struct rt_pool_bucket_t *bucket = NULL;
    union vrs_fsnapshot_t *snapshot = &intro->call_snapshot;
    struct cs_entry_t *vsess = NULL;

    /** alloc a new session for current call */
    bucket = rt_pool_bucket_get_new(rte->cs_bucket_pool, NULL);
    if (likely(bucket)) {
            vsess = (struct cs_entry_t *)bucket->priv_data;
        if(unlikely(!vsess)) {
            rt_pool_bucket_push(rte->cs_bucket_pool, bucket);
            goto finish;
        }
        cs_init_entry (vsess, &snapshot->call);
        vsess->dir = snapshot->dir;
        vsess->case_id = snapshot->case_id;
        cs_append_alive(rte, bucket);
        atomic_inc(&SGstats.alive_cnt);
        atomic_inc(&SGstats.session_cnt);//用于命中统计上报WEB，每200s会上报一次，然后重置为0
    }

finish:
    return vsess;
}

static __rt_always_inline__ void cdr_allocate_from_pool(struct hlist_node *call_hlist, struct cdr_hlist *cdr_head)
{

    INIT_HLIST_NODE(call_hlist);
    hlist_add_head(call_hlist, &cdr_head->head);
}

static __rt_always_inline__ void call_link_copy (union call_link_t *dst, union call_link_t *src)
{
    dst->data = src->data;
}

static __rt_always_inline__ int sock_dgram_init(char *ip, int port)
{
    struct sockaddr_in  vdu_addr;

    memset(&vdu_addr , 0, sizeof(vdu_addr));

    vdu_addr.sin_family = AF_INET;
    vdu_addr.sin_port = htons(port);
    vdu_addr.sin_addr.s_addr = inet_addr(ip);

    return socket(AF_INET, SOCK_DGRAM, 0);
}

static __rt_always_inline__ int sg_detector_stop_injection(uint64_t callid, uint32_t mark)
{

    int  s = 0;
    uint8_t buff[1024];
    struct sockaddr_in  sockaddr;
    struct vdu_t *_vdu;
    int xerror = -1;
    struct vrs_trapper_t *cur_trapper = vrs_default_trapper();

    _vdu = cur_trapper->vdu;
    memset(&sockaddr , 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(_vdu->port);
    sockaddr.sin_addr.s_addr = inet_addr(_vdu->ip);
    int socklen = sizeof(struct sockaddr_in);

    if (_vdu->sock < 0) {
        _vdu->sock = sock_dgram_init(_vdu->ip, _vdu->port);
        rt_log_info ("Connecting to vdu(%s:%d, sock=%d) ... %s",
                    _vdu->ip, _vdu->port, _vdu->sock, _vdu->sock > 0 ? "success" : "failure");
    }

    s = __vdu_stop_injection(buff, 1024, callid);
    if (_vdu->sock > 0) {
        s = sendto (_vdu->sock, buff, s, 0, (struct sockaddr *)&sockaddr, socklen);
        if(s > 0) {
            rt_log_info ("Stopping call %lu, cause = %s",  callid, mark == V_FRAME_TYPE_FIN ? "Session finished" : "stage finish");
            xerror = 0;
            atomic_inc(&_vdu->stop_times);
        }
    }

    return xerror;
}


static __rt_always_inline__ void sg_detector_append_payload(uint8_t *val,  int s,
                    struct call_data_t *v)
{
    int cur_size = cursize(v);
    memcpy64(v->data + cur_size, (uint8_t *)val, s);
    cursize_set(v,  (cur_size + s));
}

static __rt_always_inline__ struct vrs_matcher_t    *sg_detector_allocate_a_matcher (struct vrs_trapper_t *rte,
                        uint64_t callid)
{
    return &rte->sg_matchers[callid % (rte->matchers)];
}

static __rt_always_inline__ void sg_matcher_init_md_entry (struct cm_entry_t *md)
{
    md->data = NULL;
    md->call.data = md->cur_size = 0;
    md->dir = md->is_case= md->stages = INVALID_VID;
}


/**
    Add one stage to Matcher.
*/
static __rt_always_inline__ void sg_detector_append_curstage_entry(struct vrs_trapper_t *rte,
                    struct call_data_t *_this, union vrs_fsnapshot_t *snap, int stage)
{
    struct cm_entry_t *md;

    if (likely(_this)) {
        struct rt_pool_bucket_t *bucket;
        bucket = rt_pool_bucket_get_new(rte->cm_bucket_pool, NULL);
        if (likely(bucket)) {
            rt_log_debug ("__________%p, stage=%d, cursize=%d", bucket, stage, cursize (_this));
            md = (struct cm_entry_t *)bucket->priv_data;
            if (likely(md)) {
                sg_matcher_init_md_entry(md);

                md->stages        =    stage;
                md->cur_size        =    cursize (_this);
                md->data        =    _this->data;
                md->call.data        =    snap->call.data;
                md->dir            =    snap->dir;
                md->is_case        =    snap->case_id;

                struct vrs_matcher_t *matcher = sg_detector_allocate_a_matcher (rte, md->call.data);
                rt_pool_bucket_push(&rte->cm_msg_bucket_pool[matcher->matcher_id], bucket);
                atomic_inc(&SGstats.matching_enq);
            }
        }
    }
}

static __rt_always_inline__ void sg_detector_decide_curstage(struct vrs_trapper_t *rte,
                        void *p, union vrs_fsnapshot_t *snapshot, struct call_data_t *v)
{
    int stage = 0;

    /** This session ended */
    if (likely (snapshot->mark == V_FRAME_TYPE_FIN)) {
        stage |= V_STAGE_FIN;
        /** Send a 'STOP' signal to VDU */
        sg_detector_stop_injection (snapshot->call.data, snapshot->mark);
        //sg_detector_append_curstage_entry (rte, v, snapshot, stage);
    }else {
        /** not safety here. */
        sg_detector_append_payload ((uint8_t *)(p + V_PL_OFFSET), snapshot->payload_size, v);

        if (cursize(v) == sizeof_stagex(1)) {
            stage |= V_STAGE_1;
            sg_detector_append_curstage_entry (rte, v, snapshot, 1);
        }else {
            if (cursize(v) == sizeof_stagex(2)) {
                stage |= V_STAGE_2;
                sg_detector_append_curstage_entry (rte, v, snapshot, 2);
            }
            else if (cursize(v) >= sizeof_stagex(3)) {
                stage |= V_STAGE_3;
                sg_detector_stop_injection (snapshot->call.data, snapshot->mark);
                sg_detector_append_curstage_entry (rte, v, snapshot, 3);
            }
        }
    }

    curstage_append(v, stage);
}

static __rt_always_inline__ void sg_matcher_init_owner_entry (struct owner_t *owner)
{
    memset ((void *)owner, 0, sizeof (struct owner_t));
    memset ((void *)&owner->cmt[0], INVALID_CLUE, sizeof (int) * MAX_CLUES_PER_TARGET);
    owner->cmt_size = 0;
}

static __rt_always_inline__ int sg_matcher_filtrate_score(struct owner_t *owner, struct vrmt_t *_vrmt)
{
    int i = 0, j = 0, xerror = -1;
    struct clue_t *clue;
    struct vrs_trapper_t *rte;
    struct vpw_t *vpw = NULL;

    rte =  vrs_default_trapper();
    vpw = rte->vpw;

    if (atomic_read(&vpw->clue_layer_filter)) {

        vrmt_for_each_clue (i, _vrmt) {
            clue = &_vrmt->clue[i];
            if (valid_clue(clue->id) &&
                owner->sg_score >= clue->score) {
                owner->cmt[j ++] = clue->id;
            }
            if (valid_clue(clue->id) &&
                owner->sg_score < clue->score){
                rt_log_debug("Match Score is: %f, Clue id: %d, Clue score: %d", owner->sg_score, clue->id, clue->score);
            }
        }

        owner->cmt_size = j;
        if(j > 0) {
            xerror = 0;
            goto finish;
        }
    }
    else {
        /** The score greater than threshold of vrmt->score(60 default, for current Target) */
        if (owner->sg_score > _vrmt->target_score) {
            xerror = 0;
            goto finish;
        }
    }

finish:
    return xerror;
}

static __rt_always_inline__ int sg_matcher_delete_passed_massive_model (const char  *root_path, struct tm *tms, int passed_days)
{
    DIR  *pDir;
    struct dirent *ent;
    uint64_t    tm_cur, tm_end, tm_start;
    char cmd[256] = {0};
    char cur_dir[256] ={0};

    if (likely(root_path)) {

        snprintf(cur_dir, 255, "%04d-%02d-%02d", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday);
        rt_str2tms(cur_dir, EVAL_TM_STYLE, &tm_end);
        tm_start = tm_end - 3600 * 24 * passed_days;

        pDir = opendir(root_path);
        if (unlikely(!pDir)) {
            rt_log_error(ERRNO_FATAL,
                        "%s, %s", strerror(errno), root_path);
            return -1;
        }
        while ((ent = readdir(pDir)) != NULL) {
            if(!strcmp(ent->d_name,".") ||
                    !strcmp(ent->d_name,".."))
                continue;
            rt_str2tms(ent->d_name, EVAL_TM_STYLE, &tm_cur);

            if (tm_cur < tm_start) {
                snprintf (cmd, 255, "rm -rf  %s/%s", root_path, ent->d_name);
                //do_system (cmd);
                //rt_log_notice ("mkdir=%s, rm dirent %s of %d days ago", cur_dir, ent->d_name, passed_days);
            }
        }

        closedir(pDir);
    }

    return 0;

}

static __rt_always_inline__ int rt_file_cp(char *mass_path, char *model)
{
#define LINE_LENTH 1024
    FILE *fp,*fp2;
    char *mass_model;
    char line[LINE_LENTH] = {0};

    if((mass_model = strrchr(mass_path,  '/')) == NULL)
        goto finish;

    fp = fopen(mass_path, "r");
    if(!fp){
        rt_log_error(ERRNO_INVALID_ARGU,
                    "Open mass %s fail, error = %s", mass_path, strerror(errno));
        goto finish;
    }

    STRCAT(model, mass_model);
    fp2 = fopen(model,  "w+");
    if(!fp2){
        rt_log_error(ERRNO_INVALID_ARGU,
                    "Open model %s fail, error = %s", model, strerror(errno));
        fclose(fp);
        goto finish;
    }

    while (fgets(line, LINE_LENTH -1,  fp))
    {
        BUG_ON(rt_file_write(fp2,  (void *)line, LINE_LENTH -1, NULL) == EOF);
            clear_memory(line, LINE_LENTH);
    }

    fclose(fp);
    fclose(fp2);

finish:
    return 1;
}

static __rt_always_inline__ int sg_matcher_save_curstage_cased_model(const char *root, const char *case_dir,
                struct tm *tms, char IN *massive_model)
{
    char  case_path[256] = {0};
    snprintf (case_path, 256, "%s/%s/%04d-%02d-%02d", root, case_dir, tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday);
    __sg_check_and_mkdir (case_path);

    rt_file_cp(massive_model,case_path);
    return 0;
}

static __rt_always_inline__ int sg_matcher_save_curstage_model(const char *root, const char *massive_dir,
                struct tm *tms, struct cm_entry_t *_this, const char *cur_wave_file, char OUT *model)
{

    char  normal_dir[256] = {0};
    int wlen1, wlen2;

    snprintf(normal_dir, 256, "%s/%s/%04d-%02d-%02d", root, massive_dir, tms->tm_year + 1900, tms->tm_mon+1, tms->tm_mday);
    if (__sg_check_and_mkdir (normal_dir) == 1) {
        memcpy (&cbp.tms, tms, sizeof (struct tm));
        sprintf (cbp.root, "%s/%s", root, massive_dir);
        atomic_inc (&cbp.sig);
    }

    if(_this->dir == V_STREAM_UP)
        sprintf(model, "%s/%lx_up.model", normal_dir, _this->call.data);
    else
        sprintf(model, "%s/%lx_down.model", normal_dir, _this->call.data);

    wlen1 = wlen2 = 0;
    return WavToModel(cur_wave_file, model, &wlen1, &wlen2, W_ALAW);
}

static __rt_always_inline__ int sg_matcher_lookup_target (struct owner_t *owner, /** struct vrmt_t **_vrmt */ struct vrmt_t *_vrmt)
{
    /** return vrmt_query (owner->tid, &owner->vrmt_index, _vrmt); */
    return vrmt_query_copyout (owner->tid, &owner->vrmt_index, _vrmt);
}

static __rt_always_inline__ int sg_matcher_save_curstage_dat(const char *filename, struct cm_entry_t *md)
{
    FILE *fp;
    int  len = 0;
    int ret = 0;

    ret = (-ERRNO_NO_ELEMENT);
    fp = fopen (filename, "a+");
    if (likely(fp)) {
        len = fwrite(md->data, 1, md->cur_size, fp);
        if (len == md->cur_size)
            ret = XSUCCESS;
        else {
            ret = (-ERRNO_WARNING);
            rt_log_warning(ERRNO_WARNING,
                "(%d ?= %d), %s",  md->cur_size, len, strerror(errno));
        }

        fclose(fp);
    }

    return ret;
}

static __rt_always_inline__ void sg_matcher_init_cdr_entry (struct __cdr_t *_this)
{
    memset (_this->up_ruleid, INVALID_CLUE, sizeof(uint64_t) * MAX_CLUES_PER_TARGET);
    memset (_this->down_ruleid, INVALID_CLUE, sizeof(uint64_t) * MAX_CLUES_PER_TARGET);
    _this->up_userid = _this->down_userid = INVALID_TARGET;
    _this->up_voiceid = _this->down_voiceid = INVALID_VID;
    _this->up_rules_num = _this->down_rules_num = 0;
    _this->up_percent = _this->down_percent = 0;
    _this->stage = 0;
    _this->flags = 0;
}

static __rt_always_inline__ int sg_matcher_append_match_entry(struct __cdr_t *cdr, struct owner_t *owner,
                                                               struct vrmt_t *_vrmt, int dir)
{
    int i = 0, result = 1;

    if (sg_matcher_lookup_target (owner, _vrmt) < 0){
        result = -1;
        goto finish;
    }

    if (sg_matcher_filtrate_score(owner, _vrmt) < 0){
        result = -1;
        goto finish;
    }

    if (dir == V_STREAM_UP){
        cdr->up_percent     =    (int)owner->sg_score;
        cdr->up_userid      =    _vrmt->id;
        cdr->up_rules_num   =    owner->cmt_size;
        cdr->up_voiceid     =    owner->vid;

        vrmt_for_each_clue (i, _vrmt) {
            if (valid_clue(owner->cmt[i])) {
                cdr->up_ruleid[i] = owner->cmt[i];
                rt_log_debug ("Hit up clue_id=%lu", cdr->up_ruleid[i]);
            } else
                break;
        }

        rt_log_debug ("Hits=%d, upstream, score=%d", cdr->up_rules_num, cdr->up_percent);
    }

    else {

        cdr->down_percent        =    (int)owner->sg_score;
        cdr->down_userid            =    _vrmt->id;
        cdr->down_rules_num        =    owner->cmt_size;
        cdr->down_voiceid            =    owner->vid;

        vrmt_for_each_clue (i, _vrmt) {
            if (valid_clue(owner->cmt[i])) {
                cdr->down_ruleid[i] = owner->cmt[i];
                rt_log_debug ("Hit down clue_id=%lu", cdr->down_ruleid[i]);
            } else
                break;
        }

        rt_log_debug ("Hits=%d, downstream, score=%d", cdr->down_rules_num, cdr->down_percent);
    }

finish:
    return result;
}


/****************************************************************************
函数名称  : sg_local_topn_match
函数功能    : 指定wav和本地topn匹配
输入参数    : msg,本地topn消息体;cnt,本地topn中消息体个数;wav,要比较的wav语音
输出参数    : 无
返回值     : -1错误;0正常
创建人     : wangkai
备注      :
****************************************************************************/
static int
sg_local_topn_match(struct owner_t *owner, IN topn_msg_t *msg, int cnt, IN const char *wav, uint64_t callid)
{

    int xret = -1, i, index = 0, m;
    float aver = 0, sum = 0;
    float *sg_score = NULL;
    struct modelist_t *cur_modelist = NULL;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    char model_name[256] = {0};
    struct tm tms;
    time_t time;

    if (NULL == owner || NULL == msg || NULL == wav) {
        rt_log_error(ERRNO_FATAL, "null pointer");
        return -1;
    }

    cur_modelist = modelist_create(cnt);
    for (i=0; i<cnt; i++) {
        time =  (msg[i].callid >> 32) & 0xffffffff;
        localtime_r(&time, &tms);
        snprintf(model_name, sizeof(model_name), "%s/%s/%04d-%02d-%02d/%lx_%s.model",
            rte->vdu_dir, "normal", tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
            msg[i].callid, (msg[i].dir==V_STREAM_UP) ? "up" : "down");
        if (rt_file_exsit(model_name)) {
            rt_log_debug("Load %s", model_name);
            modelist_load_one_model(model_name, msg[i].callid, cur_modelist);
        } else {
            rt_log_error(ERRNO_FATAL, "file: %s not exist", model_name);
            /* 待优化 */
        }
    }

    if (likely (cur_modelist) && cur_modelist->sg_cur_size > 0) {
        m = cur_modelist->sg_cur_size;
        sg_score = cur_modelist->sg_score;
        xret = rte->tool->voice_recognition_advanced_ops(
                wav, cur_modelist, sg_score, &index, W_ALAW);
        if (0 == xret && index < m) {
            for (i=0; i<m; i++) {
                   sum += sg_score[i];
            }
            aver = sum / m;
            rt_log_debug("tid = %lu,callid=%lx, sg_score: %f hit scd aver: %f, valid topn: %d", owner->tid, callid, owner->sg_score, aver, m);
            if ((int)aver >= rte->hit_scd_conf.hit_score) {
                xret = 0;
                goto finish;
            }
        }
        rt_log_notice("tid=%lu,callid=%lx, score: %f, aver: %f, topns: %d, second not hit", owner->tid, callid,
                owner->sg_score, aver, m);
    } else {
        rt_log_error(ERRNO_SG, "callid=%lx, modelist load error", callid);
    }
    xret = -1;

finish:
    modelist_destroy(cur_modelist);
    return xret;
}

static __rt_always_inline__ void message_clone (char *ijstr, size_t l, void **clone)
{
    *clone = kmalloc (l + 1, MPF_CLR, -1);
    if (*clone) {
        memcpy64 (*clone, ijstr, l);
    }
}

static int sg_local_topn_process(struct owner_t *owner,
        struct vrs_trapper_t *rte, struct cm_entry_t * _this)
{
    int cnt = 0, min_inx = 0, xret = -1;
    topn_msg_t *msg_arr = NULL;
    struct tlv request = {0};
    topn_msg_t msg = {0};
    message clone = NULL;

    msg_arr = kmalloc(sizeof(topn_msg_t) * rte->hit_scd_conf.topn_max, MPF_CLR, -1);
    if (unlikely(NULL == msg_arr))
    {
        rt_log_error(ERRNO_MEM_ALLOC, "kmalloc");
        return -ERRNO_MEM_ALLOC;
    }

    xret = senior_topn_get(owner->tid, msg_arr, &cnt, &min_inx);
    if (xret < 0) {
        rt_log_error(ERRNO_FATAL, "get topn ret = %d", xret);
        kfree(msg_arr);
        return xret;
    }

    if (  (xret==0) && (cnt<=rte->hit_scd_conf.topn_max) &&
            owner->sg_score >= rte->hit_scd_conf.update_score ) {
        if ( (uint32_t)owner->sg_score > msg_arr[min_inx].score ) { // 比本地TOPN最小的大
            msg.callid = _this->call.data;
            msg.score = (uint32_t)owner->sg_score;
            msg.dir   = _this->dir;
            rt_log_notice("owner->sg_score: %f, local topn score: %d",
                    owner->sg_score, msg_arr[min_inx].score);
            senior_topn_query_to_req(owner->tid, &msg, &request);
            rt_log_debug("tlv->t=%d, l=%d, %s", request.t, request.l, request.v);
            message_clone((void *)&request, request.l+TLV_FRAME_HEAD_LEN, &clone);
            if (MQ_SUCCESS == rt_mq_send (rte->vpm_mq, clone, request.l+TLV_FRAME_HEAD_LEN)) {
                atomic_inc(&SGstats.topn_send);
            } else {
                kfree(clone);
            }
        }
    }
    kfree(msg_arr);

    return 0;
}


static __rt_always_inline__ int __sg_matcher_append_cdr_entry(struct vrs_trapper_t *rte,
                       struct cm_entry_t * _this,  struct owner_t *owner,
                       struct vrmt_t *_vrmt, char *wav, int *first_hit)
{
    int result = 1, xret = -1;
    struct cdr_entry_t *entry = NULL;
    struct rt_pool_bucket_t *bucket = NULL;
    struct __cdr_t *cdr = NULL;

    /*stages 1 is not need to report*/
    if (_this->stages == 1){
        return result;
    }
    bucket = rt_pool_bucket_get_new(rte->cdr_bucket_pool, NULL);
    if (likely (bucket)) {
        entry = (struct cdr_entry_t *)bucket->priv_data;
        if (likely(entry)) {
            cdr = (struct __cdr_t *)&entry->cdr;

            sg_matcher_init_cdr_entry (&entry->cdr);
            call_link_copy (&entry->cdr.call, &_this->call);
            cdr->stage = _this->stages;

            *first_hit = result = sg_matcher_append_match_entry(cdr, owner, _vrmt, _this->dir);
            // 如果第一次命中了，并且二次命中开启
            if ( (1== result) && rte->hit_scd_conf.hit_second_en ) {
               int cnt = 0, min_inx = 0;
               topn_msg_t *msg_arr = (topn_msg_t *)kmalloc(sizeof(topn_msg_t) * rte->hit_scd_conf.topn_max,
                        MPF_CLR, -1);
               if (unlikely(NULL == msg_arr))
               {
                   rt_log_error(ERRNO_MEMBLK_ALLOC, "MALLOC");
                   return 0;
               }
               xret = senior_topn_get(owner->tid, msg_arr, &cnt, &min_inx);
               if (cnt >= rte->hit_scd_conf.topn_min) {
                   xret = sg_local_topn_match(owner, msg_arr, cnt, wav, _this->call.data); // 本地topn匹配
                   if (xret < 0) {
                       rt_pool_bucket_push ((struct rt_pool_t *)rte->cdr_bucket_pool, bucket);
                       kfree(msg_arr);
                       return 0;  // return 1, 二次匹配命中.这里返回0，表明没有命中。
                   }
               }
               kfree(msg_arr);
            }

            /*为了实现高级筛选功能，需要将命中语音的方向存入到数据库中，现将其放入到flags字段的*/
            cdr->flags = _this->dir;

            if (cdr_flags_chk_bit(CDR_FLGS_CONN_BIT) && (cdr->up_percent != 0 || cdr->down_percent != 0)){
                if (MQ_SUCCESS != rt_mq_send (rte->cdr_mq, (void *)bucket, (int)sizeof(struct cdr_entry_t ))) {
                    rt_pool_bucket_push (rte->cdr_bucket_pool, bucket);
                }
                else{
                    atomic_inc(&SGstats.cdr_enq_cnt);
                }
            }
            else{
               rt_pool_bucket_push ((struct rt_pool_t *)rte->cdr_bucket_pool, bucket);
            }
        }
    }

   return result;
}

static __rt_always_inline__ int sg_matcher_append_cdr_entry(struct vrs_trapper_t *rte,
                        struct cm_entry_t * _this,  struct owner_t *owner,
                        struct vrmt_t *_vrmt)
{
    int result = 1;
    struct cdr_entry_t *entry = NULL;
    struct rt_pool_bucket_t *bucket = NULL;
    struct __cdr_t *cdr = NULL;
    struct cdr_hlist *cdr_head = cdr_hash(rte, _this->call.data);

    rt_mutex_lock(&cdr_head->mtx);

    /** find by callid from online table. */
    bucket = cdr_get_by_call(rte, _this->call.data);
    if (unlikely (!bucket)) {
        bucket = rt_pool_bucket_get_new(rte->cdr_bucket_pool, NULL);
        if (likely (bucket)) {
            entry = (struct cdr_entry_t *)bucket->priv_data;

            if (likely(entry)) {

                cdr = (struct __cdr_t *)&entry->cdr;

                sg_matcher_init_cdr_entry (&entry->cdr);

                call_link_copy (&entry->cdr.call, &_this->call);
                cdr->stage = _this->stages;

                cdr_allocate_from_pool(&bucket->call_hlist, cdr_hash(rte, _this->call.data));
            }
        }

        result = sg_matcher_append_match_entry(cdr, owner, _vrmt, _this->dir);

        goto finish;
    }

    else{
        entry = (struct cdr_entry_t *)bucket->priv_data;

        if (likely(entry)) {

            cdr = (struct __cdr_t *)&entry->cdr;

            result = sg_matcher_append_match_entry(cdr, owner, _vrmt, _this->dir);
            rt_log_debug("sg_matcher_append_match_entry %s", 1==result ? "ok" : "error");

            //cdr->flags = 1;原来的这个地方根本就没有使用
            /*为了实现高级筛选功能，需要将命中语音的方向存入到数据库中，现将其放入到flags字段的*/
            cdr->flags = _this->dir;

            if (cdr_flags_chk_bit(CDR_FLGS_CONN_BIT) && (cdr->up_percent != 0 || cdr->down_percent != 0)){
                if (MQ_SUCCESS != rt_mq_send (rte->cdr_mq, (void *)bucket, (int)sizeof(struct cdr_entry_t ))) {
                    hlist_del (&bucket->call_hlist);
                    rt_pool_bucket_push (rte->cdr_bucket_pool, bucket);
                }else
                    atomic_inc(&SGstats.cdr_enq_cnt);
            }else{
                hlist_del (&bucket->call_hlist);
                rt_pool_bucket_push ((struct rt_pool_t *)rte->cdr_bucket_pool, bucket);
            }
            goto finish;
        }
    }

finish:
    rt_mutex_unlock(&cdr_head->mtx);
    return result;
}

static __rt_always_inline__ void sg_reporter_logging_entry (struct cdr_entry_t *entry, int okay /** Okay, Failure*/, char *log_dir)
{

#define BUF_SIZE        2048
#define CID_SIZE        256

    struct __cdr_t *_this;
    char tm[64] = {0}, tm_ymd[64] = {0};
    int  lup = 0, ldown = 0, i = 0, cl = 0, cnt = 0;
    char  log_up[BUF_SIZE] = {0}, log_down[BUF_SIZE] = {0}, up_clue_id[CID_SIZE] = {0}, down_clue_id[CID_SIZE] = {0};
    char  log_file[CID_SIZE] = {0};
    FILE *fp = NULL;

    rt_curr_tms2str(EVAL_TM_STYLE_FULL, tm, 63);
    /** %Y-%m-%d, 2016-05-14 */
    memcpy(tm_ymd, tm, 10);

    _this = &entry->cdr;
    if (_this->up_rules_num > 0) {
        cl =  0;
        cnt = _this->up_rules_num;

        cl += SNPRINTF (up_clue_id + cl, CID_SIZE - 1 - cl, "(");
        vrmt_for_each_clue (i, NULL) {

            if (valid_clue(_this->up_ruleid[i])) {
                    cl += SNPRINTF (up_clue_id + cl, CID_SIZE - 1 - cl, "%lu", _this->up_ruleid[i]);
                    cl += SNPRINTF (up_clue_id + cl, CID_SIZE - 1 - cl, "%s", --cnt ? "," : ")");
            }
        }

        lup += SNPRINTF (log_up + lup, BUF_SIZE -1 - lup, "%20s%100s%20lu%6s%2d%3lu%4d%6s\n",
                                        tm, up_clue_id, _this->call.data, "UP", _this->stage, _this->up_voiceid, _this->up_percent, okay ? "SNDOK" : "SNDER");

        SNPRINTF(log_file, CID_SIZE - 1, "%s/%lu-CDRs-%s.log", log_dir, _this->up_userid, tm_ymd);

        rt_file_open(log_file, &fp);
        if (fp != NULL){
            BUG_ON(rt_file_write(fp, (void *)log_up, lup, NULL) == EOF);
            fclose(fp);
        }
    }

    if (_this->down_rules_num > 0) {
        cl =  0;
        cnt = _this->down_rules_num;

        cl += SNPRINTF (down_clue_id + cl, CID_SIZE - 1 - cl, "(");

        vrmt_for_each_clue (i, NULL) {

            if (valid_clue(_this->down_ruleid[i])) {
                cl += SNPRINTF (down_clue_id + cl, CID_SIZE - 1 - cl, "%lu", _this->down_ruleid[i]);
                cl += SNPRINTF (down_clue_id + cl, CID_SIZE - 1 - cl, "%s", --cnt ? "," : ")");
            }
        }

        ldown += SNPRINTF (log_down + ldown, BUF_SIZE -1 - ldown, "%20s%100s%20lu%6s%2d%3lu%4d%6s\n",
                                        tm, down_clue_id, _this->call.data, "DOWN", _this->stage, _this->down_voiceid, _this->down_percent, okay ? "SNDOK" : "SNDER");

        SNPRINTF(log_file, CID_SIZE - 1, "%s/%lu-CDRs-%s.log", log_dir, _this->down_userid, tm_ymd);
        rt_file_open(log_file, &fp);
        if (fp != NULL){
            BUG_ON(rt_file_write(fp, (void *)log_down, ldown, NULL) == EOF);
            fclose(fp);
        }
    }

}

static __rt_always_inline__ size_t sg_reporter_entry_sendto_cdr (struct vrs_trapper_t *rte,
                                                        struct cdr_t *cdr, struct cdr_entry_t *entry)
{
    size_t s = 0;
    size_t scale_size = sizeof (struct __cdr_t);

    if (cdr->sock > 0) {
        s = rt_sock_send (cdr->sock, (void *)&entry->cdr, scale_size);
        if (s == scale_size ) {
            atomic_inc(&SGstats.cdr_report_cnt);
            if (entry->cdr.stage == 3)
            {
                atomic_inc(&SGstats.cdr_real_cnt); /** 只统计第三阶段命中个数 */
                atomic_inc(&SGstats.hitted_cnt);//用于命中统计上报WEB，每200s会上报一次，然后重置为0
            }
            sg_reporter_logging_entry(entry, (s == scale_size), rte->log_dir);
        }
    }
    return s;
}

static __rt_always_inline__ void sg_modelist_init(struct modelist_t    *sg_modelist, void *modelist)
{
    struct modelist_t *_this = (struct modelist_t *)modelist;
    sg_modelist->flags = _this->flags;
    sg_modelist->sg_cur_size = _this->sg_cur_size;
    sg_modelist->sg_max_size = _this->sg_max_size;
    memcpy64(sg_modelist->sg_score, _this->sg_score, SG_MODEL_THRESHOLD);
    memcpy64(sg_modelist->sg_data, _this->sg_data, SG_MODEL_THRESHOLD);
    memcpy64(sg_modelist->sg_owner, _this->sg_owner, SG_MODEL_THRESHOLD);
}

static __rt_always_inline__ void sg_modelist_clone2_matcher (struct vrs_trapper_t *rte)
{
    struct rt_vrstool_t *tool = rte->tool;
    struct vrs_matcher_t *matcher;
    int    i;

    for (i = 0; i < rte->matchers; i ++) {

        matcher = &rte->sg_matchers[i];
        if (unlikely (!matcher->sg_modelist))
            matcher->sg_modelist = modelist_create (SG_MODEL_THRESHOLD);
        if (likely (matcher->sg_modelist))
            sg_modelist_init(matcher->sg_modelist, tool->modelist);
        rt_log_notice ("Clone2 matcher%d", matcher->matcher_id);
    }
}

static __rt_always_inline__ void sg_modelist_load (struct vrs_trapper_t *rte, int flags)
{
    struct rt_vrstool_t *tool = rte->tool;

    vpw_matcher_disable ();

    /** startup */
    if (unlikely (!tool->modelist))
        tool->modelist = modelist_create (SG_MODEL_THRESHOLD);

    /** runtime */
    if (likely (tool->modelist)) {

        modelist_load_by_user (rte->model_dir, flags, tool->modelist,
                &tool->sg_valid_models, &tool->sg_valid_models_size, &tool->sg_valid_models_total, &tool->sg_models_total, 0);

        sg_modelist_clone2_matcher (rte);
    }

    vpw_matcher_enable ();

    return;
}

void vpw_modelist_load (struct vrs_trapper_t *rte, int flags)
{
    sg_modelist_load(rte, flags);
    return;
}

/** */
static __rt_always_inline__ int SGDoMatchingLocally (struct vrs_matcher_t *matcher, const char *wfile, struct owner_t *owner)
{
    int md_index = 0;
    int xerror = -1, m = 0;
    float *s = NULL;
    char **so = NULL;
    uint64_t    begin, end;
    struct modelist_t *cur_modelist;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    struct rt_vrstool_t *tool = rte->tool;

    if (!rt_file_exsit(wfile))
        return xerror;

    if (unlikely (!matcher))
        return xerror;

    begin = rt_time_ms ();

    cur_modelist = matcher->sg_modelist;
    if (likely (cur_modelist) &&
        cur_modelist->sg_cur_size > 0) {
        s = cur_modelist->sg_score;
        m = cur_modelist->sg_cur_size;
        so = cur_modelist->sg_owner;

        xerror = tool->voice_recognition_ops (wfile, cur_modelist, &md_index, W_ALAW);
        if (xerror == 0){
            rt_log_debug("Get Modelist of Match score(%f) index(%d) model(%s)",
                        s[md_index], md_index, so[md_index]);
            if (md_index < m){
                owner->sg_score = s[md_index];
                owner->sg_index = md_index;
                sscanf (so[md_index], "%lu-%d", &owner->tid, &owner->vid);
            }
        }

#if 0
        if ((xerror == 0) &&
            (md_index < m)) {
                owner->sg_score = s[md_index];
                owner->sg_index = md_index;
                sscanf (so[md_index], "%lu-%d", &owner->tid, &owner->vid);
        }
#endif
        if (xerror == (-4)){
            rt_log_info("(%s)Effective speech length is less than 30 s\n", wfile);
        }
    }

    end = rt_time_ms ();

    rt_log_debug ("* %lu-%d matched %s, Costs=%lf sec(s)",
                        owner->tid, owner->vid, xerror ? "failure" : "success", (double)(end - begin) / 1000);
    return xerror;
}

static void *    SGMatcher (void __attribute__((__unused__))*args)
{
    struct rt_pool_bucket_t *__this;
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    struct cm_entry_t *_this;
    struct vrmt_t    *_vrmt, vrmt;
    struct tm tms;
    struct owner_t owner;
    struct vrs_matcher_t    *matcher;
    int xerror, xret = 0, first_hit = 0;
    const char *root = rte->vdu_dir;
    char  massive_model[256] = {0}, wave_dat[256] = {0};

    _vrmt = &vrmt;

    matcher = (struct vrs_matcher_t *)args;
    if (unlikely (!matcher))
        goto task_finish;

    FOREVER {
        /** Recv from internal queue */
        if ((__this = rt_pool_bucket_get(&rte->cm_msg_bucket_pool[matcher->matcher_id])) != NULL)
        {
            atomic_inc(&SGstats.matching_deq);
            _this = (struct cm_entry_t *)__this->priv_data;
            if (unlikely (!_this))
            {
                rt_log_error(ERRNO_FATAL, "callid=%lu _this null", _this->call.data);
                goto finish;
            }
                sg_matcher_init_owner_entry (&owner);

                snprintf(wave_dat, 255, "%s/%lu-%d-%d", rte->tmp_dir, _this->call.data, _this->dir, _this->stages);

                rt_log_debug ("Preparing Matching  \"%s\", at stage %d, case=%d",
                                wave_dat, _this->stages, _this->is_case);

                if (sg_matcher_save_curstage_dat (wave_dat, _this) < 0)
                    goto finish;

                if (SGDoMatchingLocally (matcher, wave_dat, &owner) < 0)
                {
                    rt_log_error(ERRNO_FATAL, "callid=%lu match failed", _this->call.data);
                    atomic_inc(&SGstats.matched_fail);
                    goto wav_del;
                }

                first_hit = 0;
                xret = __sg_matcher_append_cdr_entry (rte, _this, &owner, _vrmt, wave_dat, &first_hit);    /** To CDR */

                atomic_inc(&SGstats.matched_cnt);


                if (_this->stages == 3) {
                        sg_tm_convert (_this->call.data, &tms);
                        xerror = sg_matcher_save_curstage_model (root, "normal", &tms, _this, wave_dat, massive_model);
                        if (xerror == 0) {
                            rt_log_notice ("[Matcher_%d] Saving massive model \"%s\", \"%s\" in stage %d, case=%d",
                                matcher->matcher_id, wave_dat, massive_model, _this->stages, _this->is_case);
                            if (rte->hit_scd_conf.hit_second_en && (1 == first_hit)) {
                                sg_local_topn_process(&owner, rte, _this);
                            }
                            if (_this->is_case && xret ) {
                                xerror = sg_matcher_save_curstage_cased_model (root, "case", &tms, massive_model);
                                if (xerror < 0)
                                    rt_log_error (ERRNO_SG,
                                        "[Matcher_%d] Saveing cased model in stage %d, result=%d", matcher->matcher_id, _this->stages, xerror);
                            }
                        } else {
                            if (rt_file_exsit(massive_model))
                                remove (massive_model);
                            rt_log_error (ERRNO_SG,
                                    "[Matcher_%d] Saving massive model \"%s\", result=%d", matcher->matcher_id, massive_model, xerror);
                        }
                }
        wav_del:
                if (rt_file_exsit(wave_dat)) {
                    rt_log_debug ("Deleting wave data \"%s\"", wave_dat);
                    remove (wave_dat);
                }

        finish:
                rt_pool_bucket_push (rte->cm_bucket_pool, __this);
        }else
            usleep(1000);
    }

task_finish:

    task_deregistry_id (pthread_self());

    return NULL;
}


static __rt_always_inline__ void init_matchers (struct vrs_trapper_t *rte)
{
    int    i;
    struct rt_task_t    *task;
    struct vrs_matcher_t    *matcher;

    rte->sg_matchers = (struct vrs_matcher_t *) kmalloc (sizeof (struct vrs_matcher_t) * rte->matchers, MPF_CLR, -1);
    if (unlikely (!rte->sg_matchers))
        return;

    for (i = 0; i < rte->matchers; i ++) {
         {
             matcher = &rte->sg_matchers[i];

            INIT_LIST_HEAD (&matcher->sg_list_head);
            rt_mutex_init (&matcher->sg_list_lock, NULL);
            matcher->matcher_id = i;
            matcher->sg_modelist = NULL;
            rt_mutex_init(&(rte->cm_msg_bucket_pool[i].m), NULL);

            task    =    (struct rt_task_t    *) kmalloc (sizeof (struct rt_task_t), MPF_CLR, -1);
            if (likely (task)) {
                sprintf (task->name, "SG Matcher%d Task", i);
                task->module = THIS;
                task->core = INVALID_CORE;
                task->prio = KERNEL_SCHED;
                task->argvs = (void *)matcher;
                task->recycle = ALLOWED;
                task->routine = SGMatcher;

                task_spawn_quickly (task);
            }
        }
    }

}


/** */
static __rt_always_inline__ int SGDoMatching (const char *wfile, struct owner_t *owner)
{
    int md_index = 0;
    int xerror = -1, m = 0;
    float *s = NULL;
    char **so = NULL;
    uint64_t    begin, end;
    struct modelist_t *cur_modelist;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    struct rt_vrstool_t *tool = rte->tool;

    if (!rt_file_exsit(wfile))
        return xerror;

    begin = rt_time_ms ();

rt_mutex_lock (&tool->modelist_lock);
    cur_modelist = tool->modelist;
    if (likely (cur_modelist) &&
        cur_modelist->sg_cur_size > 0) {
        s = cur_modelist->sg_score;
        m = cur_modelist->sg_cur_size;
        so = cur_modelist->sg_owner;

        xerror = tool->voice_recognition_ops (wfile, cur_modelist, &md_index, W_ALAW);
        if ((xerror == 0) &&
            (md_index < m)) {
                owner->sg_score = s[md_index];
                owner->sg_index = md_index;
                sscanf (so[md_index], "%lu-%d", &owner->tid, &owner->vid);
        }
    }
rt_mutex_unlock (&tool->modelist_lock);

    end = rt_time_ms ();

    rt_log_debug ("* %lu-%d matched %s, Costs=%lf sec(s)",
                        owner->tid, owner->vid, xerror ? "failure" : "success", (double)(end - begin) / 1000);
    return xerror;
}


#if defined(ENABLE_MATCHER_THRDPOOL)
static void * SGThrdPoolMatcher(void *args)
{

    struct rt_pool_bucket_t *__this = (struct rt_pool_bucket_t *)args;
    struct cm_entry_t *_this;
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    struct vrmt_t    *_vrmt, vrmt;
    struct tm tms;
    struct owner_t owner;
    int xerror;
    const char *root = rte->vdu_dir;
    char  massive_model[256] = {0}, wave_dat[256] = {0};

    _vrmt = &vrmt;

    atomic_inc(&SGstats.matching_deq);

    _this = (struct cm_entry_t *)__this->priv_data;
    if (unlikely (!_this))
        goto finish;

    sg_matcher_init_owner_entry (&owner);

    snprintf(wave_dat, 255, "%s/%lu-%d-%d", rte->tmp_dir, _this->call.data, _this->dir, _this->stages);

    rt_log_debug ("Preparing Matching  \"%s\", at stage %d, case=%d",
                    wave_dat, _this->stages, _this->is_case);

    if (sg_matcher_save_curstage_dat (wave_dat, _this) < 0)
        goto finish;

    if (SGDoMatching (wave_dat, &owner) < 0)
        goto wav_del;

    if (sg_matcher_lookup_target (&owner, _vrmt) < 0)
        goto wav_del;

    if (sg_matcher_filtrate_score (&owner, _vrmt) < 0)
        goto wav_del;
    else
        sg_matcher_append_cdr_entry (rte, _this, &owner, _vrmt);    /** To CDR */

    atomic_inc(&SGstats.matched_cnt);


    if (_this->stages == 3) {
            sg_tm_convert (_this->call.data, &tms);
            xerror = sg_matcher_save_curstage_model (root, "normal", &tms, _this, wave_dat, massive_model);
            if (xerror == 0) {
                rt_log_notice ("Saving massive model \"%s\", \"%s\" in stage %d, case=%d",
                    wave_dat, massive_model, _this->stages, _this->is_case);
                if (_this->is_case) {
                    xerror = sg_matcher_save_curstage_cased_model (root, "case", &tms, massive_model);
                    if (xerror < 0)
                        rt_log_error (ERRNO_SG,
                            "Saveing cased model in stage %d, result=%d", _this->stages, xerror);
                }
            } else {
                if (rt_file_exsit(massive_model))
                    remove (massive_model);
                rt_log_error (ERRNO_SG,
                        "Saving massive model \"%s\", result=%d", massive_model, xerror);
            }
    }
wav_del:
    if (rt_file_exsit(wave_dat)) {
        rt_log_debug ("Deleting wave data \"%s\"", wave_dat);
        remove (wave_dat);
    }

finish:
    rt_pool_bucket_push (rte->cm_bucket_pool, __this);

    return NULL;
}
#endif

static void *    SGMatcherLoader (void __attribute__((__unused__))*args)
{
    struct rt_pool_bucket_t *__this, *p;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

#if defined (ENABLE_MATCHERS)
    init_matchers (rte);
    sg_modelist_load (rte, ML_FLG_RLD|ML_FLG_MOD_0);
    task_deregistry_id (pthread_self());
    return NULL;
#endif

#if defined (ENABLE_MATCHER)
    struct cm_entry_t *_this;
    struct vrmt_t    *_vrmt, vrmt;
    struct tm tms;
    struct owner_t owner;
    int xerror;
    const char *root = rte->vdu_dir;
    char  massive_model[256] = {0}, wave_dat[256] = {0};

    _vrmt = &vrmt;

#endif

    FOREVER {

        rt_mutex_lock (&matcher_lock);
        list_for_each_entry_safe (__this, p, &matcher_list, list) {

                list_del (&__this->list);

#if defined(ENABLE_MATCHER_THRDPOOL)
                threadpool_add (rte->thrdpool_for_matcher,  (void *)SGThrdPoolMatcher, (void *)__this, 0);
#endif

#if defined (ENABLE_MATCHER)
                atomic_inc(&SGstats.matching_deq);

                _this = (struct cm_entry_t *)__this->priv_data;
                if (unlikely (!_this))
                    goto finish;

                sg_matcher_init_owner_entry (&owner);

                snprintf(wave_dat, 255, "%s/%lu-%d-%d", rte->tmp_dir, _this->call.data, _this->dir, _this->stages);

                rt_log_debug ("Preparing Matching  \"%s\", at stage %d, case=%d",
                                wave_dat, _this->stages, _this->is_case);

                if (sg_matcher_save_curstage_dat (wave_dat, _this) < 0)
                    goto finish;

                if (SGDoMatching (wave_dat, &owner) < 0)
                    goto wav_del;

                if (sg_matcher_lookup_target (&owner, _vrmt) < 0)
                    goto wav_del;

                if (sg_matcher_filtrate_score (&owner, _vrmt) < 0)
                    goto wav_del;
                else
                    sg_matcher_append_cdr_entry (rte, _this, &owner, _vrmt);    /** To CDR */

                atomic_inc(&SGstats.matched_cnt);


                if (_this->stages == 3) {
                         sg_tm_convert (_this->call.data, &tms);
                        xerror = sg_matcher_save_curstage_model (root, "normal", &tms, _this, wave_dat, massive_model);
                        if (xerror == 0) {
                            rt_log_notice ("Saving massive model \"%s\", \"%s\" in stage %d, case=%d",
                                wave_dat, massive_model, _this->stages, _this->is_case);
                            if (_this->is_case) {
                                xerror = sg_matcher_save_curstage_cased_model (root, "case", &tms, massive_model);
                                if (xerror < 0)
                                    rt_log_error (ERRNO_SG,
                                        "Saveing cased model in stage %d, result=%d", _this->stages, xerror);
                            }
                        } else {
                            if (rt_file_exsit(massive_model))
                                remove (massive_model);
                            rt_log_error (ERRNO_SG,
                                    "Saving massive model \"%s\", result=%d", massive_model, xerror);
                        }
                }
        wav_del:
                if (rt_file_exsit(wave_dat)) {
                    rt_log_debug ("Deleting wave data \"%s\"", wave_dat);
                    remove (wave_dat);
                }

        finish:
                rt_pool_bucket_push (rte->cm_bucket_pool, __this);
#endif
        }
        rt_mutex_unlock (&matcher_lock);
    }

    task_deregistry_id (pthread_self());

    return NULL;
}


void     SGProc(void *p)
{
    struct rt_packet_t *packet = NULL;
    struct cs_entry_t *vsess = NULL;
    union vrs_fsnapshot_t *snapshot = NULL;
    struct call_data_t    *v;
    struct rt_packet_snapshot_t *intro;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    atomic_inc (&SGstats.sg_frames);

    packet    =    (struct rt_packet_t *)p;
    intro        =    &packet->intro;
    snapshot    =    &intro->call_snapshot;

    /** find by callid from online table. */
    vsess = cs_find_alive (rte, snapshot);
    if (unlikely (!vsess)) {
        /** DO NOT alloc session if there's no such call in online table.
            This situation shows that it's a vgrant frame. */
        if (likely(snapshot->mark == V_FRAME_TYPE_FIN)) {
            rt_log_debug("[**FIN]: \"%s\": %lu",
                    strof_stream_direction (snapshot->dir), snapshot->call.data);
            return;
        }
        vsess = cs_allocate_from_pool (rte, intro);
        /** this case is not necessary. */
        if (unlikely (!vsess)) {
            rt_log_warning (ERRNO_MEM_ALLOC,
                    "Can not allocate a voice session for call=%lu, direction=%u\n", snapshot->call.data, snapshot->dir);
            return;
        }
        rt_log_notice ("[ALLOCATING] callid=%lu  [\"%s\", %d/%d]",
                snapshot->call.data, strof_stream_direction(snapshot->dir), snapshot->payload_size, (int)intro->size);
    }

    rt_mutex_lock (&vsess->lock);

    v = curstream (vsess, snapshot->dir);

#if 0
    if (curstage_at (v, V_STAGE_FIN) ||
        curstage_at (v, V_STAGE_3)) {
        rt_log_debug ("[STAGE_ENOUGH]: callid=%lu, stages=%08x", cs_callid(vsess), curstage(v));
        goto finish;
    }
#else

    if (curstage_at (v, V_STAGE_FIN)) {
        curttl_reset (vsess);
        rt_log_debug ("[STAGE_FIN]: callid=%lu, has already finished.", cs_callid(vsess));
        goto finish;
    }

    /** To avoid stop signal is not actived. */
    if (curstage_at (v, V_STAGE_3)) {
        curttl_reset (vsess);
        rt_log_debug ("[STAGE_ENOUGH]: callid=%lu, has already arrive to stage3(%08x)", cs_callid(vsess), curstage(v));
        goto finish;
    }

#endif

    curttl_reset (vsess);

    /** append payload and check stages */
    sg_detector_decide_curstage (rte, packet->buffer, snapshot, v);

    /** 2016-4-14, What should I do here ? */
    /** Ahh, 2016-4-14, I know what should I do here !!! */
    /** @1. data enough @2. FIN proactively, otherwise, make a aging period */
    if (curstage_at (v, V_STAGE_FIN)) {
        rt_log_debug ("[***FIN_SESSION]: callid=%lu  [\"%s\", up:%d, down:%d]",
                cs_callid (vsess), strof_stream_direction (snapshot->dir),
                cursize(&vsess->stream[slotof_stream(V_STREAM_UP)]),
                cursize(&vsess->stream[slotof_stream(V_STREAM_DWN)]));
    }

finish:
    rt_mutex_unlock (&vsess->lock);

}

static void * SGAger (void __attribute__((__unused__)) *args)
{
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    struct rt_pool_t *pool = (struct rt_pool_t *)rte->cs_bucket_pool;
    struct rt_pool_bucket_t *_this, *p;
    struct cs_entry_t  *vsess;
    struct call_data_t    *v;
    union vrs_fsnapshot_t snap;
    int i = 0;

    FOREVER {

        sleep (2);
#if 1
        rt_mutex_lock (&online_lock);

        list_for_each_entry_safe (_this, p, &online_list, list) {
            vsess = (struct cs_entry_t *)_this->priv_data;
            if (likely(vsess)) {
                rt_mutex_lock (&vsess->lock);
                if (curttl_dec (vsess) <= 0) {
                    for (i = 1; i <= 2; i ++){
                        v = curstream (vsess, i);

                        if (cursize(v) <= sizeof_stagex(1)){
                            if (curstage_at(v, V_STAGE_FIN))
                                rt_log_warning (ERRNO_WARNING,
                                        "Call(%lu) time <= %d secs.", vsess->call.data, secsof_stagex(1));
                            else
                                rt_log_warning (ERRNO_WARNING,
                                        "Call(%lu) timeout.", vsess->call.data);
                        }

                        else if (!curstage_at(v, V_STAGE_3)){

                            snap.call.data = vsess->call.data;
                            snap.dir = i;
                            snap.case_id = vsess->case_id;

                            sg_detector_append_curstage_entry(rte, v, &snap, 3);
                            /*
                             * 此时会话的数据内容在匹配器中还会被用到，故会话需要再存在一段时间，
                             * 不应该老化，重置ttl后直接go out
                             */
                            curstage_append(v, V_STAGE_3);
                            curttl_reset (vsess);
                            goto LABLE;
                        }
                    }
                    list_del (&_this->list); /** Delete from online list */
#ifdef CALL_ID_HASHED
                    hlist_del (&vsess->call_hlist);    /** Delete from hlist */
#endif
                    rt_pool_bucket_push (pool, _this);
                    rt_log_notice ("[AGING]: callid=%lu  [up:%d, down:%d]",
                            vsess->call.data, cursize(&vsess->stream[slotof_stream(V_STREAM_UP)]), cursize(&vsess->stream[slotof_stream(V_STREAM_DWN)]));
                    atomic_inc (&SGstats.aged_cnt);
                    atomic_dec (&SGstats.alive_cnt);
                }
            LABLE:
                rt_mutex_unlock (&vsess->lock);
            }
        }

        rt_mutex_unlock (&online_lock);
#endif
    }

    task_deregistry_id (pthread_self());

    return NULL;
}

static void vpw_send_statistics_to_vpm(char *tm, int matching_enq, int cdr_report_cnt)
{
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    struct tlv request = {0};
    message clone = NULL;

    memset(&request, 0, sizeof(request));
    snprintf(request.v, 1023, "%u, %s, %d, %d", rte->vpw->id, tm, matching_enq, cdr_report_cnt);

    request.t = req_update_stat;
    request.l = strlen(request.v) + STR_OVER_CHAR_LEN;
    rt_log_debug("tlv t(%d), l(%d), v(statistics : %s)", request.t, request.l, request.v);

    message_clone((void *)&request, request.l+TLV_FRAME_HEAD_LEN, &clone);
    if (MQ_SUCCESS == rt_mq_send (rte->vpm_mq, clone, request.l+TLV_FRAME_HEAD_LEN))
    {
        atomic_inc(&SGstats.counter_send);
    }
    else
    {
        kfree(clone);
    }
}

static void SGVPWTmrStat(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char __attribute__((__unused__))**argv)
{
    char tm[64] = {0};

    rt_curr_tms2str(EVAL_TM_STYLE_FULL, tm, 63);

    /** 命中统计结果上报 */
    vpw_send_statistics_to_vpm(tm, 2 * atomic_add(&SGstats.session_cnt, 0), atomic_add(&SGstats.hitted_cnt, 0));

    /** 统计清零，即每次只上报增量 */
    atomic_set(&SGstats.session_cnt, 0);
    atomic_set(&SGstats.hitted_cnt, 0);
}

static void SGVpwTmrScanner(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char __attribute__((__unused__))**argv)
{

#define    GATHER_INFO_SIZE    1024
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    char    gather[GATHER_INFO_SIZE]  = {0};
    int l = 0;
    char tm[64] = {0};//tm_ymd[64] = {0};

    if (atomic_add(&cbp.sig, 0)) {
        sg_matcher_delete_passed_massive_model (cbp.root, &cbp.tms, 90);
        atomic_dec(&cbp.sig);
    }

    rt_logging_reinit_file (rte->log_dir);

    rt_curr_tms2str(EVAL_TM_STYLE_FULL, tm, 63);
    l += snprintf (gather + l, GATHER_INFO_SIZE -l,
        "%s     SGFrames=%d, Alived Session=%d, Aged Session=%d\n",
            tm, atomic_add(&SGstats.sg_frames, 0), atomic_add(&SGstats.alive_cnt, 0), atomic_add(&SGstats.aged_cnt, 0));

    l += snprintf (gather + l, GATHER_INFO_SIZE -l,
        "\tMatcher  (enqueue=%d, dequeue=%d, matched=%d, failed=%d)\n",
            atomic_add(&SGstats.matching_enq, 0), atomic_add(&SGstats.matching_deq, 0), atomic_add(&SGstats.matched_cnt, 0), atomic_add(&SGstats.matched_fail, 0));

    l += snprintf (gather + l, GATHER_INFO_SIZE -l,
        "\tReporter (enqueue=%d, dequeue=%d, reported=%d, 3rdStage=%d)\n",
            atomic_add(&SGstats.cdr_enq_cnt, 0), atomic_add(&SGstats.cdr_deq_cnt, 0), atomic_add(&SGstats.cdr_report_cnt, 0), atomic_add(&SGstats.cdr_real_cnt, 0));

    rt_log_notice ("%s", gather);
}

static void *     SGReporter (void __attribute__((__unused__)) *args)
{

    struct vrs_trapper_t *rte = vrs_default_trapper ();
    struct rt_pool_t *pool;
    struct rt_pool_bucket_t *_this = NULL;
    struct cdr_entry_t  *entry;
    size_t s = 0;
    const size_t scale_size = sizeof (struct __cdr_t);
    struct vpw_t *vpw;
    struct cdr_t *cdr;
    message    data = NULL;
    //struct cdr_hlist *cdr_head = NULL;

    vpw = rte->vpw;
    pool = (struct rt_pool_t *)rte->cdr_bucket_pool;
    cdr = &vpw->cdr;

    FOREVER {

        do {

            cdr->sock = rt_clnt_sock (0, cdr->ip, cdr->port, AF_INET);
            if (cdr->sock > 0) {
                cdr_flags_set_bit (CDR_FLGS_CONN_BIT, 1);
                break;
            }

            rt_log_notice ("Connecting to CDR (%s:%d, sock=%d): %s (%d)",
                        cdr->ip, cdr->port, cdr->sock, "failure", cdr_flags_chk_bit (CDR_FLGS_CONN_BIT));
            sleep (3);

        } while (cdr->sock < 0);


        rt_log_notice ("Connecting to CDR (%s:%d, sock=%d): %s (%d)",
                        cdr->ip, cdr->port, cdr->sock, "success", cdr_flags_chk_bit (CDR_FLGS_CONN_BIT));

        do {
            data = NULL;
            /** Recv from internal queue */
            rt_mq_recv (rte->cdr_mq, &data, (int *)&s);
            if (likely (data)){
                atomic_inc (&SGstats.cdr_deq_cnt);
                _this = (struct rt_pool_bucket_t *)data;
                entry = (struct cdr_entry_t *)_this->priv_data;

                //cdr_head = cdr_hash(rte, entry->cdr.call.data);
                //rt_mutex_lock(&cdr_head->mtx);
                //hlist_del (&_this->call_hlist);
                //rt_mutex_unlock(&cdr_head->mtx);

                if (likely (entry)) {
                    s  = sg_reporter_entry_sendto_cdr (rte, cdr, entry);
                    if (s != scale_size) {
                        cdr_flags_set_bit (CDR_FLGS_CONN_BIT, 0);
                        rt_sock_close(&cdr->sock, NULL); /** socket close or peer error */
                    }
                }
                rt_pool_bucket_push (pool, _this);
            }

        } while (cdr->sock > 0);
    }

    task_deregistry_id(pthread_self());

    return NULL;
}

#define    MSG_SIZE    1024


/****************************************************************************
 函数名称  : sg_update_model_throushold
 函数功能    : 更新Model, 如果有新阈值接着更新阈值
 输入参数    : recvb, vpm传过来的原始数据包; sz, 数据包的大小
 输出参数    : 无
 返回值     : XSUCCESS, 正确; 其他, 异常
 创建人     : wangkai
 备注      :
****************************************************************************/
static int sg_update_model_throushold(IN char *recvb, int sz)
{
    int vid, tid_index, xerror;
    target_id  tid;
    struct tlv request = {0};
    struct vrmt_t *_vrmt;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

    tid = INVALID_TARGET;
    vid = INVALID_VID;

    // 获取TID
    xerror = senior_parse_vpm_vpw_request(recvb, sz, req_update_mod, &request);
    if (xerror != XSUCCESS)
        return xerror;
    sscanf (request.v, "%lu-%d.%*s", &tid, &vid);
    // 获取Threshold
    boost_threshold_t bt = {0};
    memset(&request, 0, sizeof(request));
    xerror = senior_parse_vpm_vpw_request(recvb, sz, req_update_bt, &request);
    rt_log_debug("request: %s", request.v);
    if (XSUCCESS == xerror) {
        xerror = request_str_to_bt(&request, &bt);
        if (0 == xerror) {
            rt_log_notice("tid: %lu accurate:%d exploring:%d default:%d", tid, bt.accurate_score,
                 bt.exploring_score, bt.default_score);
        } else {
            rt_log_error(ERRNO_FATAL, "request_str_to_bt");
        }
    } else {
        rt_log_notice("parse_vpm_vpw_request ERROR");
    }

    if (!vrmt_query (tid, &tid_index, (struct vrmt_t **)&_vrmt)) {
        sg_modelist_load (rte, ML_FLG_RLD|ML_FLG_MOD_0);
        vrmt_load(VRS_RULE_CFG, 0, rte->thr_path_name);
        rt_log_notice ("(tid=%lu, vid=%d) Reloading Model Database from \"%s\" ... finished(%ld)",
            tid, vid, rte->model_dir, rte->tool->sg_valid_models);
    }

    return xerror;
}


/****************************************************************************
 函数名称  : sg_rm_topn
 函数功能    : 删除topn信息(包括磁盘和内存)
 输入参数    : tid, 要删除信息的tid
 输出参数    : 无
 返回值     : XSUCCESS, 正确; 其他, 异常
 创建人     : wangkai
 备注      :
****************************************************************************/
static int sg_rm_topn(struct vrs_trapper_t *rte, uint64_t tid)
{
    return senior_rm_topn_cell_and_disc(rte, tid);
}

/****************************************************************************
 函数名称  : sg_vpm_remove_target
 函数功能    : 处理vpm删除目标的时候操作
 输入参数    : recvb, vpm传过来的原始数据包; sz, 数据包的大小
 输出参数    : 无
 返回值     : XSUCCESS, 正确; 其他, 异常
 创建人     : wangkai
 备注      :
****************************************************************************/
static int sg_remove_target(IN char *recvb, int sz)
{
    int vid, tid_index, xerror;
    target_id  tid;
    struct tlv request = {0};
    struct vrmt_t *_vrmt;
    struct vrs_trapper_t *rte = vrs_default_trapper ();

    tid = INVALID_TARGET;
    vid = INVALID_VID;

    xerror = senior_parse_vpm_vpw_request(recvb, sz, req_remove_target, &request);
    if (xerror != XSUCCESS)
    {
        rt_log_error(xerror, "parse req_remove_target");
        return xerror;
    }

    if (2 != sscanf (request.v, "%lu-%d.%*s", &tid, &vid))
    {
        rt_log_error(ERRNO_INVALID_VAL, "parse tid vid");
        return -ERRNO_INVALID_VAL;
    }

    sg_rm_topn(rte, tid);

    if (!vrmt_query (tid, &tid_index, (struct vrmt_t **)&_vrmt))
    {
        sg_modelist_load (rte, ML_FLG_RLD|ML_FLG_MOD_0);
        rt_log_notice ("(tid=%lu, vid=%d) Reloading Model Database from \"%s\" ... finished(%ld)",
            tid, vid, rte->model_dir, rte->tool->sg_valid_models);
    }

    return xerror;
}

static int sg_update_topn(char *recvb, int sz)
{
    struct tlv request = {0};
    int xerror;
    topn_msg_t *msg = NULL;
    int topns = 0;
    uint64_t tid = 0;

    msg = (topn_msg_t *)kmalloc(sizeof(topn_msg_t) * vrs_default_trapper()->hit_scd_conf.topn_max, MPF_CLR, -1);
    if (unlikely(NULL == msg))
    {
        rt_log_error(ERRNO_MEM_ALLOC, "%s", strerror(errno));
        return -ERRNO_MEM_ALLOC;
    }
    memset(&request, 0, sizeof(request));
    xerror = senior_parse_vpm_vpw_request(recvb, sz, req_update_topn, &request);
    if (0 == xerror) {
        rt_log_notice("sg_update_topn.1");
        xerror = senior_req_upd_topn_to_topnmsg(&request, &tid, msg, &topns);
        if (0 == xerror && topns <= vrs_default_trapper()->hit_scd_conf.topn_max) {
            rt_log_notice("sg_update_topn.2");
            senior_update_topn_fulltext(tid, msg, topns);
        }
    }
    kfree(msg);
    msg = NULL;
    return 0;
}

static void SGVpmRequest(char *recvb, int sz)
{
    uint8_t type = TLV_TYPE(recvb);

    switch (type) {
    case req_update_mod:
        sg_update_model_throushold(recvb, sz);
        break;

    case req_remove_target:
        sg_remove_target(recvb, sz);
        break;

    case req_update_topn:
        if (vrs_default_trapper()->hit_scd_conf.hit_second_en)
            sg_update_topn(recvb, sz);
       break;

    default:
        rt_log_error(ERRNO_INVALID_VAL, "Unknown type %d", type);
        break;
    }
}


/****************************************************************************
 函数名称  : SGSendRequestToVpm
 函数功能    : 给VPM发送请求
 输入参数    : arg, vrs_default_trapper
 输出参数    : 无
 返回值     : 线程在正常情况下不会退出
 创建人     : wangkai
 备注      : rte->vpm->sock由任务SGVpmAgent open
****************************************************************************/
static void* SGSendRequestToVpm(void __attribute__((__unused__)) *arg)
{
    struct vrs_trapper_t *rte = vrs_default_trapper();
    size_t s;
    message data;
    struct tlv *request = NULL;
    char buf[256] = {0};
    int flg = 0, xerror, tmo;

    FOREVER {
        if (rte->vpm->sock <= 0) /* 连接是SGVpmAgent做的 */
        {
            rt_log_error (ERRNO_SG, "task sock (%s:%d, sock=%d)",
                rte->vpm->ip, rte->vpm->port, rte->vpm->sock);
            sleep(1);
            continue;
        }

        data = NULL, flg = 0;
        rt_mq_recv (rte->vpm_mq, &data, (int *)&s);
        if (unlikely(NULL == data))
        {
            rt_log_error(ERRNO_SG, "rt_mq_recv:data null pointer");
            continue;
        }
        xerror = is_sock_ready_rd_wr (rte->vpm->sock, 60 * 1000000, &tmo, &flg);
        if (xerror <= 0 && tmo == 1)
        {
            kfree(data);
            continue;
        }

        if (flg & (1<<SELECT_WRITE)) {
            request = (struct tlv *)data;
            memset(buf, 0, sizeof(buf));
            pack_request_buf(buf, request);
            rt_sock_send(rte->vpm->sock, buf, request->l + TLV_FRAME_HEAD_LEN);
        }
        else
        {
            rt_log_error(ERRNO_SG, "vpm sock unable write???");
        }
        kfree(data);
    }

    task_deregistry_id(pthread_self());
    return NULL;
}

static struct rt_task_t     SGSendVpm = {
    .module = THIS,
    .name = "SG Send Vpm Request Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine =  SGSendRequestToVpm,
    .recycle = FORBIDDEN,
};

void *    SGVpmAgent (void __attribute__((__unused__)) *args)
{
    struct vrs_trapper_t *rte = vrs_default_trapper ();
    char   recvb[MSG_SIZE] = {0};
    struct vpm_t    *vpm;
    int    rsize = 0;
    int xerror, tmo;

    vpm = rte->vpm;
    FOREVER {
        do {
            vpm->sock = (rt_clnt_sock(0, vpm->ip, vpm->port, AF_INET));
            if (vpm->sock > 0) {
                vpm_flags_set_bit (VPM_FLGS_CONN_BIT, 1);
                break;
            }
            rt_log_notice ("Connecting to VPM (%s:%d, sock=%d): %s (%d)",
                        vpm->ip, vpm->port, vpm->sock, "failure", vpm_flags_chk_bit(VPM_FLGS_CONN_BIT));
            sleep (3);
        } while (vpm->sock < 0);

        rt_log_notice ("Connecting to VPM (%s:%d, sock=%d): %s (%d)",
                        vpm->ip, vpm->port, vpm->sock, "success", vpm_flags_chk_bit(VPM_FLGS_CONN_BIT));

        do {
            xerror = is_sock_ready(vpm->sock, 60 * 1000000, &tmo);
            if (xerror == 0 && tmo == 1)
                continue;
            if (xerror < 0) {
                rt_log_error (ERRNO_SG, "Select (%s:%d, sock=%d): %s",
                    vpm->ip, vpm->port, vpm->sock, strerror(errno));
                rt_sock_close (&vpm->sock, NULL);
                break;
            }

            memset(recvb, 0, MSG_SIZE);
            rsize = rt_sock_recv (vpm->sock, recvb, MSG_SIZE);
            if (rsize <= 0) {
                rt_log_error (ERRNO_SG,
                    "Peer(VPM) (%s:%d, sock=%d, rsize=%d), %s",
                                    vpm->ip, vpm->port, vpm->sock, rsize,
                                    (rsize == 0) ? "Connection closed" : strerror (errno));
                rt_sock_close (&vpm->sock, NULL);
                break;
            }
            SGVpmRequest(recvb, rsize);
        }while (vpm->sock > 0);
    }

    task_deregistry_id(pthread_self());

    return NULL;
}


static struct rt_task_t     SGReporterTask = {
    .module = THIS,
    .name = "SG Reporter Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGReporter ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t     SGSessionAgerTask = {
    .module = THIS,
    .name = "SG Aging Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGAger ,
    .recycle = FORBIDDEN,
};


static struct rt_task_t     SGMatcherLoaderTask = {
    .module = THIS,
    .name = "SG Matcher Loader Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGMatcherLoader,
    .recycle = FORBIDDEN,
};


static struct rt_task_t     SGModelManagerTask = {
    .module = THIS,
    .name = "SG Model Manager Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGVpmAgent,
    .recycle = FORBIDDEN,
};


void vpw_trapper_tool_init ()
{
    struct rt_vrstool_t *tool = (struct rt_vrstool_t *)kmalloc(sizeof (struct rt_vrstool_t), MPF_CLR, -1);
    if (unlikely(!tool)) {
        rt_log_notice ("Can not allocate memory for vrs tool");
    }

    memcpy (tool, engine_default_ops(), sizeof (struct rt_vrstool_t));
    tool->allowded_max_tasks = 16;
    tool->cur_tasks = 4;
    vrs_trapper_set_tool (tool);
}

void vpw_trapper_eth_init (const char *netdev, const char *logdir)
{
    struct rt_ethxx_trapper *xrte = rte_default_trapper ();

    rte_netdev_config (xrte , netdev);
    rte_netdev_perf_config (xrte , logdir);
    rte_filter_config (xrte , "vrs");
    rte_pktopts_config (xrte , "A_UNDO");
    rte_open (xrte);
    rte_preview(xrte);

}

void vpw_trapper_init (struct vrs_trapper_t *rte)
{
    if (unlikely (!rte))
        return;

    current_vpw = rte->vpw;

    __sg_check_and_mkdir(rte->sample_dir);
    __sg_check_and_mkdir(rte->model_dir);
    __sg_check_and_mkdir(rte->tmp_dir);
    __sg_check_and_mkdir(rte->vdu_dir);

    vpw_trapper_eth_init (current_vpw->netdev, rte->log_dir);

    cdr_flags_set_bit (CDR_FLGS_CONN_BIT, 0);
    vpm_flags_set_bit (VPM_FLGS_CONN_BIT, 0);

    rte->perform_tmr = tmr_create (SG,
                "Performence Monitor for VPW", TMR_PERIODIC,
                SGVpwTmrScanner, 1, (char **)rte, 30);

    rte->statistics_tmr = tmr_create (SG, "Statistics Hit for WEB", TMR_PERIODIC,
                SGVPWTmrStat, 1, (char **)rte, 200);

    // VPW会经常收到VPM的更新请求,不用刷的太频繁
    if (rte->hit_scd_conf.hit_second_en) {
        __sg_check_and_mkdir(rte->hit_scd_conf.topn_dir);
        rte->topn_disc_tmr = tmr_create (SG,
                "TOPN Disc Write for VPW", TMR_PERIODIC,
                senior_save_topn_bucket_disc, 1, (char **)rte, 300);
    }

    rte->cs_bucket_pool  = rt_pool_initialize (2048,
                            __cs_entry_alloc, __cs_entry_free, 0);


    rte->cm_bucket_pool  = rt_pool_initialize (1024,
                            __cm_entry_alloc, __cm_entry_free, 0);

    rte->cdr_bucket_pool = rt_pool_initialize (1024,
                            __cdr_entry_alloc, __cdr_entry_free, 0);

#ifdef CALL_ID_HASHED
    rt_mutex_init(&rte->lock, NULL);
    INIT_LIST_HEAD(&rte->list);

    rte->call_hlist_head = cstable_create_hash();
    if(unlikely(!rte->call_hlist_head)){
        rt_log_error (ERRNO_MEM_ALLOC,
            "%s", strerror(errno));
    }

    rte->cdr_hlist_head = cdr_create_hash();
    if(unlikely(!rte->cdr_hlist_head)){
        rt_log_error (ERRNO_MEM_ALLOC,
            "%s", strerror(errno));
    }

#endif

#if defined(ENABLE_MATCHER_THRDPOOL)
    assert((rte->thrdpool_for_matcher = thpool_create (rte->matchers, rte->wqe_size, 0)) != NULL);

    rt_log_notice ("Pool for Matcher started with %d threads and "
        "queue size of %d", rte->matchers, rte->wqe_size);
#endif

    vpw_trapper_tool_init ();


    rte->cdr_mq = rt_mq_create ("Cdr Report Queue");
    rte->vpm_mq = rt_mq_create ("Request Vpm Queue");

    task_registry (&SGMatcherLoaderTask);
    task_registry (&SGSessionAgerTask);
    task_registry (&SGReporterTask);
    task_registry (&SGModelManagerTask);
    task_registry (&SGSendVpm);

}


