
#ifndef __VRS_SENIOR_H__
#define __VRS_SENIOR_H__

/** default分数为系统推荐值 */
typedef struct {
    int default_score, exploring_score, accurate_score;
} boost_threshold_t;

typedef enum {
    req_null,
    req_update_mod,
    req_remove_target,  // vpm中把同一目标的wav都删除了, 通知vpw那个目标已经不存在
    req_update_bt,   // bt代表boost threshold
    req_update_stat, // 更新统计信息，特指线索命中时候的话务量和命中数目
    req_update_topn, // vpm-->vpw
    req_topn_query,  // vpw --> vpm, vpw请求vpm此topn是否可入围全局topn
    req_max
} vpm_vpw_req_e;

typedef enum {
    threshold_accu = 101, /*  精准模式 */
    threshold_expl = 102, /*  搜寻模式 */
    threshold_deft  = 103 /*  均衡模式 */
} threshold_type_t;

struct tlv {
    uint8_t t;
    uint16_t l;
    char v[2048];
} __attribute__((packed));

/* TLV包的头长度, type + len */
#define TLV_FRAME_HEAD_LEN  3
/* STR的结束字符'\0'的长度 */
#define STR_OVER_CHAR_LEN   1
/* TLV值的长度 */
#define TLV_VAL_LEN(__START__)      ( *(uint16_t *)( (char *)(__START__)+1) )
#define TLV_LEN_TO_BUF(len, buf)    ( *(uint16_t *)(buf) = (len) )
#define TLV_TYPE(__START__)          ( *(uint8_t *)(__START__))

typedef struct __topn_msg {
    uint64_t callid;
    uint32_t  score;
    uint32_t  dir;
} __attribute__((packed)) topn_msg_t;

#define TOPN_MEM_REFRESH    0
#define TOPN_DISC_SAVE      1

typedef struct __topn_cell {
    struct list_head  node;
    uint64_t          tid;
    uint8_t           cnt;
    uint8_t           flg;  // 刷新标记
    topn_msg_t        *msg; // msg个数由vrs.yaml中topn-max配置
} __attribute__((packed)) topn_cell_t;

typedef struct __topn_bucket_t {
    struct list_head  head;
    rt_mutex          lock;
} __attribute__((packed)) topn_bucket_t;

#define MAX_TOPN_BUCKETS  100000

typedef struct __topn_hash_table {
    topn_bucket_t *          first;
    uint64_t         max_topn;
    int (*hash)(uint64_t key);
    int (*update)(uint64_t key, topn_msg_t *topn);
} topn_hash_table_t;


// public methods
extern int load_thr_path_name();
extern int vrs_get_rule_threshold(int t, IN boost_threshold_t *thr);
extern int senior_parse_tlv(IN char *buf, int sz, OUT struct tlv *request);
extern int senior_parse_vpm_vpw_request(char *buf, int sz, vpm_vpw_req_e type, struct tlv *request);
extern int request_str_to_bt(struct tlv *request, boost_threshold_t *bt);
extern int bt_to_request_str(boost_threshold_t *bt, struct tlv *request);
extern int update_or_rm_mod_to_reqstr(uint64_t target_id, vpm_vpw_req_e type, struct tlv *request);
extern void pack_request_buf(char *buf, struct tlv *request);
extern void bt_default_val(boost_threshold_t *bt);
extern int boost_get_threshold(uint64_t tid, int t, int *trhld, IN const char *thr_path);
extern int boost_save_threshold_conf(uint64_t tid, boost_threshold_t *bt, IN const char*thr_path);
extern int senior_rm_target_threshold_conf(uint64_t tid, IN const char *thr_path);
extern int senior_rm_topn_cell_and_disc(struct vrs_trapper_t *rte, uint64_t tid);
extern int senior_topn_get(uint64_t tid, OUT topn_msg_t *msg, OUT int *cnt, int *min_inx);
extern int senior_topn_query_to_req(uint64_t tid, IN topn_msg_t *msg, OUT struct tlv *request);
extern int senior_req_upd_topn_to_topnmsg(IN struct tlv*request,
        OUT uint64_t *tid, OUT topn_msg_t *msg, OUT int *cnt);
extern int senior_update_topn_fulltext(uint64_t tid, topn_msg_t *msg, int cnt);
extern int senior_load_topns(const char *topn_path);
extern int senior_req_topnquery_to_topnmsg(struct tlv *request, OUT uint64_t *tid, OUT topn_msg_t *msg);
extern int senior_upd_topn_to_req(uint64_t tid, topn_msg_t *msg, int cnt,
        struct tlv *request);
extern int senior_topn_table_init(void);
extern void senior_save_topn_bucket_disc(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc, char **argv);
extern int senior_check_nfs_mount();

#endif

