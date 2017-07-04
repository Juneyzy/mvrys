#include "sysdefs.h"
#include <json/json.h>
#include "vrs.h"
#include "json_hdr.h"
#include "vpm_dms_agent.h"
#include "vpm_init.h"
#include "vpm_boost.h"

extern int librecv_init(int __attribute__((__unused__)) argc,
    char __attribute__((__unused__))**argv);


static __rt_always_inline__ struct vpw_clnt_t *vpw_alloc (int sock)
{
    struct vpw_clnt_t *clnt;

    clnt = (struct vpw_clnt_t *)kmalloc(sizeof (struct vpw_clnt_t), MPF_CLR, -1);
    if (likely (clnt)) {
        INIT_LIST_HEAD (&clnt->list);
        clnt->sock = sock;
    }

    return clnt;
}

static __rt_always_inline__ int
vpw_del (struct vpw_clnt_t *clnt,
                void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
    struct vpm_t    *vpm;

    vpm = clnt->vpm;
    if (likely (clnt)) {
        rt_sock_close (&clnt->sock, NULL);
        list_del (&clnt->list);
        kfree (clnt);
        vpm->clnt_socks --;
    }

    return 0;
}

static __rt_always_inline__ int
vpw_dummy (struct vpw_clnt_t *clnt,
                void __attribute__((__unused__))*data, size_t __attribute__((__unused__))s)
{
    if (likely (clnt)) {
        printf (" %d", clnt->sock);
    }

    return 0;
}

static __rt_always_inline__ void vpw_list_foreach (struct vpm_t *vpm,
                    int (*routine)(struct vpw_clnt_t *, void *, size_t), void *param, size_t s)
{
    struct    vpw_clnt_t *__this, *p;

    rt_mutex_lock (&vpm->clnt_socks_lock);

    list_for_each_entry_safe (__this, p, &vpm->clnt_socks_list, list) {
        if (routine)
            routine (__this, param, s);
    }

    rt_mutex_unlock (&vpm->clnt_socks_lock);
}

static __rt_always_inline__ void vpw_list_release (struct vpm_t *vpm)
{
    struct    vpw_clnt_t *__this, *p;

    rt_mutex_lock (&vpm->clnt_socks_lock);

    list_for_each_entry_safe (__this, p, &vpm->clnt_socks_list, list) {
        vpw_del (__this, NULL, 0);
    }

    rt_mutex_unlock (&vpm->clnt_socks_lock);
}

static __rt_always_inline__ struct vpw_clnt_t *vpw_list_add (struct vpm_t *vpm, int sock)
{
    struct    sockaddr_in sock_addr;
    struct vpw_clnt_t    *clnt = NULL;

    if (rt_sock_getpeername (sock, &sock_addr) < 0) {
        rt_sock_close (&sock, NULL);
        goto finish;
    }

    clnt = vpw_alloc (sock);

    if (unlikely (!clnt)) {
        rt_sock_close (&sock, NULL);
        goto finish;

    } else {

        rt_mutex_lock (&vpm->clnt_socks_lock);
        vpm->clnt_socks ++;
        clnt->vpm = vpm;
        list_add_tail (&clnt->list, &vpm->clnt_socks_list);
        rt_mutex_unlock (&vpm->clnt_socks_lock);

        rt_log_notice ("Peer(VPW) (%s:%d, sock=%d) connected, add to vpw list (total=%d)",
                    inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock, vpm->clnt_socks);
    }

finish:
    return clnt;
}


static __rt_always_inline__ void vpw_list_query (struct vpm_t *vpm, int sock,
                    int (*routine)(struct vpw_clnt_t *, void *, size_t), void *param, size_t s)
{
    struct    vpw_clnt_t *__this, *p;

    rt_mutex_lock (&vpm->clnt_socks_lock);

    list_for_each_entry_safe (__this, p, &vpm->clnt_socks_list, list) {
        if (sock == __this->sock) {
            if (routine)
                routine (__this, param, s);
        }
    }

    rt_mutex_unlock (&vpm->clnt_socks_lock);
}


static __rt_always_inline__ void _mkdir (const char * path)
{
    if (!rt_dir_exsit (path)) {
        mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
}


static __rt_always_inline__ void message_clone (char *ijstr, size_t l, void **clone)
{
    *clone = kmalloc (l + 1, MPF_CLR, -1);
    if (*clone) {
        memcpy64 (*clone, ijstr, l);
    }
}

static __rt_always_inline__ int vpw_notifier (struct vpw_clnt_t *clnt,  void *data, size_t s)
{
    int xerror = 0;

    xerror = rt_sock_send (clnt->sock, data, s);
    if (xerror < 0) {
        rt_sock_close (&clnt->sock, NULL);
        vpw_del (clnt, NULL, 0);
        xerror = -1;
    }

    return xerror;
}

static __rt_always_inline__ void mission_dispatcher (struct vpm_t     *vpm,
            struct json_hdr *jhdr, char *ijstr, size_t s)
{
    void *clone = NULL;
    MQ_ID    mq = MQ_ID_INVALID;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    web_json_element (jhdr);

    switch(jhdr->cmd){
        case SG_X_REG:
            rt_log_notice ("Register to WEB Success!");
            break;
        case SG_X_ADD:
        case SG_X_DEL:
        case SG_X_CATE:
            mq = vpm->regular_mq;
            break;
        case SG_X_MASS_QUERY:
            mq = vpm->mass_mq;
            break;
        case SG_X_BOOST:
            if (rte->vrs_boost_st)
            {
                mq = vpm->boost_mq;
                break;
            }
            else
            {
                return ;
            }
        case SG_X_TARGET_QUERY:
            mq = vpm->target_mq;
            break;
        default:
            return;
    }

    if (jhdr->cmd != SG_X_REG) {
        message_clone (ijstr, s, &clone);
        if (MQ_SUCCESS != rt_mq_send (mq, clone, (int)s)) {
            kfree (clone);
        }
    }

}

int vpm_get_valid_samples(char *sample_dir, uint64_t target_id, struct sample_file_t *sample_file, int *cnt)
{
    DIR  *pDir;
    struct dirent *ent;

    int      idx = 0;
    int      vid = 0;
    uint64_t tid = 0;
    struct  sample_file_t *__this = NULL;

    pDir = opendir (sample_dir);
    if(unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL, "%s, %s", strerror(errno), sample_dir);
        return -1;
    }

    /** get sample file */
    while ((ent = readdir(pDir)) != NULL)
    {
        if (!strcmp(ent->d_name, ".") || !strcmp (ent->d_name, ".."))
        {
            continue;
        }
        sscanf(ent->d_name, "%lu-%d.%*s", &tid, &vid);
        if(tid != target_id)
        {
            continue;
        }
        if (idx >= FILE_NUM_PER_TARGET)
        {
            rt_log_warning(ERRNO_FATAL, " [WARNING]The file number per target is greater 10");
            break;
        }
        __this = (struct sample_file_t *)(sample_file + idx);;
        snprintf(__this->sample_realpath, 255, "%s/%s", sample_dir, ent->d_name);
        __this->is_alaw = W_NOALAW;
        __this->wlen1 = __this->wlen2 = 0;
        rt_log_debug("sample_file[%d].sample_realpath : %s", idx, __this->sample_realpath);
        idx += 1;
    }

    closedir(pDir);

    *cnt = idx;

    return 0;
}

int vpm_get_wav_file_info(vrs_sample_attr_t *__this)
{
    int st_size = 0;
    int is_alaw = -1;
    FILE *fp;
    struct stat statbuf;
    wav_header_t wav_header;

    memset(&wav_header, 0, sizeof(wav_header));
    if ((0 == stat(__this->wav_path, &statbuf)) && (S_ISREG(statbuf.st_mode)))
    {
        st_size = statbuf.st_size;
    }

    if (st_size)
    {
        fp = fopen(__this->wav_path, "r");
        fread(&wav_header, 1, sizeof(wav_header_t), fp);
        if (!strncmp((char *)wav_header.riff, "RIFF", 4))
        {
            is_alaw = W_NOALAW; /** wav文件有头部 */
        }
        else
        {
            is_alaw = W_ALAW;
        }
        fclose(fp);
    }

    if (is_alaw == -1)
    {
        rt_log_warning(ERRNO_WARNING, " [WARNING] Get WAV attr failed");
        return ERRNO_WARNING;
    }
    __this->file_size = st_size;
    __this->is_alaw = is_alaw;

    return XSUCCESS;
}

int vpm_get_samples_info(char *sample_dir, vrs_target_attr_t *target_info)
{
    DIR  *pDir;
    struct dirent *ent;

    int      idx = 0;
    int      vid = 0;
    uint64_t tid = 0;
    vrs_sample_attr_t *__this = NULL;

    pDir = opendir (sample_dir);
    if(unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL, "%s, %s", strerror(errno), sample_dir);
        return -1;
    }

    /** get sample file */
    while ((ent = readdir(pDir)) != NULL)
    {
        if (!strcmp(ent->d_name, ".") || !strcmp (ent->d_name, ".."))
        {
            continue;
        }
        sscanf(ent->d_name, "%lu-%d.%*s", &tid, &vid);
        if(tid != target_info->target_id)
        {
            continue;
        }
        if (idx >= FILE_NUM_PER_TARGET)
        {
            rt_log_warning(ERRNO_FATAL, " [WARNING]The file number per target is greater 10");
            break;
        }
        __this = (vrs_sample_attr_t *)(target_info->samples + idx);
        snprintf(__this->wav_name, FILE_NAME_LEN - 1, "%s", ent->d_name);
        snprintf(__this->wav_path, FILE_PATH_LEN - 1, "%s/%s", sample_dir, ent->d_name);

        if (XSUCCESS != vpm_get_wav_file_info(__this))
            continue;

        __this->merge_flag = WAV_FILE_MERGE;  /*每个wav都初始化为可合并，交叉匹配时不满足条件的会设置为不合并*/
        rt_log_debug("samples[%d].wav_path : %s", idx, __this->wav_path);
        rt_log_debug("samples[%d].is_alaw  : %d", idx, __this->is_alaw);
        rt_log_debug("samples[%d].file_size: %d", idx, __this->file_size);
        idx += 1;
    }

    closedir(pDir);

    target_info->sample_cnt = idx;

    return 0;

}

int vpm_get_merge_size(vrs_target_attr_t *target_info)
{
    int i = 0;
    int file_size = 0;
    vrs_sample_attr_t *__this;

    for (i = 0; i < target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        if (__this->merge_flag == WAV_FILE_MERGE)
        {
            file_size += __this->file_size;
            if (__this->is_alaw == W_NOALAW)
            {
                file_size -= WAV_HEADER;
            }
        }
    }
    return file_size;
}

void vpm_wav_header_init(wav_header_t *header, int file_size)
{
    memcpy(header->riff, "RIFF", 4);
    header->file_size = file_size + 36;
    memcpy(header->riff_type, "WAVE", 4);

    memcpy(header->fmt, "fmt ", 4);
    header->fmt_size = 16;
    header->fmt_tag = 6;
    header->fmt_channel = 1;
    header->fmt_samples_persec = 8000;
    header->avg_bytes_persec = 8000;
    header->block_align = 1;
    header->bits_persample = 8;

    memcpy(header->data, "data", 4);
    header->data_size = file_size;

    return;
}

static int __vpm_wav_merge(const char *sample_realpath, int is_alaw, FILE *dst_fp)
{
    char buf[1024] = {0};
    int real_size = 0, sum_size = 0;
    FILE *src_fp;

    src_fp = fopen(sample_realpath, "r");

    if (is_alaw == W_NOALAW)
    {
        /*将wav的头部处理掉*/
        fread(buf, 1, sizeof(wav_header_t), src_fp);
        memset(buf, 0, 1024);
    }
    while ((real_size = fread(buf, 1, 1024, src_fp)) > 0)
    {
        fwrite(buf, 1, real_size, dst_fp);
        sum_size += real_size;
    }
    fclose(src_fp);

    return sum_size;
}

int vpm_wav_merge(vrs_target_attr_t *target_info, char *merge_realpath)
{
    FILE *fp;
    int i = 0, merge_size = 0, real_size = 0;
    wav_header_t wavhead;
    vrs_sample_attr_t *__this;

    memset(&wavhead, 0, sizeof(wav_header_t));

    fp = fopen (merge_realpath, "w+");
    if (!fp)
    {
        rt_log_notice("%s, %s", strerror(errno), merge_realpath);
        return -1;
    }
    merge_size = vpm_get_merge_size(target_info);
    if (0 == merge_size)
    {
        fclose(fp);
        rt_log_notice("merge_size(%d)", merge_size);
        return -1;
    }
    vpm_wav_header_init(&wavhead, merge_size);
    fwrite(&wavhead, 1, sizeof(wav_header_t), fp);

    for (i = 0; i < target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        if (__this->merge_flag == WAV_FILE_MERGE)
            real_size += __vpm_wav_merge(__this->wav_path, __this->is_alaw, fp);
    }
    fclose(fp);

    if (merge_size != real_size)
    {
        rt_log_notice("[WARNING] merge wav size(%d, %d)", merge_size, real_size);
    }

    return 0;
}

int vpm_trans_model(vrs_target_attr_t *target_info)
{
    int  xerror = 0, wlen1, wlen2;
    char model_realpath[FILE_PATH_LEN] = {0}, merge_realpath[FILE_PATH_LEN] = {0};
    struct vrs_trapper_t *rte = vrs_default_trapper();

    snprintf(model_realpath, 255, "%s/%lu-0.model", rte->model_dir, target_info->target_id);
    if (rt_file_exsit (model_realpath))
    {
        remove (model_realpath);
    }
    if (target_info->sample_cnt == 0)
    {
        rt_log_notice("Target(%lu) file is delete all", target_info->target_id);
    }
    else
    {
        snprintf(merge_realpath, FILE_PATH_LEN - 1, "%s/%lu_0.wav", rte->model_dir, target_info->target_id);
        vpm_wav_merge(target_info, merge_realpath);
        xerror = WavToModel (merge_realpath, model_realpath, &wlen1, &wlen2, W_NOALAW);
        rt_log_info("Creat Model(%lu-0.model) %s", target_info->target_id, xerror ? "failure" : "success");
        if (xerror)
        {
            if (xerror == (-4))
            {
                rt_log_info("(%s)Effective speech length is less than 30 s\n", merge_realpath);
            }
            else
            {
                rt_log_info("WavToModel ERROR return = %d\n", xerror);
            }
        }
    }
    if (rt_file_exsit (merge_realpath))
    {
        /*合并生成的wav文件在生成model后需要删除*/
        remove (merge_realpath);
    }
    return xerror;
}

// old
int vpm_samples_convert_to_model(uint64_t target_id)
{
    int  xerror = 0;
    struct vpm_t    *vpm;
    struct vrs_trapper_t    *rte;
    vrs_target_attr_t target_info;

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    memset(&target_info, 0, sizeof(target_info));
    target_info.target_id = target_id;
    xerror = vpm_get_samples_info(rte->sample_dir, &target_info);
    if (xerror != 0)
    {
        rt_log_error(ERRNO_FATAL, "error");
        return -1;
    }

    xerror = vpm_trans_model(&target_info);
    if (!xerror)
    {
        char *clone = NULL;
        struct tlv upd_mod = {0};
        size_t s = 0;

        memset(&upd_mod, 0, sizeof(struct tlv));
        xerror = update_or_rm_mod_to_reqstr(target_id, req_update_mod, &upd_mod);
        if (xerror < 0) {
            return -1;
        }
        s += xerror;

        clone = (char *)kmalloc(s, MPF_CLR, -1);
        if (!clone) return -1;

        pack_request_buf(clone, &upd_mod);
        rt_log_debug("up_mod.v = %s", upd_mod.v);
        rt_log_debug("msg = %s", clone+TLV_FRAME_HEAD_LEN);

        if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone,  s))
        {
            kfree(clone);
        }
    }
    return 0;
}

static void wav_model_merge_log(const char *format, ...)
{
#define DEFAULT_MERGE_LOG_DIR  "/usr/local/etc/merge_log.txt"
	char buf[1024] = {0};
	char cmd[1024] = {0};
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	rt_log_notice("record: %s", buf);

	snprintf(cmd, sizeof(cmd), "echo \"%s\" >> %s", buf, DEFAULT_MERGE_LOG_DIR);
	do_system(cmd);
}

/*
 目前合并信息是放到: /usr/local/etc/merge_log.txt中，追加在文件最后一行。
 分两行: xxxx.wav yyyy.wav zzzz.wav ===> xxxx-0.model
         计算得出来的参数信息
 */
static void vpm_samples_log(vrs_target_attr_t *target_info, char *model_realpath)
{
    int i = 0;
    char log[2048] = {0};
    vrs_sample_attr_t *__this = NULL;

    strcat(log, target_info->root_dir);
    strcat(log, "/");
    for (i = 0; i < target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        if (__this->merge_flag == WAV_FILE_MERGE)
        {
            strcat(log, __this->wav_name);
            strcat(log, " ");
        }
    }
    strcat(log, "==>");
    strcat(log, model_realpath);
    wav_model_merge_log("%s", log);
    wav_model_merge_log("Threshold: accurate %d, exploring %d, default %d",
            target_info->bth.accurate_score, target_info->bth.exploring_score, target_info->bth.default_score);

}

static int
vpm_send_update_model_threshold(struct vpm_t *vpm, vrs_target_attr_t *target_info)
{
    int xerror = XSUCCESS;
    char *clone = NULL;
    struct tlv upd_mod = {0}, upd_bt = {0};
    size_t s = 0;

    if (NULL == vpm || NULL == target_info)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "pointer null");
        return -ERRNO_INVALID_ARGU;
    }
    memset(&upd_mod, 0, sizeof(struct tlv));
    memset(&upd_bt, 0, sizeof(struct tlv));

    xerror = update_or_rm_mod_to_reqstr(target_info->target_id, req_update_mod, &upd_mod);
    if (xerror < 0) {
        return -1;
    }
    s += xerror;
    xerror = bt_to_request_str(&(target_info->bth), &upd_bt);
    if (xerror < 0) {
        return -1;
    }
    s += xerror;

    clone = (char *)kmalloc(s, MPF_CLR, -1);
    if (unlikely(!clone))
    {
        rt_log_error(ERRNO_MEM_ALLOC, "malloc");
        return -ERRNO_MEM_ALLOC;
    }
    pack_request_buf(clone, &upd_mod);
    pack_request_buf(clone+upd_mod.l+TLV_FRAME_HEAD_LEN, &upd_bt);
    rt_log_debug("clone[] = %s, clone[] = %s", clone+TLV_FRAME_HEAD_LEN, clone+upd_mod.l+TLV_FRAME_HEAD_LEN+TLV_FRAME_HEAD_LEN);

    if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone,  s))
    {
        kfree(clone);
    }
    return 0;
}


static int
vpm_send_remove_target(struct vpm_t *vpm, uint64_t target_id)
{
    int xerror = XSUCCESS;
    char *clone = NULL;
    struct tlv rm_mod = {0};
    size_t s = 0;

    memset(&rm_mod, 0, sizeof(struct tlv));
    xerror = update_or_rm_mod_to_reqstr(target_id, req_remove_target, &rm_mod);
    if (xerror < 0) {
        return -1;
    }
    s += xerror;

    clone = (char *)kmalloc(s, MPF_CLR, -1);
    if (unlikely(!clone))
    {
        rt_log_error(ERRNO_MEM_ALLOC, "malloc");
        return -ERRNO_MEM_ALLOC;
    }

    pack_request_buf(clone, &rm_mod);
    rt_log_debug("clone[] = %s", clone+TLV_FRAME_HEAD_LEN);
    if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone,  s))
    {
        kfree(clone);
    }

    return 0;
}


// new
int boost_vpm_samples_convert_to_model(uint64_t target_id, json_object *arr, int json_en, char *wav)
{
    int  xerror = 0, i = 0, status = add_st_normal;
    char model_realpath[256] = {0};
    int valid_sample_cnt = 0;
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    vrs_target_attr_t target_info;
    vrs_sample_attr_t *__this = NULL;

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    memset(&target_info, 0, sizeof(target_info));
    target_info.target_id = target_id;
    snprintf(target_info.root_dir, FILE_PATH_LEN, "%s", rte->sample_dir);
    xerror = vpm_get_samples_info(rte->sample_dir, &target_info);
    if (xerror != 0)
    {
        rt_log_error(ERRNO_FATAL, "error");
        return -1;
    }
    snprintf(model_realpath, 255, "%s/%lu-0.model", rte->model_dir, target_id);

    valid_sample_cnt = target_info.sample_cnt;
    if (0 == valid_sample_cnt)
    {
        if (rt_file_exsit (model_realpath))
        {
            /*有效样本为0，删除model*/
            remove (model_realpath);
        }
        bt_default_val(&(target_info.bth));
        rt_log_notice("valid sample cnt is 0");
    }
    else if (1 == valid_sample_cnt)
    {
        if (json_en)
        {
            /** 有效样本数目为1，判断有效时长 */
            status = vpm_get_wav_status(wav);
            vpm_json_assembling_result(wav, status, arr);
        }
        if ((status == add_st_less_30s) || (status == add_st_null))
        {
            rt_log_notice("wav error status(%d)", status);
            return -1;
        }
        xerror = vpm_trans_model(&target_info);
        bt_default_val(&(target_info.bth));
    }
    else if (2 == valid_sample_cnt)
    {
        if (json_en)
        {
            /** 有效样本数目为2，判断相似度 */
            status = vpm_judge_samples_similarity(&target_info);
            vpm_json_assembling_result(wav, status, arr);
        }
        if (status != add_st_null)
        {
            xerror = vpm_trans_model(&target_info);
            bt_default_val(&(target_info.bth));
        }
    }
    else
    {
        xerror = vpm_comp_analysis_samples(&target_info);
        if (json_en)
        {
            for (i = 0; i < valid_sample_cnt; i++)
            {
                __this = (vrs_sample_attr_t *)(target_info.samples + i);
                vpm_json_assembling_result(__this->wav_path, __this->status, arr);
            }
        }
        if (xerror < 0)
        {
            return -1; // 没有可以建立model样本，这里直接返回
        }
        xerror = vpm_trans_model(&target_info);
    }

    if (!xerror)
    {
        if (0 == valid_sample_cnt) // 没有tid-x.wav,走删除流程
        {
            senior_rm_target_threshold_conf(target_info.target_id, rte->thr_path_name);
            senior_rm_topn_cell_and_disc(rte, target_info.target_id);
            xerror = vpm_send_remove_target(vpm, target_info.target_id);
        }
        else // 至少有一个tid-x.wav, 走更新流程
        {
            vpm_samples_log(&target_info, model_realpath);
            if (target_info.bth.accurate_score != 0 &&
                    target_info.bth.default_score != 0 && target_info.bth.exploring_score != 0) { // 保存学习到的阈值配置
                xerror = boost_save_threshold_conf(target_info.target_id, &target_info.bth, rte->thr_path_name);
                if (XSUCCESS != xerror)
                    return xerror;
            } else {
                rt_log_error(ERRNO_FATAL, "threshold calc error");
                return -ERRNO_FATAL;
            }
            xerror = vpm_send_update_model_threshold(vpm, &target_info);
        }

        if (XSUCCESS != xerror)
        {
            rt_log_error(xerror, "send model change");
        }
    }

    return xerror;
}

static __rt_always_inline__ int vpm_add_sample (const char *sample, int *status, int flags)
{
    int    notify = 0, wlen1, wlen2, xerror = 0;
    char sample_realpath[256] = {0}, model_realpath[256] = {0}, sample_snapshot[32] = {0};
    struct    vrs_trapper_t    *rte;
    struct    vpm_t    *vpm;

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    _mkdir (rte->sample_dir);

    sscanf(sample,  "%[^.].%*s", sample_snapshot);
    snprintf(sample_realpath, 255, "%s/%s", rte->sample_dir, sample);
    snprintf(model_realpath, 255, "%s/%s.model", rte->model_dir, sample_snapshot);

    if (rt_file_exsit (sample_realpath)) {
        xerror = WavToModel (sample_realpath, model_realpath, &wlen1, &wlen2, W_NOALAW);
        if (xerror){
            *status = 0;
            if (xerror == (-4)){
                rt_log_info("(%s)Effective speech length is less than 30 s\n", sample_realpath);
            }
        }
        else {
            notify = 1;
            *status = 1;
        }
    }

    if (notify &&
        (flags & VPM_FLGS_RSYNC)) {
        void *clone = NULL;
        size_t s =  strlen(sample_snapshot);
        message_clone (sample_snapshot, s, &clone);
        if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone,  s)) {
            kfree (clone);
        }
    }

    return 0;
}

static __rt_always_inline__ int vpm_del_sample (const char *sample, int *status, int flags)
{
    int    notify = 0;
    char sample_realpath[256] = {0}, model_realpath[256] = {0}, sample_snapshot[32] = {0};
    struct    vrs_trapper_t    *rte;
    struct    vpm_t    *vpm;

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    _mkdir (rte->sample_dir);

    sscanf(sample,  "%[^.].%*s", sample_snapshot);
    snprintf(sample_realpath, 255, "%s/%s", rte->sample_dir, sample);
    snprintf(model_realpath, 255, "%s/%s.model", rte->model_dir, sample_snapshot);

    if (rt_file_exsit (sample_realpath)) {
        remove (sample_realpath);
    }
    if (rt_file_exsit (model_realpath)) {
        remove (model_realpath);
    }

    *status =1;
    notify = 1;

    if (notify &&
        (flags & VPM_FLGS_RSYNC)) {
        void *clone = NULL;
        size_t s =  strlen(sample_snapshot);
        message_clone (sample_snapshot, s, &clone);
        if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone,  s)) {
            kfree (clone);
        }
    }

    return 0;
}

static __rt_always_inline__ int vpm_mass_sample (json_object *sample_array, const char *sample,
            time_t *start, time_t *end, int __attribute__((__unused__))*status, int __attribute__((__unused__))flags)
{

    static const uint64_t offset = 24 * 60 * 60;
    char sample_realpath[256] = {0}, model_realpath[256] = {0}, filename[256] = {0}, **so = NULL;
    float *sg_score = NULL;
    int xerror = -1, m = 0, md_index = 0, i = 0;
    int64_t sg_valid_models, sg_valid_models_size, sg_valid_models_total, sg_models_total, sg_max_models = 0;
    time_t tm;
    struct vrs_trapper_t    *rte;
    struct modelist_t *cur_modelist;
    struct rt_vrstool_t *tool;
    struct json_object* array_ls_atom;
    struct tm      tt = { 0 };
    time_t    cur_time = time(NULL);

    rte = vrs_default_trapper ();
    tool = rte->tool;

    for (tm = *start; tm <= *end; tm += offset){
        localtime_r(&tm, &tt);
        snprintf(model_realpath, 1024, "%s/normal/%04d-%02d-%02d/", rte->vdu_dir, tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        sg_max_models = sg_get_max_models(model_realpath, cur_time);
        rt_log_info("In directory(%s) summary file is(%lu)", model_realpath, sg_max_models);

        if (sg_max_models <= 0){
            rt_log_error(ERRNO_NO_ELEMENT, "Summary file is (%lu)", sg_max_models);
            continue;
        }

        cur_modelist = modelist_create (sg_max_models);
        if(unlikely(!cur_modelist)){
            rt_log_error(ERRNO_MEM_ALLOC, "Create modelist(%lu) fail", sg_max_models);
            return -1;
        }

        localtime_r(&tm, &tt);
        snprintf(model_realpath, 1024, "%s/normal/%04d-%02d-%02d/", rte->vdu_dir, tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
        xerror = modelist_load_by_user (model_realpath, ML_FLG_RIGN|ML_FLG_EXC_MOD_0, cur_modelist,
                &sg_valid_models, &sg_valid_models_size, &sg_valid_models_total,
                &sg_models_total, cur_time);
        if (xerror < 0){
            rt_log_error(ERRNO_FATAL, "Directory(%s) conversion model failure", model_realpath);
            continue;
        }

        if (likely (cur_modelist) &&
            cur_modelist->sg_cur_size > 0) {
            sg_score = cur_modelist->sg_score;
            m = cur_modelist->sg_cur_size;
            so = cur_modelist->sg_owner;
        }
        sprintf(sample_realpath, "%s/%s", rte->mass_dir, sample);

        xerror = tool->voice_recognition_advanced_ops (sample_realpath, cur_modelist, sg_score, &md_index, W_ALAW);
        if ((xerror == 0) &&
            (md_index < m)){
            for (i = 0 ; i < m; i ++) {
                if (sg_score[i] >= 0) {
                    sscanf(so[i],  "%[^_]_%*s", filename);
                    array_ls_atom = json_object_new_object();
                    json_object_object_add(array_ls_atom, "filename", json_object_new_string(filename));
                    json_object_object_add(array_ls_atom, "score", json_object_new_int((int)sg_score[i]));
                    json_object_array_add(sample_array, array_ls_atom);
                }else{
                    sscanf(so[i],  "%[^_]_%*s", filename);
                    rt_log_debug("(%s)Can't match the score is(%f)", filename, sg_score[i]);
                }
            }
        }

        modelist_destroy (cur_modelist);
    }

    return 0;
}

// old
static __rt_always_inline__ int vpm_json_add_sample (struct json_hdr __attribute__((__unused__))*jhdr,
                json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample, *desc, *sample_object, *sample_array;
    int    samples = 0, i, status = 0;
    char    *__desc;
    int      vid = 0;
    uint64_t tid = 0;

    ls =  web_json_to_field (msg_body, "ls");
    samples = json_object_array_length (ls);
    sample_array = json_object_new_array ();

    for (i = 0; i < samples; i ++) {

        status    =    0;
        sample    =    json_object_array_get_idx(ls, i);
        desc    =    web_json_to_field(sample, "filename");
        if (unlikely (!desc))
            continue;

        __desc = (char *)json_object_get_string(desc);

        sscanf(__desc, "%lu-%d.%*s", &tid, &vid);
        status= 1;
        vpm_add_sample (__desc, &status, 0); // VPM_FLGS_RSYNC --> 0

        sample_object = json_object_new_object();
        json_object_object_add(sample_object, "filename", desc);
        json_object_object_add(sample_object, "status", json_object_new_int(status));
        json_object_array_add (sample_array, sample_object);
    }

    json_object_object_add (msg_obody, "ls", sample_array);

    if (tid != 0)
        vpm_samples_convert_to_model(tid);

    return 0;
}

// new
int boost_vpm_json_add_sample (struct json_hdr __attribute__((__unused__))*jhdr,
                json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample, *desc;
    char    *__desc = NULL;
    int      vid = 0, samples = 0, i = 0, status;
    uint64_t tid = 0;
    char wav[128] = {0};

    ls =  web_json_to_field (msg_body, "ls");
    samples = json_object_array_length (ls);

    for (i = 0; i < samples; i ++) {
        sample    =    json_object_array_get_idx(ls, 0);
        desc    =    web_json_to_field(sample, "filename");
        if (!desc) {
            json_object *sample_array = json_object_new_array ();
            json_object_object_add (msg_obody, "ls", sample_array);
            return 0;
        }
        __desc = (char *)json_object_get_string(desc);
        sscanf(__desc, "%lu-%d.%*s", &tid, &vid);
    }
    if (__desc)
        snprintf(wav, sizeof(wav), "%s/%s", vrs_default_trapper()->sample_dir, __desc);

    if (tid != 0) {
        json_object *arr = json_object_new_array();
        if (unlikely(!arr))
            return -1;
        if (1 != check_current_file_redundant(tid, wav, arr)) {
            vpm_add_sample(__desc, &status, 0);
            boost_vpm_samples_convert_to_model(tid, arr, 1, wav);
        }
        json_object_object_add(msg_obody, "ls", arr);
    }

    return 0;
}

// old
static __rt_always_inline__ int vpm_json_del_sample (struct json_hdr __attribute__((__unused__))*jhdr,
                json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample, *desc, *sample_object, *sample_array;
    int    samples = 0, i, status = 0;
    char    *__desc;
    int      vid = 0;
    uint64_t tid = 0;

    ls = web_json_to_field (msg_body, "ls");
    samples = json_object_array_length(ls);
    sample_array = json_object_new_array();

    for (i = 0; i < samples; i ++) {

        status    =    0;

        sample    =    json_object_array_get_idx(ls, i);
        desc    =    web_json_to_field (sample, "filename");
        if (unlikely (!desc))
            continue;

        __desc = (char *)json_object_get_string(desc);

        sscanf(__desc, "%lu-%d.%*s", &tid, &vid);
        status = 1;
        vpm_del_sample (__desc, &status, 0); //VPM_FLGS_RSYNC --> 0

        sample_object = json_object_new_object();
        json_object_object_add(sample_object, "filename", desc);
        json_object_object_add(sample_object, "status", json_object_new_int(status));
        json_object_array_add (sample_array, sample_object);
    }

    json_object_object_add (msg_obody, "ls", sample_array);

    if (tid != 0)
        vpm_samples_convert_to_model(tid);

    return 0;
}


// new
static __rt_always_inline__ int boost_vpm_json_del_sample (struct json_hdr __attribute__((__unused__))*jhdr,
                json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample, *desc, *sample_object, *sample_array;
    int    samples = 0, i, status = 0;
    char    *__desc;
    int      vid = 0;
    uint64_t tid = 0;

    ls = web_json_to_field (msg_body, "ls");
    samples = json_object_array_length(ls);
    sample_array = json_object_new_array();

    for (i = 0; i < samples; i ++) {

        status    =    0;

        sample    =    json_object_array_get_idx(ls, i);
        desc    =    web_json_to_field (sample, "filename");
        if (unlikely (!desc))
            continue;

        __desc = (char *)json_object_get_string(desc);

        sscanf(__desc, "%lu-%d.%*s", &tid, &vid);
        status = 1;
        vpm_del_sample (__desc, &status, 0); //VPM_FLGS_RSYNC --> 0

        sample_object = json_object_new_object();
        json_object_object_add(sample_object, "filename", desc);
        json_object_object_add(sample_object, "status", json_object_new_int(status));
        json_object_array_add (sample_array, sample_object);
    }

    json_object_object_add (msg_obody, "ls", sample_array);

    if (tid != 0)
        boost_vpm_samples_convert_to_model(tid, msg_obody, 0, NULL);

    return 0;
}

static __rt_always_inline__ int vpm_json_array_add(char *__desc, char **so, float *sg_score, int m,
                                     json_object *sample_array)
{
    json_object *array_item_node, *array_item, *array_obj;
    char filename[256] = {0};
    int i = 0;
    int __cate = 0;

    array_obj = json_object_new_object();
    array_item = json_object_new_array();
    for (i = 0 ; i < m; i ++) {
        if (sg_score[i] >= 0) {
            array_item_node = json_object_new_object();
            snprintf (filename, 255, "%s.wav", so[i]);
            json_object_object_add(array_item_node, "filename", json_object_new_string(filename));
            json_object_object_add(array_item_node, "score", json_object_new_int((int)sg_score[i]));
            json_object_object_add(array_item_node, "oldname", json_object_new_string(__desc));
            json_object_array_add(array_item, array_item_node);
            __cate ++;
        }
    }

    json_object_object_add(array_obj, "item", array_item);
    json_object_array_add(sample_array, array_obj);

    return 0;
}

static __rt_always_inline__ int vpm_cate_sample(json_object *ls, json_object *sample_array, int samples)
{
    int64_t sg_max_models = 0, sg_valid_models, sg_valid_models_size, sg_valid_models_total, sg_models_total;
    float *sg_score = NULL;
    char **so = NULL, *__desc, sample_realpath[256] = {0};
    int xerror = -1, m = 0, md_index = 0, i = 0;
    json_object *sample, *desc;
    struct vrs_trapper_t    *rte;
    struct modelist_t *cur_modelist;
    struct rt_vrstool_t *tool;

    rte = vrs_default_trapper ();
    tool = rte->tool;

    sg_max_models = samples;
    cur_modelist = modelist_create (sg_max_models);
    if(unlikely(!cur_modelist))
        return -1;

    for (i = 0; i < samples; i ++) {
        sample    =    json_object_array_get_idx(ls, i);
        desc        =    web_json_to_field (sample, "filename");
        if (unlikely (!desc))
            continue;
        __desc = (char *)json_object_get_string(desc);
        xerror = modelist_delta_load_model_to_memory (rte->category_dir, __desc, cur_modelist, ML_FLG_RIGN,
                                           &sg_valid_models, &sg_valid_models_size, &sg_valid_models_total,
                                           &sg_models_total);
        if (xerror <= 0)
            continue;

    }

    if (likely (cur_modelist) &&
        cur_modelist->sg_cur_size > 0) {
        sg_score = cur_modelist->sg_score;
        m = cur_modelist->sg_cur_size;
        so = cur_modelist->sg_owner;
    }

    for (i = 0; i < samples; i++) {
        sample    =    json_object_array_get_idx(ls, i);
        desc    =    web_json_to_field (sample, "filename");
        if (unlikely (!desc))
            continue;

        __desc = (char *)json_object_get_string(desc);
        sprintf(sample_realpath, "%s/%s", rte->category_dir, __desc);
        xerror = tool->voice_recognition_advanced_ops (sample_realpath, cur_modelist, sg_score, &md_index, W_ALAW);
        if ((xerror == 0) &&
            (md_index < m)) {
            xerror = vpm_json_array_add(__desc, so, sg_score, m, sample_array);
        }
    }

    modelist_destroy (cur_modelist);

    return 0;
}

static __rt_always_inline__ int vpm_json_cate_sample(struct json_hdr __attribute__((__unused__))*jhdr,
                                       json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample_array;
    int    samples = 0;

    ls = web_json_to_field (msg_body, "ls");
    samples = json_object_array_length(ls);
    sample_array = json_object_new_array();

    vpm_cate_sample(ls, sample_array, samples);

    json_object_object_add (msg_obody, "ls", sample_array);

    return 0;
}

static __rt_always_inline__ int vpm_json_mass_query (struct json_hdr __attribute__((__unused__))*jhdr,
                json_object *msg_body, json_object *msg_obody)
{
    json_object *sample, *sample_array, *start_object, *end_object;
    int    status = 0;
    char    *__desc;
    time_t start, end;

    sample = web_json_to_field (msg_body, "filename");

    start_object = web_json_to_field (msg_body, "startdate");
    sscanf(json_object_get_string(start_object), "%ld", &start);

    end_object = web_json_to_field (msg_body, "enddate");
    sscanf(json_object_get_string(end_object), "%ld", &end);

    if (unlikely (sample)) {

        sample_array = json_object_new_array();


        __desc = (char *)json_object_get_string(sample);

        vpm_mass_sample (sample_array, __desc, &start, &end, &status, 0);
        json_object_object_add (msg_obody, "ls", sample_array);
    }

    return 0;
}

struct target_query_msg {
    char name[32];
    int  score;
    uint64_t  key;
    int  cnt;
};

#define TARGET_GROUPS_MAX 50

#define TARGET_MAX        (TARGET_GROUPS_MAX * FILE_NUM_PER_TARGET)
struct target_query_msg tqm[TARGET_MAX];

#define GROUP_ADDR(group) ( &tqm[group*FILE_NUM_PER_TARGET % TARGET_MAX] )

#define SET_TARGET_WAV_MSG(msg, __key, __score, __name) \
    do { \
        msg.key = (__key); \
        msg.score = (int)(__score); \
        snprintf(msg.name, sizeof(msg.name), "%s.wav", (__name)); \
    } while (0)

#define next_group(_pos) \
    (_pos += FILE_NUM_PER_TARGET)

#define group_tail() (&tqm[TARGET_MAX - FILE_NUM_PER_TARGET])

#define forlist_target_groups(_pos, head) \
    for (_pos=head; _pos->key!=0 && _pos<=group_tail(); next_group(_pos)) \

static int vpm_target_valid_groups(uint64_t key)
{
    int i;
    struct target_query_msg *head = NULL;

    if (0 == key) {
        rt_log_error(ERRNO_INVALID_ARGU, "KEY == 0");
        goto finish;
    }
    for (i=0; i<TARGET_GROUPS_MAX; i++) {
        head = GROUP_ADDR(i);
        if (0==head->key || key == head->key)
            return i;
    }

finish:
    return -1;
}

static inline void dump_target_msg(struct target_query_msg *msg)
{
    rt_log_debug("key %lu", msg->key);
    rt_log_debug("score %d", msg->score);
    rt_log_debug("name %s", msg->name);
}

static int vpm_target_wav_insert_group(struct target_query_msg *msg)
{
    struct target_query_msg *head = NULL;
    if (!msg) {
        rt_log_error(ERRNO_INVALID_ARGU, "null pointer");
        return -ERRNO_INVALID_ARGU;
    }

    int g = vpm_target_valid_groups(msg->key);
    if (g < 0) {
        rt_log_error(ERRNO_FATAL, "target gourps full?");
        return -1;
    }

    rt_log_debug("Get group %d", g);
    dump_target_msg(msg);

    head = GROUP_ADDR(g);
    if (head->cnt > FILE_NUM_PER_TARGET) {
        rt_log_error(ERRNO_FATAL, "insert target wav > %d", FILE_NUM_PER_TARGET);
        return -1;
    }

    head[head->cnt].key = msg->key;
    head[head->cnt].score = msg->score;
    strncpy(head[head->cnt].name, msg->name, strlen(msg->name));
    head->cnt++; // 计数器只用在head处add

    return 0;
}

static __rt_always_inline__ int vpm_json_target_item_array_add(char **so, float *sg_score, int m,
                                     json_object *sample_array, float sec,
                                     char *oldname)
{
    json_object *array_item_node, *array_item, *array_obj;
    int i = 0;
    uint64_t   key;
    struct target_query_msg msg, *pos = NULL;
    char timestr[8] = {0};

    bzero(tqm, sizeof(tqm));
    for (i=0; i<m; i++) {
        if (sg_score[i] > TARGET_QUERY_SCORE) {
            sscanf(so[i], "%lu-%*s", &key);
            bzero(&msg, sizeof(msg));
            SET_TARGET_WAV_MSG(msg, key, sg_score[i], so[i]);
            vpm_target_wav_insert_group(&msg);
            rt_log_debug("key: %lu, so[%d]: %s", key, i, so[i]);
        }
    }

    forlist_target_groups(pos, tqm) {
        rt_log_debug("addr %p, cnt =%d", pos, pos->cnt);
        array_obj = json_object_new_object();
        array_item = json_object_new_array();

        for (i=0; i<pos->cnt; i++) {
            array_item_node = json_object_new_object();
            json_object_object_add(array_item_node, "filename", json_object_new_string(pos[i].name));
            json_object_object_add(array_item_node, "score", json_object_new_int(pos[i].score));
            snprintf(timestr, sizeof(timestr), "%.2f", sec);
            json_object_object_add(array_item_node, "time", json_object_new_double_s(sec, timestr));
            json_object_object_add(array_item_node, "uploadname", json_object_new_string(oldname));
            json_object_array_add(array_item, array_item_node);
        }
        json_object_object_add(array_obj, "item", array_item);
        json_object_array_add(sample_array, array_obj);
    }

    return 0;
}

static __rt_always_inline__ int vpm_target_query(json_object *ls, json_object *sample_array, int samples)
{
    int xerror = -1, i = 0, md_index = 0, m = 0;
    float *sg_score = NULL;
    int64_t sg_valid_models, sg_valid_models_size, sg_valid_models_total, sg_models_total;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    struct rt_vrstool_t *tool = rte->tool;
    struct modelist_t *cur_modelist = NULL;
    json_object *sample = NULL, *desc = NULL, *oldname = NULL;
    char **so = NULL, *__desc, *__oldname = NULL, sample_realpath[256] = {0};
    uint64_t    begin, end;

    sg_models_total = sg_get_upload_models_num(rte->model_dir, 1);
    cur_modelist = modelist_create(sg_models_total);
    if (!cur_modelist) {
        rt_log_error(ERRNO_FATAL, "modelist_create failed");
        return -1;
    }

    sg_models_total = 0;
    xerror = modelist_load_by_user(rte->model_dir, ML_FLG_RLD|ML_FLG_RIGN|ML_FLG_EXC_MOD_0,
        cur_modelist, &sg_valid_models, &sg_valid_models_size,
        &sg_valid_models_total, &sg_models_total, time(NULL));
    if (xerror < 0) {
        rt_log_error(ERRNO_FATAL, "modelist_load_exclude_mod_0");
        return -1;
    }

    if (cur_modelist->sg_cur_size > 0) {
        sg_score = cur_modelist->sg_score;
        m = cur_modelist->sg_cur_size;
        so = cur_modelist->sg_owner;
    } else {
        rt_log_error(ERRNO_FATAL, "cur_modelist load size = %ld", cur_modelist->sg_cur_size);
        return -1;
    }

    rt_log_debug("After load mod..., samples=%d", samples);

    for (i=0; i<samples; i++) {
        begin = rt_time_ms();
        sample = json_object_array_get_idx(ls, i);
        if (unlikely(!sample)) {
            rt_log_error(ERRNO_FATAL, "json_object_array_get_idx get sample null");
            return -1;
        }
        desc = web_json_to_field(sample, "filename");
        if (unlikely(!desc))
            continue;
        oldname = web_json_to_field(sample, "uploadname");
        if (unlikely(!oldname))
            continue;
        __desc = (char *)json_object_get_string(desc);
        __oldname = (char *)json_object_get_string(oldname);
        snprintf(sample_realpath, sizeof(sample_realpath), "%s/%s", rte->targetmatch_dir, __desc);
        rt_log_debug("before match...");
        xerror = tool->voice_recognition_advanced_ops(sample_realpath, cur_modelist,
            sg_score, &md_index, W_ALAW);
        if ((0 == xerror) && (md_index < m)) {
            rt_log_notice("max score %f", sg_score[md_index]);
            end = rt_time_ms();
            vpm_json_target_item_array_add(so, sg_score, m, sample_array, (double)(end - begin) / 1000, __oldname);
        } else {
            rt_log_error(ERRNO_FATAL, "voice_recognition_advanced_ops error, ret = %d, md_index = %d m = %d\n",
                xerror, md_index, m);
        }
    }
    modelist_destroy (cur_modelist);

    return xerror;
}

static __rt_always_inline__ int vpm_json_target_query(struct json_hdr __attribute__((__unused__))*jhdr,
                                       json_object *msg_body, json_object *msg_obody)
{
    json_object *ls, *sample_array;
    int samples = 0;

    ls = web_json_to_field(msg_body, "ls");
    samples = json_object_array_length(ls);

    sample_array = json_object_new_array();
    vpm_target_query(ls, sample_array, samples);
    json_object_object_add(msg_obody, "ls", sample_array);

    return 0;
}

static void *SGTargetMatch (void *param)
{
    const char *jstr = NULL;
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    message    data = NULL;
    int    s = 0;
    struct json_hdr jhdr;
    json_object *head, *injson,*msg_i_body, *msg_o_body;

    rte = (struct vrs_trapper_t    *)param;
    vpm = rte->vpm;

    FOREVER {
       data = NULL;
       /** Recv from internal queue */
       rt_mq_recv (vpm->target_mq, &data, &s);
       if (likely (data)) {
            rt_log_info ("%s", (char *)data);

            memset(&jhdr, 0, sizeof(struct json_hdr));
            web_json_parser (data, &jhdr, &injson);

            msg_i_body = web_json_to_field (injson, "msg");
            msg_o_body = json_object_new_object();
            head = json_object_new_object();

            if (SG_X_TARGET_QUERY == jhdr.cmd) {
                vpm_json_target_query(&jhdr, msg_i_body, msg_o_body);
            } else {
                rt_log_error(ERRNO_FATAL, "cmd = %d", jhdr.cmd);
            }

            web_json_head_add (head, &jhdr, msg_o_body);
            jstr = json_object_to_json_string (head);
            json_send (vpm->web_sock, jstr, strlen(jstr), web_json_data_rebuild);

            json_object_put (injson);
            json_object_put (msg_o_body);
            json_object_put (head);
            kfree (data);
       }
    }

    task_deregistry_id (pthread_self());

    return NULL;
}

static __rt_always_inline__ int  vpm_json_register(int web_sock, uint64_t no)
{
    char *regstr;
    struct json_hdr jhdr;
    struct json_object *object, *msg;

    object = json_object_new_object();
    msg = json_object_new_object();

    jhdr.ver = 1;
    jhdr.dir = 1;
    jhdr.cmd = SG_X_REG;
    jhdr.seq = no;

    json_object_object_add (msg, "program", json_object_new_string("vrs"));
    web_json_head_add (object, &jhdr, msg);

    regstr = (char *)json_object_to_json_string(object);
    json_send (web_sock, regstr, strlen(regstr), web_json_data_rebuild);

    json_object_put (object);
    json_object_put (msg);

    return 0;
}


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

static __rt_always_inline__ void vpm_check_env (struct vrs_trapper_t *rte)
{
    if(likely(rte)) {

        struct vpm_t    *vpm;
        vpm = rte->vpm;
        rt_log_notice ("*** Basic Configuration of VPM(%d) ", vpm->id);
        rt_log_notice ("*** VRS BOOST is %s", rte->vrs_boost_st ? "ON" : "OFF");
        rt_log_notice ("        TOPX is %s(%d)", rte->topx_switch ? "ON" : "OFF", rte->topx_num);
        rt_log_notice ("        MQ: (Notifier=%lu, Regular=%lu, Category=%lu, Target=%lu)", vpm->notify_mq, vpm->regular_mq, vpm->mass_mq, vpm->target_mq);
        rt_log_notice ("        VPM<-->VPW: (%s:%d)", vpm->ip, vpm->port);
        rt_log_notice ("        VPM<-->WEB: (%s:%d)", vpm->web, vpm->web_port);

        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Wave", rte->sample_dir, rt_dir_exsit (rte->sample_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Mass", rte->mass_dir, rt_dir_exsit (rte->mass_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Comp", rte->category_dir, rt_dir_exsit (rte->category_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Model", rte->model_dir, rt_dir_exsit (rte->model_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "VDU", rte->vdu_dir, rt_dir_exsit (rte->vdu_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Target", rte->targetmatch_dir, rt_dir_exsit (rte->targetmatch_dir) ? "ready" : "not ready");
        rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "Threshold_Path_Name", rte->thr_path_name, rt_file_exsit (rte->thr_path_name) ? "ready" : "not ready");

        rt_log_notice ("*** Basic Configuration of Hit-Second");
        rt_log_notice ("        Hit-Second-en: %d", rte->hit_scd_conf.hit_second_en);
        rt_log_notice ("        Update-Score: %d", rte->hit_scd_conf.update_score);
        rt_log_notice ("        Hit-Score: %d", rte->hit_scd_conf.hit_score);
        rt_log_notice ("        Topn-Max: %d", rte->hit_scd_conf.topn_max);
        rt_log_notice ("        Topn-Min: %d", rte->hit_scd_conf.topn_min);
        rt_log_notice ("        TOPN-DIR: %s", rte->hit_scd_conf.topn_dir);
    }
}


/****************************************************************************
 函数名称  : vpm_check_topn_callid_exist
 函数功能    : 检查topn当前的callid是否已经被记录
 输入参数    : query_msg, vpw上传的msg; msg_arr, 全局topn中查询到的msg数组
 输出参数    : inx,已经存在的callid的索引
 返回值     : 1,重复;0,不重复;-1,报错
 创建人     : wangkai
 备注      :
****************************************************************************/
static int vpm_check_topn_callid_exist(topn_msg_t *query_msg,
        topn_msg_t *msg_arr, int cnt, int *inx)
{
    int i;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    if (!conf || cnt>conf->topn_max || !query_msg || !msg_arr || !inx) {
        rt_log_error(ERRNO_FATAL, "cnt:%d > %d ? ", cnt, conf->topn_max);
        return -1;
    }

    for (i=0; i<cnt; i++) {
        if (query_msg->callid == msg_arr[i].callid) {
            *inx = i;
            return 1;
        }
    }
    return 0;
}

static int vpm_fill_topn_msg_arr(topn_msg_t *arr, int inx, topn_msg_t *msg)
{
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    if ( !arr || !msg || !conf || (inx>=conf->topn_max) )
        return -ERRNO_INVALID_ARGU;

    arr[inx].callid = msg->callid;
    arr[inx].dir    = msg->dir;
    arr[inx].score  = msg->score;

    return 0;
}

static void vpm_handle_topn_query(struct tlv *msg)
{
    uint64_t tid = 0;
    int xerror = 0, sz = 0;
    int cnt = 0, min_inx = 0, new_cnt = 0;
    topn_msg_t *msg_arr = NULL;
    topn_msg_t query_msg = {0};
    struct tlv request   = {0};
    struct vrs_trapper_t    *rte;
    struct vpm_t    *vpm;
    hit_second_conf_t *conf = &(vrs_default_trapper()->hit_scd_conf);

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    /* kmalloc: MPF_CLR会清内存 */
    msg_arr = (topn_msg_t *)kmalloc(sizeof(topn_msg_t) * conf->topn_max, MPF_CLR, -1);
    if (unlikely(NULL == msg_arr))
    {
        rt_log_error(ERRNO_MEM_ALLOC, "kmalloc");
        return;
    }
    memset(&query_msg, 0, sizeof(query_msg));
    memset(&request, 0, sizeof(request));

    xerror = senior_req_topnquery_to_topnmsg(msg, &tid, &query_msg);
    if (xerror < 0)
    {
        rt_log_error(ERRNO_FATAL, "sscanf error");
        kfree(msg_arr);
        return;
    }
    rt_log_debug("senior_req_topnquery_to_topnmsg ok");
    xerror = senior_topn_get(tid, msg_arr, &cnt, &min_inx);  // 获取全局topn
    rt_log_debug("senior_topn_get ok");
    // 本地topn比较
    if (xerror == 0 && likely(cnt <= conf->topn_max))
    {
        new_cnt = cnt;
        if (query_msg.score > msg_arr[min_inx].score)
        {
            // 比全局TOPN最小的大
            rt_log_notice("query score: %d, global topn score: %d",
                    query_msg.score, msg_arr[min_inx].score);
            int inx = 0;
            if ((cnt > 0) && vpm_check_topn_callid_exist(&query_msg, msg_arr, cnt, &inx) > 0)
            {
                min_inx = inx;
            }
            else
            {
                new_cnt = cnt<conf->topn_max ? cnt+1 : conf->topn_max;
            }
            rt_log_notice("cnt = %d, new_cnt = %d", cnt, new_cnt);
            if (0 == vpm_fill_topn_msg_arr(msg_arr, min_inx, &query_msg))
            {
                senior_update_topn_fulltext(tid, msg_arr, new_cnt);
            }
            else
            {
                new_cnt = cnt;
            }
        }
        sz = senior_upd_topn_to_req(tid, msg_arr, new_cnt, &request);
        void *clone = NULL;
        message_clone ((char *)&request, sz, &clone);
        if (MQ_SUCCESS != rt_mq_send (vpm->notify_mq, clone, sz))
        {
            kfree (clone);
        }
    }
    else
    {
        rt_log_error(ERRNO_FATAL, "senior_topn_get error");
    }

    kfree(msg_arr);
    return;
}

static void vpm_handle_update_stat(struct tlv *msg)
{
    void *clone = NULL;
    struct counter_upload_t _this;
    struct vpm_t    *vpm;
    struct vrs_trapper_t *rte;

    rte = vrs_default_trapper ();
    vpm = rte->vpm;

    memset(&_this, 0, sizeof(_this));

    sscanf(msg->v, "%d, %[^,], %d, %d", &(_this.vpw_id), _this.tm, &(_this.matched_sum), &(_this.hitted_sum));


    /*使用异步方式，将解析后的内容发往消息队列，消息队列的      出口专门进行数据库的写入操作*/
    message_clone((void *)&_this, sizeof(_this), &clone);
    if (MQ_SUCCESS != rt_mq_send (vpm->cdb_mq, clone, sizeof(_this)))
    {
        rt_log_error(ERRNO_FATAL, "insert counter error");
        kfree(clone);
        clone = NULL;
    }
    return ;
}

static void vpm_handle_msg(struct tlv *msg)
{
    switch (msg->t)
    {
        case req_topn_query:
            vpm_handle_topn_query(msg);
            break;
        case req_update_stat:
            vpm_handle_update_stat(msg);
            break;

        default:
            rt_log_warning(ERRNO_WARNING, "invalid tag(%d)", msg->t);
            break;
    }
    return ;
}

static void *
vpwTask (void *args)
{
    struct sockaddr_in sock_addr;
    char buffer[1024] = {0};
    int xerror = 0, tmo, sz = 0;
    struct vpm_t    *vpm;
    struct vpw_clnt_t *clnt;
    struct tlv request   = {0};
    struct rt_task_t *cur_task = task_query_id (pthread_self());

    clnt = (struct vpw_clnt_t *)args;

    if (!clnt || clnt->sock <= 0 ||
        rt_sock_getpeername(clnt->sock, &sock_addr) < 0){
        return NULL;
    }

    if (likely(cur_task))
        rt_log_notice (
            "Task (\"%s\", thread_id=%ld) starting ...",
                cur_task->name, cur_task->pid);

    vpm = clnt->vpm;
    FOREVER {
        xerror = is_sock_ready (clnt->sock, 30 * 1000000, &tmo);
        if (xerror == 0 && tmo == 1)
            continue;
        if (xerror < 0)
            break;

        memset(buffer, 0, sizeof(buffer));
        sz = rt_sock_recv (clnt->sock, buffer, 1024);
        if (sz <=0) {
            rt_log_warning(ERRNO_SG,
                    "Peer(VPW) (%s:%d, sock=%d, rsize=%d), %s",
                        inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), clnt->sock, sz,
                        (sz == 0) ? "Connection closed" : strerror (errno));

            break;
        }

        rt_log_debug("get request!");
        if (sz > 0) {
            /** 当前vpw发送到vpm的消息，只有一个tlv结构 */
            xerror = senior_parse_tlv(buffer, sz, &request);
            if (0 == xerror)
            {
                rt_log_debug("parse tlv from vpw ok");
                vpm_handle_msg(&request);
            }
        }
    }

    vpw_list_query (vpm, clnt->sock, vpw_del, NULL, 0);

    if (likely(cur_task))
        rt_log_notice (
            "Task (\"%s\", thread_id=%ld) destroy ...",
                cur_task->name, cur_task->pid);

    task_deregistry_id (pthread_self());

    return NULL;
}


void *WebAgent (void *args /** Config from vpm.yaml */)
{
#define    MSG_SIZE    4096
    struct vrs_trapper_t *rte;
    char request[MSG_SIZE] = {0};
    struct vpm_t    *vpm;
    int    rsize = 0, xerror, tmo;
    struct json_hdr hdr;
    json_object *injson;

    rte = (struct vrs_trapper_t *)args;
    vpm = rte->vpm;

    FOREVER {
        do {
            atomic64_inc(&vpm->seq);
            vpm->web_sock = (rt_clnt_sock (0, vpm->web, vpm->web_port, AF_INET));
            if (vpm->web_sock > 0)
                break;
            rt_log_notice ("Connecting to WEB (%s:%d, sock=%d): %s",
                        vpm->web, vpm->web_port, vpm->web_sock, "failure");
            sleep (3);

        } while (vpm->web_sock < 0);


        rt_log_notice ("Connecting to WEB  (%s:%d, sock=%d): %s",
                        vpm->web, vpm->web_port, vpm->web_sock, "success");
        vpm_json_register (vpm->web_sock, atomic64_read(&vpm->seq));

        do {

            xerror = is_sock_ready (vpm->web_sock, 60 * 1000000, &tmo);
            if (xerror == 0 && tmo == 1)
                continue;
            if (xerror < 0) {
                rt_log_error (ERRNO_SG, "Select (%s:%d, sock=%d): %s",
                    vpm->ip, vpm->port, vpm->web_sock, strerror(errno));
                rt_sock_close (&vpm->sock, NULL);
                break;
            }

            memset(request, 0, MSG_SIZE);
            rsize = rt_sock_recv (vpm->web_sock, request, MSG_SIZE);
            if (rsize <= 0) {
                rt_log_error (ERRNO_SG,
                    "Peer(WEB) (%s:%d, sock=%d, rsize=%d), %s",
                            vpm->ip, vpm->port, vpm->web_sock, rsize,
                            (rsize == 0) ? "Connection closed" : strerror (errno));
                rt_sock_close (&vpm->web_sock, NULL);
                break;

            }

            web_json_parser (request, &hdr, &injson);
            mission_dispatcher (vpm, &hdr, request, rsize);

        }while (vpm->web_sock > 0);
    }

    task_deregistry_id(pthread_self());

    return NULL;
}

static void *VpwNotifier (void *param)
{
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    message    data = NULL;
    int    s = 0;

    rte = (struct vrs_trapper_t    *)param;
    vpm = rte->vpm;

    FOREVER {
        data = NULL;
        /** Recv from internal queue */
        rt_mq_recv (vpm->notify_mq, &data, &s);
        if (likely (data)) {
            vpw_list_foreach (vpm, vpw_notifier, data, (size_t)s);
            kfree (data);
        }

    }

    task_deregistry_id (pthread_self());

    return NULL;
}


static void *VpwManager (void *param)
{
    int sock, xerror = 0, tmo = 0;
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    char    desc[32] = {0};

    rte = (struct vrs_trapper_t    *)param;
    vpm = rte->vpm;

    INIT_LIST_HEAD (&vpm->clnt_socks_list);

    FOREVER {

        do {
            vpm->serv_sock = rt_serv_sock (0, vpm->port, AF_INET);
            if (vpm->serv_sock > 0) {
                break;
            }
            rt_log_notice ("Listen (port=%d, sock=%d), %s",
                        vpm->port, vpm->serv_sock, "failure");
            sleep (3);
        } while (vpm->serv_sock < 0);

        rt_log_notice ("Ready to Listen (port=%d, sock=%d)",
                        vpm->port, vpm->serv_sock);

        do {

            xerror = is_sock_ready (vpm->serv_sock, 30 * 1000000, &tmo);
            if (xerror == 0 && tmo == 1)
                continue;
            if (xerror < 0) {
                /** clear clnt list here */
                vpw_list_release (vpm);
                rt_sock_close (&vpm->serv_sock, NULL);
                break;
            }

            sock = rt_serv_accept (0, vpm->serv_sock);
            if (sock <= 0) {
                rt_log_error(ERRNO_SOCK_ACCEPT, "%s\n", strerror(errno));
                continue;
            }
            struct vpw_clnt_t *clnt;
            clnt = vpw_list_add (vpm, sock);
            if (likely(clnt)) {
                sprintf (desc, "vpw%d Task", clnt->sock);
                task_spawn(desc, 0, NULL, vpwTask, clnt);
                task_detail_foreach ();
            }
        } while (vpm->serv_sock > 0);
    }

    task_deregistry_id (pthread_self());

    return NULL;
}

static void *VpmMassCategorier (void *param)
{
    const char *jstr = NULL;
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    message    data = NULL;
    int    s = 0;
    int send_data_len1 = 0 ,send_data_len = 0 ,send_len = 0;
    char *send_data = NULL, *send_data1 = NULL;
    struct json_hdr jhdr;
    json_object *head, *injson,*msg_i_body, *msg_o_body;

    rte = (struct vrs_trapper_t    *)param;
    vpm = rte->vpm;

    FOREVER {
        data = NULL;
        /** Recv from internal queue */
        rt_mq_recv (vpm->mass_mq, &data, &s);
        if (likely (data)) {
            rt_log_info ("%s", (char *)data);

            web_json_parser (data, &jhdr, &injson);
            msg_i_body = web_json_to_field (injson, "msg");

            msg_o_body = json_object_new_object();
            head = json_object_new_object();
            switch (jhdr.cmd) {
                case SG_X_MASS_QUERY:
                    vpm_json_mass_query (&jhdr, msg_i_body, msg_o_body);
                    web_json_head_add (head, &jhdr, msg_o_body);
                    break;
                default:
                    break;
            }
            jstr = json_object_to_json_string (head);
            send_data_len = strlen(jstr);
            send_data = (char*)malloc((send_data_len / 1024 + 1) * 1024);
            send_data1 = send_data;
            bzero(send_data, ((send_data_len / 1024 + 1) * 1024));
            snprintf(send_data, ((send_data_len / 1024 + 1) * 1024), "%s\n", jstr);
            send_data_len1 = strlen(send_data);
            while(send_data_len1 > 0){
                send_len = rt_sock_send (vpm->web_sock, send_data,send_data_len1);
                send_data = send_data +send_len;
                send_data_len1 -= send_len;
            }

            web_json_tokener_parse ("OUTBOUND", jstr);
            jstr = NULL;
            json_object_put (injson);
            json_object_put (msg_o_body);
            //json_object_put (msg_i_body);
            json_object_put (head);
            kfree (data);
            kfree(send_data1);
        }
    }

    task_deregistry_id (pthread_self());

    return NULL;
}

static void *SGRegularyPT (void *param)
{

    const char *jstr = NULL;
    struct    vpm_t    *vpm;
    struct    vrs_trapper_t    *rte;
    message    data = NULL;
    int    s = 0;
    struct json_hdr jhdr;
    json_object *head, *injson,*msg_i_body, *msg_o_body;

    rte = (struct vrs_trapper_t    *)param;
    vpm = rte->vpm;

    FOREVER {
        data = NULL;
        /** Recv from internal queue */
        rt_mq_recv (vpm->regular_mq, &data, &s);
        if (likely (data)) {

            rt_log_info ("%s", (char *)data);

            web_json_parser (data, &jhdr, &injson);
            msg_i_body = web_json_to_field (injson, "msg");

            msg_o_body = json_object_new_object();
                head = json_object_new_object();

            switch (jhdr.cmd) {
                case SG_X_ADD:
                    if (rte->vrs_boost_st) {
                        boost_vpm_json_add_sample(&jhdr, msg_i_body, msg_o_body);
                    }
                    else {
                        vpm_json_add_sample (&jhdr, msg_i_body, msg_o_body);
                    }
                    break;
                case SG_X_DEL:
                    if (rte->vrs_boost_st) {
                        boost_vpm_json_del_sample(&jhdr, msg_i_body, msg_o_body);
                    }
                    else {
                        vpm_json_del_sample (&jhdr, msg_i_body, msg_o_body);
                    }
                    break;
                case SG_X_CATE:
                    vpm_json_cate_sample(&jhdr, msg_i_body, msg_o_body);
                    break;
                default:
                    break;
            }

            web_json_head_add (head, &jhdr, msg_o_body);
            jstr = json_object_to_json_string (head);
            json_send (vpm->web_sock, jstr, strlen(jstr), web_json_data_rebuild);

            json_object_put (injson);
            json_object_put (msg_o_body);
            json_object_put (head);
            kfree (data);
        }
    }

    task_deregistry_id (pthread_self());

    return NULL;
}


static void SGVpmTmrScanner(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char __attribute__((__unused__))**argv)
{

    struct vrs_trapper_t *rte;

    rte = (struct vrs_trapper_t *)argv;

    rt_logging_reinit_file (rte->log_dir);
}


static struct rt_task_t SGVpwManagerTask =
{
    .module = THIS,
    .name = "VPW Management Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = VpwManager,
};

static struct rt_task_t SGWebAgentTask =
{
    .module = THIS,
    .name = "Web Agent Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = WebAgent,
};

static struct rt_task_t SGVpwNotifierTask =
{
    .module = THIS,
    .name = "VPW Cluster Notification Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = VpwNotifier,
};

static struct rt_task_t SGMassCategoryTask =
{
    .module = THIS,
    .name = "SG Massive Gategory Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = VpmMassCategorier,
};

static struct rt_task_t SGRegularTask =
{
    .module = THIS,
    .name = "SG Regular Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGRegularyPT,
};

static struct rt_task_t SGTargetMatchTask =
{
    .module = THIS,
    .name = "SG Target Match task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGTargetMatch,
};

static void vpm_argv_parser(char **argv, struct vpm_t *vpm)
{
    int i;

    for (i = 0; argv[i] != NULL; i++){
        /** Serial number parser */
        if (!STRCMP (argv[i], "--sn")){
            if (argv[++i] != NULL) {
                sscanf(argv[i], "%d", &vpm->serial_num);
                continue;
            }
            rt_log_error(ERRNO_INVALID_VAL, "Serial number is not set");
            goto finish;
        }
        /** decoder configuration parser */
        if (!STRCMP(argv[i], "--config")){
            if (argv[++i] != NULL){
                continue;
            }
            rt_log_error(ERRNO_INVALID_VAL, "Vpm configuration is not set");
            goto finish;
        }
    }
finish:
    return;
}

static void __engine_init_tool ()
{
    struct rt_vrstool_t *tool = (struct rt_vrstool_t *)kmalloc(sizeof (struct rt_vrstool_t), MPF_CLR, -1);
    if (unlikely(!tool)) {
        printf ("Can not allocate memory for vrs tool\n");
        return;
    }

    memcpy (tool, engine_default_ops(), sizeof (struct rt_vrstool_t));

    vrs_trapper_set_tool (tool);
}


static void
vpm_trapper_init(struct vrs_trapper_t *rte)
{
    struct vpm_t    *vpm;

    if(likely(rte)) {

        __sg_check_and_mkdir(rte->sample_dir);
        __sg_check_and_mkdir(rte->mass_dir);
        __sg_check_and_mkdir(rte->category_dir);
        __sg_check_and_mkdir(rte->model_dir);
        __sg_check_and_mkdir(rte->tmp_dir);
        __sg_check_and_mkdir(rte->vdu_dir);
        __sg_check_and_mkdir(rte->targetmatch_dir);

        rte->perform_tmr = tmr_create (SG,
                "Performence Monitor for VPM", TMR_PERIODIC,
                SGVpmTmrScanner, 1, (char **)rte, 30);

        if (rte->hit_scd_conf.hit_second_en) {
            __sg_check_and_mkdir(rte->hit_scd_conf.topn_dir);
            // 新增定时刷磁盘的任务，但是有一定的风险，会有学到的topn没有刷到磁盘
            rte->topn_disc_tmr = tmr_create (SG,
                    "TOPN Disc Write for VPM", TMR_PERIODIC,
                    senior_save_topn_bucket_disc, 1, (char **)rte, 3);
        }

        cdr_flags_set_bit (CDR_FLGS_CONN_BIT, 0);
        vpm_flags_set_bit (VPM_FLGS_CONN_BIT, 0);


        vpm = rte->vpm;
        if (likely (vpm)) {
            vpm->notify_mq    =    rt_mq_create ("VPM Notifier Queue");
            vpm->regular_mq    =    rt_mq_create ("VPM Regular Queue");
            vpm->mass_mq    =    rt_mq_create ("VPM Massive Query Queue");
            vpm->boost_mq   =   rt_mq_create ("VPM Boost Queue");
            vpm->cdb_mq     =   rt_mq_create ("VPM DB Queue");
            vpm->target_mq  =   rt_mq_create ("VPM Target Queue");
        }
    }
}

void vpm_init (int __attribute__((__unused__)) argc,
               char **argv, struct vrs_trapper_t *rte)
{
    /** read config before do rte init */
    vpm_conf_init ();

    vpm_argv_parser(argv, rte->vpm);

    __engine_init_tool ();

    /** rte init */
    vpm_trapper_init (rte);

    task_registry (&SGMassCategoryTask);
    task_registry (&SGRegularTask);
    task_registry (&SGVpwManagerTask);
    task_registry (&SGWebAgentTask);
    task_registry (&SGVpwNotifierTask);
    task_registry (&SGTargetMatchTask);

}

void vpm_startup (struct vrs_trapper_t *rte)
{
    struct vrs_trapper_t *trapper = vrs_default_trapper();

    tmr_start (trapper->perform_tmr);

    vpm_check_env (rte);

    rte->tool->engine_init ("SpkSRE.cfg", "/usr/local/etc/vpm");
}

int main (int argc, char **argv)
{
    struct vrs_trapper_t *rte;

    librecv_init (argc, argv);

    rte  = vrs_default_trapper();

    vpm_init (argc, argv, rte);

    if (rte->vrs_boost_st) {
        vpm_boost_init();
    }

    vpm_dms_init();

    vpm_startup (rte);

    /** 本地TOPN加载到内存 */
    if (rte->hit_scd_conf.hit_second_en) {
        senior_topn_table_init();
        senior_load_topns(rte->hit_scd_conf.topn_dir);
        tmr_start (rte->topn_disc_tmr);
    }

    task_run ();

    FOREVER {
        sleep (1000);
    }


    return 0;
}
