#include "sysdefs.h"
#include <json/json.h>
#include "vrs.h"
#include "apr_md5.h"
#include "apr_xlate.h"
#include "apr_general.h"
#include "vpm_boost.h"
#include "conf.h"
#include "conf_yaml_loader.h"


static int del_topn_cell(topn_cell_t *cell);


// -------------------------------------------------------------------------------------
// VPW VPM通信tlv协议
/**
 * 描述：检查类型是否是vpm_vpw_req_e中的一种
 * 返回: 1 合法; 0 非法
 */
static int check_request_type_valid(int v)
{
    if (v > req_null && v < req_max) {
        return 1;
    }
    else {
        return 0;
    }
}

void pack_request_buf(char *buf, struct tlv *request)
{
    buf[0] = request->t;
    TLV_LEN_TO_BUF(request->l, buf+1);
    strncpy(buf+TLV_FRAME_HEAD_LEN, request->v, request->l);
}


/****************************************************************************
 函数名称  : senior_type_index_valid
 函数功能    : 检查当前index是否有效
 输入参数    : tlv_data, 从网络接收到的tlv数据; index当前解析的索引; size, 网络数据流
            的总长度
 输出参数    : 无
 返回值     : 1, 有效;0, 无效
 创建人     : wangkai
 备注      :
****************************************************************************/
static int senior_type_index_valid(const char *tlv_data, int index, int size)
{
    return (index+TLV_VAL_LEN(tlv_data+index)+TLV_FRAME_HEAD_LEN <= size);
}

static void senior_fill_tlv_request(OUT struct tlv *request, IN const char *tlv_data)
{
    request->t = TLV_TYPE(tlv_data);
    request->l = TLV_VAL_LEN(tlv_data);
    memcpy(request->v, tlv_data+TLV_FRAME_HEAD_LEN, request->l);
}

static int senior_tlv_next_type_index(OUT int *type_index, IN const char *tlv_data)
{
    int val_len = 0;
    val_len = TLV_VAL_LEN(tlv_data);
    *type_index += val_len;
    *type_index += TLV_FRAME_HEAD_LEN;
    return 0;
}

/****************************************************************************
 函数名称  : senior_parse_vpm_vpw_request
 函数功能    : 将二进制数据流按照特定的type解析成tlv包
 输入参数    : buf,二进制数据流; size, 收到的字节流的个数; type,指定解析的请求类型
 输出参数    : request, 指定type的tlv请求包
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_parse_vpm_vpw_request(IN char *buf, int size, vpm_vpw_req_e type, OUT struct tlv *request)
{
    int i, type_index = 0;
    if ( (NULL==buf) || (NULL==request) || (size > (int)sizeof(request->v)-TLV_FRAME_HEAD_LEN) )
    {
        return -ERRNO_INVALID_ARGU;
    }
    for (i=0; i<size; i++)
    {
        if (!senior_type_index_valid(buf, type_index, size))
        {
            return -ERRNO_RANGE;
        }
        if ( check_request_type_valid( TLV_TYPE(buf+type_index) ) )
        {
            if (TLV_TYPE(buf+type_index) == type)
            {
                senior_fill_tlv_request(request, buf+type_index);
                return XSUCCESS;
            }
            senior_tlv_next_type_index(&type_index, buf+type_index);
            i = type_index;
        }
        else
        {
            return -ERRNO_INVALID_VAL;
        }
    }

    return -ERRNO_FATAL;
}

int senior_parse_tlv(IN char *buf, int sz, OUT struct tlv *request)
{
    int xerror = -1;

    if (!buf || !request)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        goto finish;
    }

    request->t = TLV_TYPE(buf);
    request->l = TLV_VAL_LEN(buf);

    xerror = check_request_type_valid((int)(request->t));
    if (0 == xerror)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "invalid tlv type(%d)", request->t);
        xerror = -1;
        goto finish;
    }
    if (sz != (request->l + TLV_FRAME_HEAD_LEN))
    {
        rt_log_error(ERRNO_INVALID_ARGU, "invalid size(%d)", sz);
        xerror = -1;
        goto finish;
    }

    strncpy(request->v, buf + TLV_FRAME_HEAD_LEN, request->l);

    rt_log_debug("tlv t(%d), l(%d), v(%s)", request->t, request->l, request->v);

    xerror = 0;
finish:
    return xerror;
}

/****************************************************************************
 函数名称  : request_str_to_bt
 函数功能    : 将tlv请求包解析成线索命中的阈值
 输入参数    : request, tlv请求包
 输出参数    : bt,阈值结构体
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int request_str_to_bt(IN struct tlv *request, OUT boost_threshold_t *bt)
{
    int flag = 0;
    char *pos = NULL;
    if (!request || !bt) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -1;
    }

    if ( (pos = strstr(request->v, "ACCU-")) ) {
        if (1 == sscanf(pos, "ACCU-%d%*s", &bt->accurate_score)) {
            flag++;
        }
    }
    if ( (pos = strstr(request->v, "EXPL-")) ) {
        if (1 == sscanf(pos, "EXPL-%d%*s", &bt->exploring_score)) {
            flag++;
        }
    }
    if ( (pos = strstr(request->v, "DEFA-")) ) {
        if (1 == sscanf(pos, "DEFA-%d%*s", &bt->default_score)) {
            flag++;
        }
    }

    if (3 == flag)
        return 0;
    else
        return -1;
}

/****************************************************************************
 函数名称  : bt_to_request_str
 函数功能    : 将获得的阈值信息转成tlv格式
 输入参数    : bt获得的阈值信息
 输出参数    : request生成的tlv消息体
 返回值     : tlv包的长度,包括type和len
 创建人     : wangkai
 备注      :
****************************************************************************/
int bt_to_request_str(boost_threshold_t *bt, OUT struct tlv *request)
{
    char str[512] = {0};
    if (!bt || !request) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -1;
    }

    snprintf(str, sizeof(str)-STR_OVER_CHAR_LEN, "ACCU-%d EXPL-%d DEFA-%d", bt->accurate_score,
                bt->exploring_score, bt->default_score);

    request->t = req_update_bt;
    request->l = strlen(str) + STR_OVER_CHAR_LEN;

    strncpy(request->v, str, request->l);
    return request->l + TLV_FRAME_HEAD_LEN;
}

/****************************************************************************
 函数名称  : update_mod_to_reqstr
 函数功能    : 将目标id转成tlv格式
 输入参数    : 目标id
 输出参数    : request生成的tlv消息体
 返回值     : tlv包的总长度,包括type和len
 创建人     : wangkai
 备注      :
****************************************************************************/
int update_or_rm_mod_to_reqstr(uint64_t target_id, vpm_vpw_req_e type, OUT struct tlv *request)
{
    char sample_snapshot[64] = {0};
    if (!request) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -1;
    }
    snprintf(sample_snapshot, sizeof(sample_snapshot)-STR_OVER_CHAR_LEN, "%lu-0", target_id);

    request->t = type;
    request->l = strlen(sample_snapshot) + STR_OVER_CHAR_LEN;
    strncpy(request->v, sample_snapshot, request->l);
    return request->l + TLV_FRAME_HEAD_LEN;
}

// ---------------------------------------------------------------------------------
// 线索优化
static INIT_MUTEX(trhld_lock);

int load_thr_path_name()
{
    struct vrs_trapper_t *rte = vrs_default_trapper();
    char path[128] = {0};

    snprintf(path, sizeof(path), "%s/.threshold_cfg", rte->vdu_dir);
    rt_check_and_mkdir(path);

    snprintf(rte->thr_path_name, sizeof(rte->thr_path_name), "%s/threshold_cfg.ini", path);

    return XSUCCESS;
}


/****************************************************************************
 函数名称  : boost_save_threshold_conf
 函数功能    : 保存boost阈值的配置文件到本地磁盘
 输入参数    : tid,目标id; bt,将要被被保存的阈值信息; thr_path, 配置路径名
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int boost_save_threshold_conf(uint64_t tid, IN boost_threshold_t *bt, IN const char*thr_path)
{
    FILE *fp = NULL;
    int xerror = XSUCCESS;
    int lfar = 0, lfrr = 0, default_v = 0, lines = 0, flg = 0;
    uint64_t tmp_tid = 0;
    char cmd[256] = {0};
    char readline[1024]= {};

    if (!bt || !thr_path) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -ERRNO_INVALID_ARGU;
    }

rt_mutex_lock(&trhld_lock);

    fp = fopen(thr_path, "a+");
    if (unlikely(!fp)) {
        xerror = -ERRNO_ACCESS_DENIED;
        rt_log_error(ERRNO_ACCESS_DENIED,
            "%s", strerror(errno));
        goto finish;
    }

    while (fgets(readline, 1024, fp)) {
        tmp_tid = lfar = lfrr = default_v = 0;
        if (4 != sscanf(readline, "%lu %d %d %d", &tmp_tid, &lfar, &lfrr, &default_v))
            continue;
        lines++;
        if (tmp_tid == tid) {
            flg = 1;
            break;
        }
    }
    fclose(fp);

    if (flg) { //如果已经记录了这个target id, 则删除这行
        snprintf(cmd, sizeof(cmd), "sed -i '%dd' %s", lines, thr_path);
        rt_log_debug("do system: %s", cmd);
        do_system(cmd);
    }

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd)-STR_OVER_CHAR_LEN, "echo \"%lu %d %d %d\" >> %s", tid, bt->accurate_score, bt->exploring_score,
            bt->default_score, thr_path);
    rt_log_debug("do system: %s", cmd);
    do_system(cmd);

finish:
rt_mutex_unlock(&trhld_lock);

    return xerror;
}


/****************************************************************************
 函数名称  : senior_rm_target_threshold_conf
 函数功能    : 将磁盘配置文件中的相应的tid记录删掉
 输入参数    : tid,目标id, 配置路径名
 输出参数    : 无
 返回值     : XSUCCESS, 正确; 其他, 异常
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_rm_target_threshold_conf(uint64_t tid, IN const char *thr_path)
{
    FILE *fp = NULL;
    int xerror = XSUCCESS;
    int lfar = 0, lfrr = 0, default_v = 0, lines = 0, flg = 0;
    uint64_t tmp_tid = 0;
    char cmd[256] = {0};
    char readline[1024]= {};

    if (!thr_path)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -ERRNO_INVALID_ARGU;
    }

    rt_mutex_lock(&trhld_lock);

    fp = fopen(thr_path, "a+");
    if (unlikely(!fp)) {
        xerror = -ERRNO_ACCESS_DENIED;
        rt_log_error(ERRNO_ACCESS_DENIED,
            "%s", strerror(errno));
        goto finish;
    }

    while (fgets(readline, 1024, fp)) {
        tmp_tid = lfar = lfrr = default_v = 0;
        lines++;
        if (4 != sscanf(readline, "%lu %d %d %d", &tmp_tid, &lfar, &lfrr, &default_v))
            continue;
        if (tmp_tid == tid) {
            flg = 1;
            break;
        }
    }
    fclose(fp);

    if (flg) { // 如果已经记录了这个target id, 则删除这行
        snprintf(cmd, sizeof(cmd), "sed -i '%dd' %s", lines, thr_path);
        rt_log_debug("do system: %s", cmd);
        do_system(cmd);
    } else {  // 没有找到则出错
        xerror = -ERRNO_NO_ELEMENT;
        rt_log_error(xerror, "not find target:%lu record", tid);
    }

finish:
    rt_mutex_unlock(&trhld_lock);

    return xerror;
}


/****************************************************************************
 函数名称  : vrs_get_rule_threshold
 函数功能    : 根据类型获取阈值
 输入参数    : t,阈值类型; 配置文件的中获取的历史
 输出参数    : 无
 返回值     : -1,错误;>0,获取的得分
 创建人     : wangkai
 备注      :
****************************************************************************/
int vrs_get_rule_threshold(int t, IN boost_threshold_t *thr)
{
    int score = 0;

    switch (t)
    {
        case threshold_accu:
            score = thr->accurate_score;
            break;
        case threshold_expl:
            score = thr->exploring_score;
            break;
        case threshold_deft:
            score = thr->default_score;
            break;
        default:
            score = (-ERRNO_INVALID_ARGU);
    }
    return score;
}

/****************************************************************************
 函数名称  : boost_get_threshold
 函数功能    : 从磁盘里读取配置信息到内存
 输入参数    : tid,输入的目标id;t,要获取的阈值类型(LFRR LFAR DEFAULT)
 输出参数    : trhld,获取到的阈值
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int boost_get_threshold(uint64_t tid, int t, OUT int *trhld, const char *thr_path)
{
    char    readline[1024] = {0};
    int     xerror = -ERRNO_FATAL, ret;
    uint64_t  tmp_tid = 0;
    boost_threshold_t tmp_thr = {0};
    FILE*    fp;

    if (unlikely(!trhld || !thr_path)) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -ERRNO_INVALID_ARGU;
    }

    rt_mutex_lock (&trhld_lock);
    fp = fopen(thr_path, "r");
    if (unlikely(!fp))    {
        xerror = (-ERRNO_ACCESS_DENIED);
            rt_log_error(ERRNO_INVALID_ARGU,
                "%s", strerror(errno));
        goto finish;
    }

    *trhld = 0;
    while (fgets(readline, 1024, fp)) {
        memset(&tmp_thr, 0, sizeof(tmp_thr));
        if (4 != sscanf(readline, "%lu %d %d %d", &tmp_tid, &tmp_thr.accurate_score,
            &tmp_thr.exploring_score, &tmp_thr.default_score))
            continue; // 没有解析到thr

        if (tid == tmp_tid) {
            ret = vrs_get_rule_threshold(t, &tmp_thr);
            if (ret > 0) {
                *trhld = ret;
            }
            break;
        }
    }
    fclose(fp);
finish:
    rt_mutex_unlock(&trhld_lock);
    // 异常处理, 一般不会找不到，阈值设定为最低值
    if (0 == *trhld) {
        rt_log_notice("trhld Zero(0) cfg=%s, set default val=%d", thr_path, EXPLORING_MIN);
        *trhld = EXPLORING_MIN;
    }
    rt_log_notice("vrs senior: tid=%lu, type=%d, trhld=%d", tid, t, *trhld);

    return xerror;
}


//---------------------------------------------------------------------------------------
// vpw 二次命中

#define TOPN_CHECK_POINTER(__pointer) \
    do { \
        if (!__pointer) \
        { \
            rt_log_error(ERRNO_INVALID_ARGU, "pointer null"); \
            return -ERRNO_INVALID_ARGU; \
        } \
    } while (0)


static topn_hash_table_t TOPN_TABLE;

topn_hash_table_t *get_topn_tbl()
{
    return &TOPN_TABLE;
}

static int senior_topn_hash(uint64_t key)
{
    return key % MAX_TOPN_BUCKETS;
}


/****************************************************************************
 函数名称  : new_topn_cell
 函数功能    : 新建一个topn cell
 输入参数    : 无
 输出参数    : 无
 返回值     : 新建的topn cell; NULL, 出错
 创建人     : wangkai
 备注      :
****************************************************************************/
static topn_cell_t * new_topn_cell()
{
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);
    if (unlikely(NULL == conf))
    {
        return NULL;
    }
    topn_cell_t *new_cell = (topn_cell_t *)kmalloc(sizeof(topn_cell_t), MPF_CLR, -1);
    if (!new_cell) {
        rt_log_error(ERRNO_MEM_ALLOC, "%s", strerror(errno));
        return NULL;
    }
    new_cell->msg = (topn_msg_t *)kmalloc(sizeof(topn_msg_t) * conf->topn_max, MPF_CLR, -1);
    if (!new_cell->msg) {
        rt_log_error(ERRNO_MEM_ALLOC, "%s", strerror(errno));
        return NULL;
    }
    INIT_LIST_HEAD(&new_cell->node);
    return new_cell;
}


/****************************************************************************
 函数名称  : del_topn_cell
 函数功能    : 删除topn cell
 输入参数    : cell,被删除的topn cell
 输出参数    : 无
 返回值     : XSUCCESS,正确;其他，异常
 创建人     : wangkai
 备注      :
****************************************************************************/
static int del_topn_cell(topn_cell_t *cell)
{
    if (unlikely(NULL == cell))
    {
        rt_log_error(ERRNO_INVALID_ARGU, "pointer null");
        return -ERRNO_INVALID_ARGU;
    }

    list_del(&cell->node);
    kfree(cell->msg);
    cell->msg = NULL;
    kfree(cell);

    return XSUCCESS;
}


/****************************************************************************
 函数名称  : topn_cell_init
 函数功能    : 初始化topn cell
 输入参数    : tid, 初始化的cell的目标id; new_cell,被初始化的cell;
         :      msg, topn msg的数组; cnt, msg数组的成员个数
 输出参数    : 无
 返回值     : XSUCCESS,正确; 其他,异常
 创建人     : wangkai
 备注      : 只在topn刷新的时候用到
****************************************************************************/
static int topn_cell_init(uint64_t tid, topn_cell_t *new_cell, topn_msg_t *msg, int cnt)
{
    int i;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);
    if (NULL == new_cell || cnt>conf->topn_max)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "new_cell=%p, cnt:%d conf->topn_max=%d",
                new_cell, cnt, conf->topn_max);
        return -ERRNO_INVALID_ARGU;
    }
    new_cell->tid = tid;
    new_cell->cnt = cnt;
    new_cell->flg = TOPN_MEM_REFRESH;
    for (i=0; i<cnt; i++) {
        new_cell->msg[i].callid = msg[i].callid;
        new_cell->msg[i].score  = msg[i].score;
        new_cell->msg[i].dir    = msg[i].dir;
    }
    return XSUCCESS;
}


/****************************************************************************
 函数名称  : senior_update_topn_one_msg
 函数功能    : 更新一条topn消息到hash表
 输入参数    : key,目标ID; msg,输入的topn消息体
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      : 只有在初始化的时候从文件加载的时候用到
****************************************************************************/
static int senior_update_topn_one_msg(uint64_t key, OUT topn_msg_t *msg)
{
    topn_hash_table_t *tbl = get_topn_tbl();
    topn_cell_t *pos, *p;
    int found = 0, xret = XSUCCESS;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    // 参数判空
    TOPN_CHECK_POINTER(conf);
    TOPN_CHECK_POINTER(msg);
    TOPN_CHECK_POINTER(tbl->hash);

    int inx = tbl->hash(key);
    topn_bucket_t *bucket = tbl->first + inx;
    TOPN_CHECK_POINTER(bucket);

    rt_mutex_lock(&bucket->lock);
    list_for_each_entry_safe(pos, p, &bucket->head, node) {
        if (pos->tid == key) {
            found = 1;
            if (pos->cnt < conf->topn_max) {
                pos->msg[pos->cnt].callid = msg->callid;
                pos->msg[pos->cnt].score = msg->score;
                pos->msg[pos->cnt].dir   = msg->dir;
                pos->cnt ++;
                pos->flg = TOPN_DISC_SAVE; // 从磁盘加载的,磁盘上已经存在了
            } else {
                rt_log_error(ERRNO_FATAL, "MSG full");
            }
       }
    }
    if (0 == found) {
        topn_cell_t *new_cell = new_topn_cell();
        if (unlikely(NULL == new_cell))
        {
            xret = -ERRNO_MEM_ALLOC;
            goto finish;
        }
        if (conf->topn_max > 0)
        {
            new_cell->tid = key;
            new_cell->cnt = 1;
            new_cell->flg = TOPN_DISC_SAVE;
            new_cell->msg[0].callid = msg->callid;
            new_cell->msg[0].score  = msg->score;
            new_cell->msg[0].dir    = msg->dir;
            list_add_tail(&new_cell->node , &bucket->head);
        }
        else
        {
            rt_log_error(ERRNO_FATAL, "conf->topn_max = %d", conf->topn_max);
        }
    }

finish:
    rt_mutex_unlock(&bucket->lock);
    return xret;
}


/****************************************************************************
 函数名称  : senior_topn_get
 函数功能    : 从hash表中取出目标ID为tid的cnt个消息体,并得出分最低的一个索引
 输入参数    : tid,被查询的tid名
 输出参数    : msg,插叙到的topn消息体;cnt,topn消息体的个数;min_inx,分数最低的索引
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_topn_get(uint64_t tid, OUT topn_msg_t *msg, OUT int *cnt, OUT int *min_inx)
{
    topn_hash_table_t *tbl = get_topn_tbl();
    topn_bucket_t *bucket = NULL;
    topn_cell_t *pos, *p;
    int i = 0, xret = -1;
    uint32_t min_score = 0;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    TOPN_CHECK_POINTER(conf);
    TOPN_CHECK_POINTER(msg);
    TOPN_CHECK_POINTER(cnt);
    TOPN_CHECK_POINTER(min_inx);
    TOPN_CHECK_POINTER(tbl->hash);

    int inx = tbl->hash(tid);
    bucket = tbl->first + inx;

    rt_mutex_lock(&bucket->lock);
    list_for_each_entry_safe(pos, p, &bucket->head, node) {
        if (pos->tid == tid) {
            *cnt = pos->cnt;
            rt_log_debug("inx=%d, Get cnt = %d", inx, pos->cnt);
            for (i=0; i<pos->cnt; i++) {
                if ( pos->cnt ==  conf->topn_max) {
                    if (0 == i) {
                        min_score = pos->msg[i].score;
                        *min_inx = i;
                    }
                    else if (min_score > pos->msg[i].score) {
                        min_score = pos->msg[i].score;
                        *min_inx = i;
                    }
                } else if ( pos->cnt < conf->topn_max ) { // conf->topn_max,不用比较第pos->cnt就是最小的
                    *min_inx = pos->cnt;
                } else { // cnt > conf->topn_max，系统异常
                    rt_log_error(ERRNO_FATAL, "pos->cnt > %d", conf->topn_max);
                    goto finish;
                }
                msg[i].callid = pos->msg[i].callid;
                msg[i].score  = pos->msg[i].score;
                msg[i].dir    = pos->msg[i].dir;
            }
        }
    }
    xret = 0;

finish:
    rt_mutex_unlock(&bucket->lock);

    return xret;
}

/****************************************************************************
 函数名称  : senior_topn_table_init
 函数功能    : topn哈希表初始化
 输入参数    : 无
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_topn_table_init(void)
{
    int i;
    topn_hash_table_t *tbl = get_topn_tbl();
    topn_bucket_t *pos = NULL;

    memset(tbl, 0, sizeof(topn_hash_table_t));
    tbl->first = (topn_bucket_t *)kmalloc(MAX_TOPN_BUCKETS*sizeof(topn_bucket_t)
            , MPF_CLR, -1);
    if (NULL == tbl->first)
        return -1;

    tbl->max_topn = MAX_TOPN_BUCKETS;
    tbl->hash     = senior_topn_hash;
    tbl->update   = senior_update_topn_one_msg;

    pos = tbl->first;
    for (i=0; i<MAX_TOPN_BUCKETS; i++) {
        INIT_LIST_HEAD(&(pos[i].head));
    }
    for (i=0; i<MAX_TOPN_BUCKETS; i++) {
        rt_mutex_init(&(pos[i].lock), NULL);
    }

    return 0;
}


/****************************************************************************
 函数名称  : senior_topn_table_free
 函数功能    : topn哈希表内存释放
 输入参数    : 无
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_topn_table_free(void)
{
    int i;
    topn_cell_t *pos, *p;
    topn_hash_table_t *tbl = get_topn_tbl();
    topn_bucket_t *bucket = tbl->first;

    for (i=0; i<MAX_TOPN_BUCKETS; i++)
    {
        if (NULL == bucket+i)
        {
            rt_log_error(ERRNO_FATAL, "bucket null");
            return -ERRNO_FATAL;
        }
        rt_mutex_lock(&bucket[i].lock);
        list_for_each_entry_safe(pos, p, &bucket[i].head, node)
        {
            del_topn_cell(pos); //所有的cell删除
            pos = NULL;
        }
        rt_mutex_unlock(&bucket[i].lock);
    }
    kfree(tbl->first);
    tbl->first = NULL;

    return 0;
}


/****************************************************************************
 函数名称  : senior_load_topn_bucket
 函数功能    : 加载本地的指定名字的topn文件到hash表中
 输入参数    : tid, 指定目标名; filename，指定的文件名
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
static int senior_load_topn_bucket(uint64_t tid, IN const char *filename)
{
    FILE *fp = NULL;
    int xret = -1;
    char line[1024] = {0};
    topn_msg_t msg = {0};
    topn_hash_table_t *hash_tbl = get_topn_tbl();

    TOPN_CHECK_POINTER(hash_tbl->update);
    TOPN_CHECK_POINTER(filename);

    fp = fopen(filename, "r");
    if (likely(fp)) {
        while (fgets(line, sizeof(line), fp)) {
            if (3 != sscanf(line, "%lx %u %u", &(msg.callid), &(msg.score), &(msg.dir)))
                continue;
            hash_tbl->update(tid, &msg);
        }
        fclose(fp);
        xret = 0;
    }

    return xret;
}


/****************************************************************************
 函数名称  : senior_load_topns
 函数功能    : 加载本地的topn文件到内存
 输入参数    : 保存topn的路径
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_load_topns(const char *topn_path)
{
    uint64_t tid = 0;
    DIR  *pDir;
    struct dirent *ent;

    TOPN_CHECK_POINTER(topn_path);
    chdir(topn_path);
    pDir = opendir (topn_path);
    if(unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL,
                     "%s, %s", strerror(errno), topn_path);
        return -1;
    }
    while ((ent = readdir(pDir)) != NULL) {
        if (!strcmp(ent->d_name, ".") ||
                    !strcmp (ent->d_name, "..")){
            continue;
        }
        if (1 != sscanf(ent->d_name, "%lu.topn", &tid)) {
            continue;
        }
        senior_load_topn_bucket(tid, ent->d_name);
    }
    chdir("..");
    closedir(pDir);

    return 0;
}


/****************************************************************************
 函数名称  : senior_save_one_topn_cell_disc
 函数功能    : 将内存中的一条topn cell数据刷到磁盘里
 输入参数    : pos,将要刷到磁盘的cell指针; name, 保存的topn文件名
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      : vpw使用
****************************************************************************/
static int senior_save_one_topn_cell_disc(IN topn_cell_t *pos, IN char *name)
{
#define LINE_OVER_STR "\n"
    int i, sz = 0, xret = -1;
    char buf[2048] = {0};
    char tmp[128]  = {0};

    rt_log_debug("save %s", name);

    for (i=0; i<pos->cnt; i++) {
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, sizeof(tmp), "%lx %u %u", pos->msg[i].callid,
                pos->msg[i].score, pos->msg[i].dir);
        if (strlen(buf)+strlen(tmp)+strlen(LINE_OVER_STR) < sizeof(buf)) {
            strcat(buf, tmp);
            strcat(buf, LINE_OVER_STR);
        } else {
            rt_log_error(ERRNO_RANGE, "BUF ERROR");
        }
    }

    rt_log_debug("%s", buf);
    FILE *fp = fopen(name, "w+");
    if (unlikely(!fp)) {
        rt_log_error(ERRNO_FATAL, "fopen %s", name);
        return xret;
    }

    sz = fwrite(buf, 1, strlen(buf), fp);
    if ( (uint32_t)sz ==  strlen(buf) ) {
        xret = XSUCCESS;
        pos->flg = TOPN_DISC_SAVE;
    }

    fclose(fp);
    return xret;
}


/****************************************************************************
 函数名称  : senior_save_topn_bucket_disc
 函数功能    : 将内存中的所有未刷到磁盘中的topn数据刷到磁盘里
 输入参数    : pos,将要刷到磁盘的cell指针; name, 保存的topn文件名
 输出参数    : 无
 返回值     : 无
 创建人     : wangkai
 备注      : vpw使用
****************************************************************************/
void senior_save_topn_bucket_disc(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc, char **argv)
{
    int i;
    char name[128] = {0};
    topn_cell_t *pos = NULL, *p = NULL;
    topn_hash_table_t *hash_tbl = get_topn_tbl();
    struct vrs_trapper_t *rte;
    rte = (struct vrs_trapper_t *)argv;

    topn_bucket_t *bucket = hash_tbl->first;
    for (i=0; i<MAX_TOPN_BUCKETS; i++) {
        if (NULL == bucket+i) {
            rt_log_error(ERRNO_MEM_INVALID, "hash bucket null");
            return;
        }
        rt_mutex_lock(&bucket[i].lock);
        list_for_each_entry_safe(pos, p, &bucket[i].head, node) {
            if (pos->tid != 0 && TOPN_MEM_REFRESH == pos->flg) { // pos->flg记录此cell是否刷入到了磁盘
                memset(name, 0, sizeof(name));
                snprintf(name, sizeof(name), "%s/%lu.topn", rte->hit_scd_conf.topn_dir, pos->tid);
                senior_save_one_topn_cell_disc(pos, name); // 成功刷入磁盘会值pos->flg
            }
        }
        rt_mutex_unlock(&bucket[i].lock);
    }
}


/****************************************************************************
 函数名称  : senior_update_topn_fulltext
 函数功能    : 将指定tid的cnt个消息体更新到内存的hash表中
 输入参数    : msg消息体的具体内容
 输出参数    : 无
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_update_topn_fulltext(uint64_t tid, topn_msg_t *msg, int cnt)
{
    topn_hash_table_t *tbl = get_topn_tbl();
    TOPN_CHECK_POINTER(msg);
    topn_cell_t *pos, *p;
    int found = 0, xret = XSUCCESS;

    // 参数判空
    TOPN_CHECK_POINTER(msg);
    TOPN_CHECK_POINTER(tbl->hash);

    int inx = tbl->hash(tid);
    topn_bucket_t *bucket = tbl->first + inx;
    TOPN_CHECK_POINTER(bucket);

    rt_log_debug("Hash table inx = %d", inx);
    rt_mutex_lock(&bucket->lock);
    list_for_each_entry_safe(pos, p, &bucket->head, node) {
        if (pos->tid == tid) {
            found = 1;
            xret = topn_cell_init(tid, pos, msg, cnt);
            if (XSUCCESS != xret)
            {
                goto finish;
            }
        }
    }
    rt_log_debug("hash cell found = %s, cnt = %d\n", 1==found ? "found" : "not found", cnt);
    if (0 == found) {
        topn_cell_t *new_cell = new_topn_cell();
        if (unlikely(!new_cell)) {
            xret = -ERRNO_MEM_ALLOC;
            goto finish;
        }
        xret = topn_cell_init(tid, new_cell, msg, cnt);
        if (XSUCCESS != xret)
        {
            goto finish;
        }
        list_add_tail(&new_cell->node , &bucket->head);
    }
    rt_log_debug("senior_update_topn_fulltext success");

finish:
    rt_mutex_unlock(&bucket->lock);
    return xret;
}


int do_remove(const char *topn_name)
{
    int xerror = -ERRNO_FATAL;
    xerror = remove(topn_name);
    if (XSUCCESS == xerror)
    {
        rt_log_notice("rm %s", topn_name);
    }
    else
    {
        rt_log_error(xerror, "remove %s", strerror(errno));
    }
    return xerror;
}

/****************************************************************************
 函数名称  : boost_rm_target_topn_disc
 函数功能    : 删除对应目标topn文件
 输入参数    : rte, 有topn目录的配置信息; tid,将要删除文件的tid
 输出参数    : 无
 返回值     : XSUCCESS,正确; 其他, 异常
 创建人     : wangkai
 备注      :
****************************************************************************/
int boost_rm_target_topn_disc(IN struct vrs_trapper_t *rte, uint64_t tid)
{
    char topn_name[128] = {0};

    if (unlikely(NULL == rte))
    {
        rt_log_error(ERRNO_INVALID_ARGU, "pointer null");
        return -ERRNO_INVALID_ARGU;
    }

    snprintf(topn_name, sizeof(topn_name)-STR_OVER_CHAR_LEN, "%s/%lu.topn", rte->hit_scd_conf.topn_dir, tid);
    return do_remove(topn_name);
}


/****************************************************************************
 函数名称  : senior_rm_topn_cell_disck
 函数功能    : 删除对应的tid topn文件和hash表中的cell
 输入参数    : tid, 要删除cell的tid
 输出参数    : 无
 返回值     : XSUCCESS, 正确; 其他异常
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_rm_topn_cell_and_disc(struct vrs_trapper_t *rte, uint64_t tid)
{
    int xerror = -ERRNO_FATAL;
    topn_hash_table_t *tbl = get_topn_tbl();
    topn_cell_t *pos, *p;
    TOPN_CHECK_POINTER(tbl->hash);

    int index = tbl->hash(tid);
    topn_bucket_t *bucket = tbl->first + index;
    TOPN_CHECK_POINTER(bucket);

    rt_mutex_lock(&bucket->lock);
    list_for_each_entry_safe(pos, p, &bucket->head, node)
    {
        if (tid == pos->tid)
        {
            del_topn_cell(pos);
            pos = NULL;
            xerror = boost_rm_target_topn_disc(rte, tid);
            rt_log_notice("remove tid=%lu topn: %s", tid, xerror==XSUCCESS ? "SUCCESS" : "!!!FAIL!!!");
            rt_mutex_unlock(&bucket->lock);
            return xerror;
        }
    }

    rt_log_notice("tid=%lu cell not exsit", tid);
    rt_mutex_unlock(&bucket->lock);

    return -ERRNO_NO_ELEMENT;
}


/****************************************************************************
 函数名称  : senior_upd_topn_to_req
 函数功能    : 将vpm的请求vpw更新topn的消息转成tlv请求包
 输入参数    : msg,请求的消息
 输出参数    : request,tlv请求包
 返回值     : tlv包的总长度
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_upd_topn_to_req(uint64_t tid, topn_msg_t *msg, int cnt,
        struct tlv *request)
{
    char reqstr[2048] = {0};
    int i;
    TOPN_CHECK_POINTER(request);

    snprintf(reqstr, sizeof(reqstr)-1, "tid-%lu ", tid);
    for (i=0; i<cnt; i++) {
        char tmp[128] = {0};
        snprintf(tmp, sizeof(tmp)-STR_OVER_CHAR_LEN, "%d-%lx,%u,%u ", i, msg[i].callid, msg[i].score, msg[i].dir);
        if (strlen(reqstr)+strlen(tmp) < sizeof(reqstr)) {
            strcat(reqstr, tmp);
        } else {
            rt_log_error(ERRNO_RANGE, "BUF ERROR");
        }
    }

    request->t = req_update_topn;
    if (strlen(reqstr) > (sizeof(request->v)-STR_OVER_CHAR_LEN)) {
        rt_log_notice("request len >= %lu", sizeof(request->v)-STR_OVER_CHAR_LEN);
        request->l = sizeof(request->v)-STR_OVER_CHAR_LEN;
    } else {
        request->l = strlen(reqstr) + STR_OVER_CHAR_LEN;
    }

    strncpy(request->v, reqstr, request->l);
    return request->l + TLV_FRAME_HEAD_LEN;
}


/****************************************************************************
 函数名称  : senior_req_upd_topn_to_topnmsg
 函数功能    : 将vpm请求vpw更新本地topn的tlv包解析成消息体和个数
 输入参数    : request,  tlv请求包
 输出参数    : tid,解析出的目标id; msg,解析出的消息体;cnt,解析出的消息体个数
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_req_upd_topn_to_topnmsg(IN struct tlv*request,
        OUT uint64_t *tid, OUT topn_msg_t *msg, OUT int *cnt)
{
    int i;
    char *pos = NULL;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    TOPN_CHECK_POINTER(request);
    TOPN_CHECK_POINTER(tid);
    TOPN_CHECK_POINTER(msg);
    TOPN_CHECK_POINTER(cnt);
    TOPN_CHECK_POINTER(conf);

    if (1 != sscanf(request->v, "tid-%lu", tid)) {
        rt_log_error(ERRNO_FATAL, "request update topn no tid");
        return -1;
    }

    for (i=0; i<conf->topn_max; i++) {
        char tmp[128] = {0};
        snprintf(tmp, sizeof(tmp), "%d-", i);
        if ( (pos = strstr(request->v, tmp)) != NULL ) {
            if ( 3 != sscanf(pos+strlen(tmp), "%lx,%u,%u", &msg[i].callid, &msg[i].score, &msg[i].dir) ) {
                rt_log_error(ERRNO_FATAL, "%s:not find callid score", pos);
                return -1; // 没有找到对应的callid, score
            }
            *cnt = i+1;
        } else { // 没有找到特征码,无第i个请求
            break;
        }
    }

    return 0;
}


/****************************************************************************
 函数名称  : senior_topn_query_to_req
 函数功能    : 将tid和msg信息转换成tlv包
 输入参数    : msg信息体
 输出参数    : request, tlv请求包
 返回值     : tlv包的总长度
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_topn_query_to_req(uint64_t tid, IN topn_msg_t *msg, OUT struct tlv *request)
{
    char reqstr[1024] = {0};
    TOPN_CHECK_POINTER(msg);
    TOPN_CHECK_POINTER(request);

    snprintf(reqstr, sizeof(reqstr)-STR_OVER_CHAR_LEN, "tid-%lu 0-%lx,%u,%u", tid,
            msg->callid, msg->score, msg->dir);
    request->t = req_topn_query;
    request->l = strlen(reqstr) + STR_OVER_CHAR_LEN;
    strncpy(request->v, reqstr, request->l);
    return request->l + TLV_FRAME_HEAD_LEN;
}


/****************************************************************************
 函数名称  : senior_req_topnquery_to_topnmsg
 函数功能    : vpm将收到的tlv request转换成topn_msg
 输入参数    : vpm的请求消息
 输出参数    : tid,解析出的请求tid,msg解析出的消息体
 返回值     : -1错误;0正确
 创建人     : wangkai
 备注      :
****************************************************************************/
int senior_req_topnquery_to_topnmsg(struct tlv *request, OUT uint64_t *tid, OUT topn_msg_t *msg)
{
    TOPN_CHECK_POINTER(request);
    TOPN_CHECK_POINTER(tid);
    TOPN_CHECK_POINTER(msg);

    if (4 != sscanf(request->v, "tid-%lu 0-%lx,%u,%u", tid, &msg->callid, &msg->score, &msg->dir))
        return -1;
    return 0;
}

// 挂载检测

/****************************************************************************
 函数名称  : call_bash_check_status
 函数功能    : 调用一个bash命令并且查看状态
 输入参数    : cmd, 待执行的命令
 输出参数    : 无
 返回值     : 1, 命中执行正确; 0, 错误
 创建人     : wangkai
 备注      :
****************************************************************************/
int call_bash_check_status(const char *cmd)
{
    int status = system(cmd);

    if (-1 == status)
    {
        rt_log_error(ERRNO_FATAL, "system: %s error", cmd);
        return -1;
    }
    else
    {
        if (WIFEXITED(status) && 0==WEXITSTATUS(status))
        {
            rt_log_notice("NFS mount OK");
            return 1;
        }
        else
        {
            rt_log_error(ERRNO_FATAL, "NFS mount error, please check if NFS mount");
            return 0; /* 更好的方案是exit */
        }
    }
}

int senior_check_nfs_mount()
{
    return call_bash_check_status("mount | grep /vrs > /dev/null");
}































