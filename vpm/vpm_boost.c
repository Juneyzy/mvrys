#include "sysdefs.h"
#include <json/json.h>
#include "vrs.h"
#include "json_hdr.h"
#include "vpm_dms_agent.h"
#include "vpm_init.h"
#include "vpm_boost.h"
#include "conf.h"
#include "conf-yaml-loader.h"
#include "apr_md5.h"
#include "apr_xlate.h"
#include "apr_general.h"


//--------------------------------------------------------------------------------------
// Chap1. 样本合并, 阈值计算

void json_sample_redunant(char *pathname, json_object *arr)
{
    json_object *obj;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    obj = json_object_new_object();
    json_object_object_add(obj, "filename",
    json_object_new_string(pathname+strlen(rte->sample_dir)+1));
    json_object_object_add(obj, "status", json_object_new_int(add_st_redundant));
    json_object_array_add(arr, obj);
}

static int md5sum (char *file, unsigned char digest[])
{
    apr_md5_ctx_t context;

    char readline[1024] ={0};
    FILE *fp = NULL;

    apr_md5_init(&context);

    fp = fopen(file, "r");
    if (likely (fp)) {
        while (fgets(readline, sizeof(readline), fp)) {
            apr_md5_update(&context, readline, strlen(readline));
            memset (readline, 0, sizeof(readline));
        }

        fclose (fp);
    }

    apr_md5_final(digest, &context);

    return 0;
}

static int
list_target_all_wav(uint64_t tid, OUT char **wavlist, int list_n)
{
    DIR *pDir = NULL;
    struct dirent *ent = NULL;
    int  cnt = 0, vid = 0;
    uint64_t file_tid = 0;
    struct vrs_trapper_t *rte = vrs_default_trapper();

    if (!wavlist) {
        rt_log_error(ERRNO_INVALID_ARGU, "NULL POINTER");
        return -1;
    }

    pDir = opendir(rte->sample_dir);
    if (unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL,
                "%s, %s", strerror(errno), rte->sample_dir);
        return -1;
    }

    while ((ent = readdir(pDir)) != NULL) {
        if (!strcmp(ent->d_name, ".") ||
            !strcmp(ent->d_name, ".."))
            continue;

        file_tid = 0;
        if (2 != sscanf(ent->d_name, "%lu-%d.%*s", &file_tid, &vid))
        {
            continue;
        }
        if(file_tid != tid)
        {
            continue;
        }

        if (cnt >= list_n) {
            rt_log_notice("wav actual num[%d] > %d", cnt, list_n);
            break;
        }
        rt_log_debug("!!!%d!!!", cnt);
        snprintf(wavlist[cnt], FILE_NAME_LEN, "%s/%s", rte->sample_dir, ent->d_name);
        cnt++;
    }

    closedir(pDir);

    return cnt;
}

/**
 *    描述：根据两两之间的md5值，找出多余的样本并删除
 */
int
check_redundant_target_wav(uint64_t tid, json_object *arr)
{
    int xret = -1;
    int i, j, wavs;
    char *wavlist[FILE_NUM_PER_TARGET] = {NULL};
    unsigned char *digest[FILE_NUM_PER_TARGET] = {NULL};

    for (i=0; i<FILE_NUM_PER_TARGET; i++) {
        wavlist[i] = (char *)kmalloc(FILE_NAME_LEN, MPF_CLR, -1);
        digest[i] = (unsigned char *)kmalloc(FILE_NAME_LEN, MPF_CLR, -1);
        if (!wavlist[i] || !digest[i])
            goto finish;
    }

    rt_log_debug("Enter check_redundant_target_wav");
    wavs = list_target_all_wav(tid, wavlist, FILE_NUM_PER_TARGET);
    if (wavs < 0)
        goto finish;

    rt_log_debug("After list_target_all_wav");
    for (i=0; i<wavs; i++) {
        md5sum(wavlist[i], digest[i]);
    }

    for (i=0; i<wavs; i++) {
        for (j=i+1; j<wavs; j++) {
            if (!memcmp(digest[i], digest[j], APR_MD5_DIGESTSIZE)) {
                remove(wavlist[j]);
                rt_log_notice("get file \"%s\" and \"%s\" md5 same, remove %s",
                        wavlist[i], wavlist[j], wavlist[j]);
                json_sample_redunant(wavlist[j], arr);
            }
        }
    }
    xret = 0;

finish:
    for (i=0; i<FILE_NUM_PER_TARGET; i++) {
        kfree(wavlist[i]);
        kfree(digest[i]);
    }

    return xret;
}


/****************************************************************************
 函数名称  : vpm_check_current_file_id
 函数功能    : 检查当前web传的wav在wavlist数组中的id
 输入参数    : wavname，上传语音的完整路径名; wavlist,导入到内存的wav名字的数组
 输出参数    : 无
 返回值     : -1, 没有找到(异常);其他对应的数组中的id
 创建人     : wangkai
 备注      :
****************************************************************************/
static int
vpm_check_current_file_id(const char *wavname, char **wavlist, int cnt)
{
    int i;
    for (i=0; i<cnt; i++) {
        if (!strncmp(wavlist[i], wavname, strlen(wavname)))
            return i;
    }

    return -1;
}


/****************************************************************************
 函数名称  : check_current_file_redundant
 函数功能    : 检查当前web上传的wavname是重复
 输入参数    : wavname，上传语音的完整路径名
 输出参数    : 无
 返回值     : 1-重复上传, 其他值不重复或者异常
 创建人     : wangkai
 备注      :
****************************************************************************/
int
check_current_file_redundant(uint64_t tid, char *wavname, json_object *arr)
{
    int xret = -1, id;
    int i, wavs;
    char *wavlist[FILE_NUM_PER_TARGET] = {NULL};
    unsigned char *digest[FILE_NUM_PER_TARGET] = {NULL};

    for (i=0; i<FILE_NUM_PER_TARGET; i++) {
        wavlist[i] = (char *)kmalloc(FILE_NAME_LEN, MPF_CLR, -1);
        digest[i] = (unsigned char *)kmalloc(FILE_NAME_LEN, MPF_CLR, -1);
        if (!wavlist[i] || !digest[i])
            goto finish;
    }

    rt_log_debug("Enter check_redundant_target_wav");
    wavs = list_target_all_wav(tid, wavlist, FILE_NUM_PER_TARGET);
    if (wavs < 0)
        goto finish;

    id = vpm_check_current_file_id(wavname, wavlist, wavs);
    if (id < 0) {
        rt_log_error(ERRNO_FATAL, "current file id not found");
        goto finish;
    }

    rt_log_debug("current file id = %d", id);
    rt_log_debug("After list_target_all_wav");
    for (i=0; i<wavs; i++) {
        md5sum(wavlist[i], digest[i]);
    }

    for (i=0; i<wavs; i++) {
        if  (i != id) {
            if (!memcmp(digest[id], digest[i], APR_MD5_DIGESTSIZE)) {
                remove(wavname);
                json_sample_redunant(wavname, arr);
                rt_log_notice("file %s and update file %s md5sum same, remove it",
                        wavlist[i], wavname);
                xret = 1;
                break;
            }
        }
    }

finish:
    for (i=0; i<FILE_NUM_PER_TARGET; i++) {
        kfree(wavlist[i]);
        kfree(digest[i]);
    }

    return xret;

}


static int calc_wav_average_score(float *sg_score, int m)
{
    int i;
    float total = 0;

    if (0 == m)
        return 0;

    for (i=0; i<m; i++) {
        total += sg_score[i];
    }

    return (int)(total/m);
}

static int calc_wav_min_score(float *sg_score, int m)
{
    int i;
    float min = sg_score[0];

    for (i=1; i<m; i++) {
        if (min > sg_score[i])
            min = sg_score[i];
    }
    return (int)min;
}

struct average_msg {
    int aver, min;
};


/****************************************************************************
 函数名称  : vpm_bth_default_filter
 函数功能    : 均衡模式阈值过滤
 输入参数    : min_value, 得到的高于X分值最高的三个的样本匹配的最低得分
 输出参数    : 无
 返回值     : 过滤得到的均衡模式阈值
 创建人     : wangkai
 备注      :
****************************************************************************/
static int vpm_bth_default_filter(int min_value)
{
    int retval = 0;
    vrs_boost_t *booster = default_booster();

    if (min_value < booster->threshold_cfg.default_min)
    {
        retval= booster->threshold_cfg.default_min;
    }
    else if (min_value > booster->threshold_cfg.default_max) {
        retval = booster->threshold_cfg.default_max;
    }
    else {
        retval = min_value;
    }

    return retval;
}


/****************************************************************************
 函数名称  : vpm_bth_exploring_filter
 函数功能    : 搜寻模式阈值过滤
 输入参数    : min_value, 交叉匹配的最低得分
 输出参数    : 无
 返回值     : 过滤得到的均衡模式阈值
 创建人     : wangkai
 备注      :
****************************************************************************/
static int vpm_bth_exploring_filter(int min_value, int default_score)
{
    int retval = 0;
    vrs_boost_t *booster = default_booster();

    if (min_value < booster->threshold_cfg.exploring_min)
    {
        retval= booster->threshold_cfg.exploring_min;
    }
    else if (min_value > booster->threshold_cfg.exploring_max) {
        retval = booster->threshold_cfg.exploring_max;
    }
    else {
        retval = min_value;
    }

    // 均衡模式的阈值至少比搜寻模式的阈值>=exploring_magic分
    if (default_score - retval < booster->threshold_cfg.exploring_magic) {
        retval = default_score - booster->threshold_cfg.exploring_magic;
    }

    return retval;
}

void bt_default_val(boost_threshold_t *bt)
{
    vrs_boost_t *booster = default_booster();
    bt->default_score  = booster->threshold_cfg.default_min;
    bt->exploring_score = booster->threshold_cfg.exploring_min;
    bt->accurate_score = bt->default_score+booster->threshold_cfg.accurate_magic;
}

int vpm_cate_target_samples(vrs_target_attr_t *target_info)
{
    int64_t sg_valid_models, sg_valid_models_size, sg_valid_models_total, sg_models_total;
    int xret = -1, i, m = 0, md_index = 0;
    float *sg_score = NULL;
    struct modelist_t *modelist = NULL;
    struct rt_vrstool_t *tool = NULL;
    struct vrs_trapper_t *rte = vrs_default_trapper();
    vrs_sample_attr_t *__this = NULL;

    tool = rte->tool;

    if (!target_info)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "NULL POINTER");
        goto finish;
    }

    modelist = modelist_create(target_info->sample_cnt);
    for (i=0; i<target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        rt_log_debug("path = %s, wav = %s", __this->wav_path, __this->wav_name);
        xret = modelist_delta_load_model_to_memory(target_info->root_dir, __this->wav_name, modelist,
                        ML_FLG_RIGN, &sg_valid_models, &sg_valid_models_size,
                        &sg_valid_models_total, &sg_models_total);
        if (xret < 0)
        {
            rt_log_warning(ERRNO_WARNING, "load modelist error(%s)", __this->wav_path);
            goto finish;
        }
    }
    rt_log_debug("modelist load");

    if (likely(modelist) && modelist->sg_cur_size > 0)
    {
        sg_score = modelist->sg_score;
        m = modelist->sg_cur_size;
    }
    rt_log_debug("load model num = %d", m);

    for (i=0; i<target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        xret = tool->voice_recognition_advanced_ops(__this->wav_path, modelist,
                            sg_score, &md_index, W_ALAW);
        if (xret == 0 && md_index < m)
        {
            __this->avg_score = calc_wav_average_score(sg_score, m);
            __this->min_score = calc_wav_min_score(sg_score, m);
        }
        /*
        printf("wav[%s] cate score ==>", __this->wav_path);
        for(int j = 0; j < m; j++)
            printf("%2d ", (int)sg_score[j]);
        printf("----- %2d %2d", __this->avg_score, __this->min_score);

        printf("\n");
        */
    }

    modelist_destroy(modelist);

finish:
    return xret;

}

/**
 * 描述：给当前target下所有的wav做交叉匹配,并有结果算出LESS FAR和LESS FRR, default值
 * 输入参数: sample,所有合法的输入样本的完整路径名等信息;root,sample中的完整路径名的根目录;cnt,有效样本的
 *           个数;
 * 输出参数: bt,三个阈值;outsample,合并成model的A个wav名字; msg_obody，和web交互的json信息, 是否添加json
 *           由参数json_en来控制
 * 返回：    0，正常;其他，异常
 */
static int vpm_calc_target_bth(vrs_target_attr_t *target_info)
{
    int i, j, xret = 0, cnt = 0, no_merge_cnt = 0;
    int min = 0, exploring_s = 100;
    vrs_boost_t *booster = default_booster();
    vrs_sample_attr_t *__this = NULL;
    struct average_msg msg[FILE_NUM_PER_TARGET];
    struct average_msg tmp;

    bzero(&msg, sizeof(msg));
    bzero(&tmp, sizeof(tmp));

    for (i=0; i<target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        if (__this->avg_score > 0)
        {
            msg[cnt].aver = __this->avg_score;
            msg[cnt].min = __this->min_score;
            cnt++;
            rt_log_debug("wav(%s) : %d, %d, %d, %d", __this->wav_path, __this->avg_score, __this->min_score, booster->merge_cfg.bad_score, booster->merge_cfg.merge_score);
            if (__this->avg_score < booster->merge_cfg.bad_score)
            {
                __this->merge_flag = WAV_FILE_NO_MERGE;
                __this->status = add_st_tiny_similar;
                no_merge_cnt++;
            }
            else if ((__this->avg_score >= booster->merge_cfg.bad_score) 
                      && (__this->avg_score < booster->merge_cfg.merge_score))
            {
                __this->status = add_st_poor_similar;
            }
            else
            {
                __this->status = add_st_normal;
            }
        }
        if (__this->min_score < exploring_s)
        {
            exploring_s = __this->min_score;
        }
    }

    if (no_merge_cnt == target_info->sample_cnt)
    {
        xret = -1;
        rt_log_notice("No Sample can used to merge!");
        goto finish;
    }

    /** 如果交叉匹配的样本个数小于配置的值A，直接使用默认的阈值 */
    if (cnt < booster->merge_cfg.min_num)
    {
        rt_log_notice("Valid Cate Cnt: %d", cnt);

        bt_default_val(&(target_info->bth));
        goto finish;
    }

    /** 存在大于配置的值A的样本, 做排序 */
    for (i=0; i<cnt; i++)
    {
      for (j=i+1; j<cnt; j++)
      {
          if (msg[i].aver < msg[j].aver) {
              memcpy(&tmp, &msg[i], sizeof(tmp));
              memcpy(&msg[i], &msg[j], sizeof(tmp));
              memcpy(&msg[j], &tmp, sizeof(tmp));
          }
      }
    }

    min = msg[0].min;
    for (i=1; i<booster->merge_cfg.min_num; i++)
    {
      if (min > msg[i].min)
          min = msg[i].min;
    }

    rt_log_debug("A_X min: %d", min);

    /* 由A个平均高于X分的样本的最低分作阈值 */
    target_info->bth.default_score = vpm_bth_default_filter((int)min);

    target_info->bth.exploring_score = vpm_bth_exploring_filter((int)exploring_s, target_info->bth.default_score);

    target_info->bth.accurate_score = target_info->bth.default_score + booster->threshold_cfg.accurate_magic;

finish:

    rt_log_notice("default score=%d, accurate score=%d, exploring score = %d",
                  target_info->bth.default_score, target_info->bth.accurate_score, target_info->bth.exploring_score);

    return xret;
}

int vpm_comp_analysis_samples(vrs_target_attr_t *target_info)
{
    int xret = -1;

    if (!target_info)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "NULL POINTER");
        goto finish;
    }
    xret = vpm_cate_target_samples(target_info);
    if (xret != 0)
    {
        rt_log_error(ERRNO_FATAL, "samples cate error(%d)", xret);
        goto finish;
    }
    xret = vpm_calc_target_bth(target_info);
finish:
    return xret;
}

int vpm_judge_samples_similarity(vrs_target_attr_t *target_info)
{
    int xret = -1, i = 0, avg_score = 0;
    int status = add_st_null;
    float sum_score = 0;
    vrs_sample_attr_t *__this = NULL;

    if (!target_info)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "NULL POINTER");
        goto finish;
    }

    xret = vpm_cate_target_samples(target_info);
    if (xret != 0)
    {
        rt_log_error(ERRNO_FATAL, "samples cate error(%d)", xret);
        goto finish;
    }
    /** 只有样本数为2时才会调用这个接口函数，下面的逻辑即获取两个样本相互匹配分数的平均值*/
    for (i = 0; i < target_info->sample_cnt; i++)
    {
        __this = (vrs_sample_attr_t *)(target_info->samples + i);
        sum_score = sum_score + __this->min_score;
    }
    avg_score = (int)sum_score / target_info->sample_cnt;

    if (avg_score < 35)
    {
        /** 2个样本相互匹配的平均分小于35，相似度很差 */
        status = add_st_few_similar;
    }
    else if (avg_score < 55)
    {
        /** 2个样本相互匹配的平均分小于55，相似度差 */
        status = add_st_less_similar;
    }
    else
    {
        status = add_st_normal;
    }

finish:
    return status;
}

static int g_last_three_frame[3] = {FRAME_UNKNOW, FRAME_UNKNOW, FRAME_UNKNOW};

/** 能量门限值，当前先取0x52 */
static int g_energy_gate = 0x52;

static inline int get_energy_gate()
{
    return (g_energy_gate *g_energy_gate);
}

static inline int set_energy_gate(int val)
{
    return g_energy_gate = val;
}


static int calc_frame_avg_energy(unsigned char *frame, int frame_size)
{
    int i = 0;
    int E_per = 0;
    int E_sum = 0;
    int E_avg = 0;

    for (i = 0; i < frame_size; i++)
    {
        E_per = frame[i] & 0x7f;

        E_sum += (E_per * E_per);
    }
    E_avg = E_sum / frame_size;

    return E_avg;
}

static inline int abs_e(int a, int b)
{
    if (a >= b)
    {
        return (a - b);
    }
    else
    {
        return (b - a);
    }
}

static int calc_frame_czr(unsigned char *frame, int frame_size)
{
    int i = 0;
    int sgn1 = 0, sgn2 = 0;
    int rate = 0;

    for (i = 1; i < frame_size; i++)
    {
        sgn1 = frame[i - 1] & 0x80;
        sgn2 = frame[i] & 0x80;
        if ((sgn1 != sgn2) && (abs_e(frame[i - 1] & 0x7f, frame[i] & 0x7f) >= 2))
        {
            rate += 1;
        }
    }
    return rate;
}

static inline int frame_vad_smooth()
{
    if ((g_last_three_frame[0] ==  FRAME_VOICE)
        || (g_last_three_frame[1] ==  FRAME_VOICE)
        || (g_last_three_frame[2] ==  FRAME_VOICE))
    {
        return FRAME_SMOOTH_VOICE;
    }
    else
    {
        return FRAME_SILENCE;
    }
}

static inline void update_vad_smooth(int frame_type)
{
    g_last_three_frame[2] = g_last_three_frame[1];
    g_last_three_frame[1] = g_last_three_frame[0];
    g_last_three_frame[0] = frame_type;
}

static inline void init_vad_smooth()
{
    g_last_three_frame[2] = FRAME_UNKNOW;
    g_last_three_frame[1] = FRAME_UNKNOW;
    g_last_three_frame[0] = FRAME_UNKNOW;
}

static int vpm_judge_frame_type(unsigned char *frame, int frame_size)
{
    int czr = 0;
    int energy_avg = 0;
    int frame_type = FRAME_UNKNOW;

    energy_avg = calc_frame_avg_energy(frame, frame_size);
    if (energy_avg >= get_energy_gate())
    {
        /** 平均能量大于门限值，初步判断为静音帧，进行VAD平滑 */
        frame_type = frame_vad_smooth();
        if (frame_type == FRAME_SILENCE)
        {
            /** 平滑结果仍为静音帧， 计算过零率 */
            czr = calc_frame_czr(frame, frame_size);
            if (czr >= 3)
            {
                frame_type = FRAME_VOICE; //此时可能为FRAME_SMOOTH_VOICE，待验证后再决定是否调整
            }
        }
    }
    else
    {
        frame_type = FRAME_VOICE;
    }

    //printf("frame_type = %d \n", frame_type);
    update_vad_smooth(frame_type);

    return frame_type;
}

static int vpm_get_wav_valid_len(char *fullpath)
{
    int  valid_voice_len = -1;
    int  file_size = 0, real_size = 0, valid_size = 0;
    int  frame_size = 0, frame_offset = FRAME_OFFSET, frame_type = FRAME_UNKNOW;
    unsigned char *buf= NULL, *p = NULL;
    FILE *fp = NULL;
    struct stat statbuf;

    if (stat(fullpath, &statbuf) < 0)
    {
        rt_log_error(ERRNO_FATAL, "%s : %s", fullpath, strerror(errno));
        goto finish;
    }
    file_size = statbuf.st_size;
    buf = (unsigned char *)kmalloc(file_size, MPF_CLR, -1);
    if (!buf)
    {
        rt_log_error(ERRNO_FATAL, "%s : %s", fullpath, strerror(errno));
        goto finish;
    }
    fp = fopen(fullpath, "rb");
    if (!fp)
    {
        rt_log_error(ERRNO_FATAL, "%s : %s", fullpath, strerror(errno));
        goto finish;
    }

    real_size = fread(buf, 1, file_size, fp);
    if (real_size != file_size)
    {
        rt_log_error(ERRNO_FATAL, "fread invalid(%d)", real_size);
        goto finish;
    }
    /* 处理wav文件头 */
    p = (unsigned char *)(buf + sizeof(wav_header_t));
    real_size -= sizeof(wav_header_t);
    while (real_size > 0)
    {
        frame_size = (real_size >= FRAME_LEN ? FRAME_LEN : real_size);
        frame_type = vpm_judge_frame_type(p, frame_size);

        /* 为实现平滑，采用滑动窗口，每次只偏移一个帧的2/5 */
        frame_offset = (frame_size == FRAME_LEN ? FRAME_OFFSET : frame_size);

        if (frame_type == FRAME_VOICE || frame_type == FRAME_SMOOTH_VOICE)
            valid_size += frame_offset;

        real_size -= frame_offset;
        p = (unsigned char *)(p + frame_offset);
    }

    /** 计算有效语音长度，WAV_AVG_BYTES_PERSEC这个值应该读取wav的头部获取，这里先直接用宏 */
    valid_voice_len = valid_size / WAV_AVG_BYTES_PERSEC;

    rt_log_notice("wav(%s)[%d sec ==> %d sec]", fullpath, (file_size/WAV_AVG_BYTES_PERSEC), valid_voice_len);

    /** 每次都要初始化VAD平滑记录 */
    init_vad_smooth();

finish:
    if (buf)
    {
        kfree(buf);
        buf = NULL;
    }
    if (fp)
    {
        fclose(fp);
    }

    return valid_voice_len;
}

int vpm_get_wav_status(char *fullpath)
{
    int status = add_st_null;
    int valid_len = 0;

    if (!fullpath)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "vpm_get_wav_status");
        goto finish;
    }
    valid_len = vpm_get_wav_valid_len(fullpath);
    if (valid_len < 0)
    {
        status = add_st_null;
    }
    else if (valid_len < 30)
    {
        status = add_st_less_30s;
    }
    else if (valid_len < 60)
    {
        status = add_st_less_60s;
    }
    else
    {
        status = add_st_normal;
    }

finish:
    return status;
}

void vpm_json_assembling_result(char *filepath, int status, json_object *arr)
{
    char *ptr = NULL;
    json_object *obj;

    if (NULL != (ptr = strrchr(filepath, '/')))
    {
        ptr += 1;
    }
    else
    {
        ptr = filepath;
    }

    obj = json_object_new_object();
    json_object_object_add(obj, "filename", json_object_new_string(ptr));
    json_object_object_add(obj, "status", json_object_new_int(status));
    json_object_array_add(arr, obj);

}

#define VPM_BF_TMP_DIR    "/root/boost/tmp"

static void vpm_make_tmp_dir(vrs_target_attr_t *target_info)
{
    char tmp_dir[FILE_PATH_LEN] = {0};

    snprintf(tmp_dir, FILE_PATH_LEN - 1, "%s/%lu", VPM_BF_TMP_DIR, target_info->target_id);

    rt_check_and_mkdir(tmp_dir);

    strcpy(target_info->root_dir, tmp_dir);

    return ;
}

static int __vpm_get_sample_path(char *vpu_dir, char *suffix_path, char *wav_realpath)
{
    int  ret = -1;
    DIR  *pDir;
    struct dirent *ent;

    pDir = opendir (vpu_dir);
    if(unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL, "%s, %s", strerror(errno), vpu_dir);
        return ret;
    }

    /** get sample file path */
    while ((ent = readdir(pDir)) != NULL)
    {
        if (!strcmp(ent->d_name, ".") || !strcmp (ent->d_name, ".."))
        {
            continue;
        }
        /*真实路径拼装需要与VPU的wav文件保存方式保持一致*/
        snprintf(wav_realpath, 256, "%s/%s/case/%s", vpu_dir, ent->d_name, suffix_path);
        if (rt_file_exsit (wav_realpath))
        {
            ret = 0;
            break;
        }
        else
        {
            /** 案件目录中不存在则在海量目录中查找*/
            snprintf(wav_realpath, 256, "%s/%s/normal/%s", vpu_dir, ent->d_name, suffix_path);
            if (rt_file_exsit (wav_realpath))
            {
                ret = 0;
                break;
            }
        }
    }
    closedir(pDir);

    return ret;
}

/****************************************************************************
 函数名称  : vpm_get_sample_path
 函数功能    : 根据callid和dir，找到wav语音的真实路径
 输入参数    : 1、callid  用于拼装wav文件名和路径
           2、dir  1代表up，2代表down， 用于拼装wav文件名
 输出参数    : 1、sample_realpath    wav语音的真实路径
 返回值     : 成功返回0，失败返回非0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_get_sample_path(uint64_t callid, int dir, char *wav_realpath)
{
    time_t    call_time;
    uint32_t  pos;
    uint8_t   e1_no;
    uint8_t   ts_no;
    char suffix_path[FILE_PATH_LEN] = {0};
    struct tm *tms;
    struct tm tm_buf;
    vrs_boost_t *booster = default_booster();

    if (NULL == wav_realpath)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "The Point is NULL");
        return ERRNO_INVALID_ARGU;
    }

    call_time = (callid >> 32) & 0xffffffff;
    pos = (callid >> 15) & 0x1ffff;
    e1_no = (pos >> 5) & 0x3f;
    ts_no = pos & 0x1f;

    tms = localtime_r(&call_time, &tm_buf);
    if (tms == NULL)
    {
        rt_log_error(ERRNO_INVALID_VAL, "The Time of callid is err");
        return ERRNO_INVALID_VAL;
    }
    snprintf(suffix_path, FILE_PATH_LEN - 1, "%04d-%02d-%02d/%02d/%02d/%016lx_%s",
            tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
            e1_no, ts_no, callid, (dir == 1) ? "up" : "down");

    /** 修改vpm挂载vpu的方式，将各个vpu存储语音路径挂载到配置文件
        指定目录的下一级目录上，适配A29现场环境的多个vpu场景*/
    if (0 != __vpm_get_sample_path(booster->vocfile_dir, suffix_path, wav_realpath))
    {
        rt_log_error(ERRNO_INVALID_VAL, "The wav file(%s) is not exist", suffix_path);
        return ERRNO_INVALID_VAL;
    }
    rt_log_debug("wav_realpath : %s", wav_realpath);
    return XSUCCESS;
}

/****************************************************************************
 函数名称  : vpm_decrypt_wave_data
 函数功能    : 对wav数据内容解密
 输入参数    : 1、data    2、data_len
 输出参数    :
 返回值     : 返回0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_decrypt_wav_data(char *data, int data_len)
{
    int i;

    /*解密方式与SVM的加密方式保持一致*/
    for (i = 0; i < data_len; i++) {
        data[i] = data[i] ^ 0x53;
    }

    return 0;
}


/****************************************************************************
 函数名称  : vpm_write_decrypt_wav
 函数功能    : 读取样本文件内容，解密后写入待匹配的wav文件中
 输入参数    : 1、sample_realpath    2、dst_fp
 输出参数    :
 返回值     : 返回0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_write_decrypt_wav(const char *sample_realpath, FILE *dst_fp)
{
    char buf[1024] = {0};
    int file_size = 0, real_size = 0;
    FILE *src_fp;
    struct stat statbuf;
    wav_header_t wavhead;

    if ((0 == stat(sample_realpath, &statbuf)) && (S_ISREG(statbuf.st_mode)))
    {
        file_size = statbuf.st_size;
    }
    /** A29现场bug，增加一个wav header，部分解密后的wav若无wav header，无法转换model*/
    vpm_wav_header_init(&wavhead, file_size);
    fwrite(&wavhead, 1, sizeof(wav_header_t), dst_fp);

    if (file_size)
    {
        src_fp = fopen(sample_realpath, "r");
        while ((real_size = fread(buf, 1, 1024, src_fp)) > 0)
        {
            vpm_decrypt_wav_data(buf, real_size);
            fwrite(buf, 1, real_size, dst_fp);
            file_size -= real_size;
        }
        fclose(src_fp);
    }
    if (file_size != 0)
    {
        rt_log_notice("Write merge wav size error");
        return -1;
    }
    return 0;
}

/****************************************************************************
 函数名称  : vpm_get_sample_wav
 函数功能    : 解析json参数，获取样本文件路径，将其内容解密后写入wav_file
 输入参数    : 1、ls        json消息体，需要进行解析
           2、samples   样本文件总数
 输出参数    : wav_file
 返回值     : 成功返回0，失败返回非0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_get_sample_wav(json_object *ls, int samples, char *tmp_dir)
{
    int xerror = XSUCCESS;
    int idx = 0;
    int dir = 0;
    uint64_t callid;
    FILE *fp;
    char sample_realpath[FILE_PATH_LEN] = {0};
    char tmp_realpath[FILE_PATH_LEN] = {0};
    json_object *sample, *callid_desc, *dir_desc;

    if (NULL == ls || NULL == tmp_dir)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "Param is NULL");
        xerror = ERRNO_INVALID_ARGU;
        goto finish;
    }
    for (idx = 0; idx < samples; idx++)
    {
        sample = json_object_array_get_idx(ls, idx);
        callid_desc = web_json_to_field (sample, "callid");
        dir_desc = web_json_to_field (sample, "dir");
        if (unlikely (!callid_desc) || unlikely (!dir_desc))
        {
            rt_log_notice("Json param invalid");
            continue;
        }
        callid = (uint64_t)json_object_get_int64(callid_desc);
        dir = json_object_get_int(dir_desc);

        /*获取样本文件的真实路径*/
        memset(sample_realpath, 0, FILE_PATH_LEN);
        xerror = vpm_get_sample_path(callid, dir, sample_realpath);
        if (unlikely(xerror != 0))
        {
            rt_log_notice("Sample path error");
            continue;
        }
        snprintf(tmp_realpath, FILE_PATH_LEN - 1, "%s/%lu_%d.wav", tmp_dir, callid, dir);
        fp = fopen (tmp_realpath, "a+");
        if (unlikely(!fp))
        {
            rt_log_error(ERRNO_ACCESS_DENIED, "can not open or create file(%s)", tmp_realpath);
            continue;
        }
        /*读取样本文件，进行解密后转存到临时样本目录中，计算阈值时会用到*/
        vpm_write_decrypt_wav(sample_realpath, fp);
        fclose(fp);
    }

finish:
    return xerror;
}

static int vpm_copy_orig_sample( char *sample_dir, vrs_target_attr_t *target_info)
{
    char sample_realpath[FILE_PATH_LEN] = {0};
    char cmd[256] = {0};
	int      vid = 0;
	uint64_t tid = 0;
    DIR  *pDir;
    struct dirent *ent;

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
		snprintf(sample_realpath, FILE_PATH_LEN - 1, "%s/%s", sample_dir, ent->d_name);
        rt_log_debug("sample_realpath : %s", sample_realpath);

        snprintf(cmd, 255, "cp %s %s", sample_realpath, target_info->root_dir);
        do_system(cmd);
    }
    closedir(pDir);

    return 0;
}


static int vpm_get_all_samples(vrs_target_attr_t *target_info)
{
    DIR  *pDir;
    struct dirent *ent;

    int      idx = 0;
    vrs_sample_attr_t *__this = NULL;

    pDir = opendir (target_info->root_dir);
    if(unlikely(!pDir)) {
        rt_log_error(ERRNO_FATAL, "%s, %s", strerror(errno), target_info->root_dir);
        return -1;
    }

    /** get sample file */
    while ((ent = readdir(pDir)) != NULL)
    {
        if (!strcmp(ent->d_name, ".") || !strcmp (ent->d_name, ".."))
        {
            continue;
        }
        if (NULL == strstr(ent->d_name, ".wav"))
        {
            continue;
        }
        if (idx >= FILE_NUM_PER_TARGET)
        {
            rt_log_warning(ERRNO_FATAL, " [WARNING]The file number per target is greater %d", FILE_NUM_PER_TARGET);
            break;
        }
        __this = (vrs_sample_attr_t *)(target_info->samples + idx);
        snprintf(__this->wav_name, FILE_NAME_LEN - 1, "%s", ent->d_name);
        snprintf(__this->wav_path, FILE_PATH_LEN - 1, "%s/%s", target_info->root_dir, ent->d_name);

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

int vpm_get_bth_info(vrs_target_attr_t *target_info)
{
    int xerror = 0;
    vrs_boost_t *booster = default_booster();

    xerror = vpm_get_all_samples(target_info);
    if (xerror != 0)
    {
        rt_log_error(ERRNO_FATAL, "Get all samples error");
        goto finish;
    }

    if (0 == target_info->sample_cnt)
    {
        rt_log_warning(ERRNO_WARNING, "valid sample cnt is 0");
        xerror = -1;
        goto finish;
    }
    else if (3 > target_info->sample_cnt)
    {
        /** 小于3个样本时无法通过交叉匹配确定阈值，这里直接去默认值 */
        rt_log_notice("valid_sample_cnt(%d) less than 3", booster->merge_cfg.min_num);

        bt_default_val(&(target_info->bth));
    }
    else
    {
        xerror = vpm_comp_analysis_samples(target_info);
        if (xerror < 0)
        {
            rt_log_warning(ERRNO_WARNING, "vpm_comp_analysis_samples error");
            xerror = 0;
            bt_default_val(&(target_info->bth));
        }
    }
    rt_log_notice("default score=%d, accurate score=%d, exploring score = %d",
                    target_info->bth.default_score, target_info->bth.accurate_score, target_info->bth.exploring_score);

finish:
    return xerror;
}

static int vpm_get_bf_threshold(int score, boost_threshold_t *bth)
{
    int      result = (-ERRNO_NO_ELEMENT);

    if ((score >= 0) && (score <= 100))
    {
        result = score;
    }
    else
    {
        result = vrs_get_rule_threshold(score, bth);
    }

    rt_log_info("bf_threshold : %d", result);

    return result;
}

int vpm_get_bth_by_topx(int score, int topx, int clue_id, int object_id)
{
    int *score_records = NULL;
    int real_topx = 0, ret_cnt = 0;
    int bf_threshold = 0;

    if ((score >= 0) && (score <= 100))
    {
        /** 自定义模式，直接使用用户设定的阈值*/
        return score;
    }

    /** 根据搜寻、均衡、精准三个模式，real_dln取 dln*2、dln、dln/2三个值，一一对应 */
    switch (score)
    {
        case threshold_expl:
            real_topx = topx * 2;
            break;
        case threshold_accu:
            real_topx = topx / 2;
            break;
        case threshold_deft:
            real_topx = topx;
            break;
        default:
            rt_log_error(ERRNO_NO_ELEMENT, "score : %d", score);
            return score;
    }
    score_records = (int *)kmalloc(sizeof(int) * real_topx, MPF_CLR, -1);

    ret_cnt = vrs_get_topx_records(real_topx, clue_id, object_id, score_records);
    rt_log_notice("Get hit-records(%d) by clueid(%d) and objectid(%d)", ret_cnt, clue_id, object_id);
    if (ret_cnt > 0)
    {
        if (ret_cnt >= real_topx)
        {
            bf_threshold = score_records[real_topx - 1];
        }
        else
        {
            bf_threshold = score_records[ret_cnt - 1];
        }
    }
    kfree(score_records);
    score_records = NULL;

    return bf_threshold;
}


/****************************************************************************
 函数名称  : vpm_get_hit_records
 函数功能    : 从数据库中获取命中记录信息
 输入参数    : 1、clue_id    线索id
           2、object_id   对象id
 输出参数    : records 链表结构，存放命中信息
 返回值     : 成功返回命中信息的个数，失败返回非0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_get_hit_records(int clue_id, int object_id, struct hit_records_t *records)
{
    int retval = 0;

    if (NULL == records)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "The Point is NULL");
        return ERRNO_INVALID_ARGU;
    }

    /*调用数据库访问接口，分配的内存会在vpm_load_hit_model中释放*/
    retval = vrs_get_hit_records(clue_id, object_id, records);

    rt_log_notice("Get hit-records(%d) by clueid(%d) and objectid(%d)", retval, clue_id, object_id);

    return retval;
}

/****************************************************************************
 函数名称  : vpm_load_hit_model
 函数功能    : 根据历史命中信息找到并加载所有待匹配的model，
 输入参数    : 1、max_model_num    待加载的model总数
           2、vdu_dir   model的根目录
           3、records  历史命中信息
 输出参数    : cur_modelist 链表结构，存放加载的model信息
 返回值     : 成功返回0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_load_hit_model(int max_model_num, char *vdu_dir,
                                        struct hit_records_t *records, struct modelist_t *cur_modelist)
{
    int xerror = 0, valid_models = 0;
    char model_realpath[FILE_PATH_LEN] = {0};
    uint64_t  begin, end;
    time_t    call_time;
    struct tm *tms;
    struct tm tm_buf;
    struct hit_records_t *__this = NULL, *__tmp = NULL;

    begin = rt_time_ms ();
    __this = records->next;
    while (__this != NULL)
    {
        call_time = (__this->callid >> 32) & 0xffffffff;

        tms = localtime_r(&call_time, &tm_buf);
        if (tms == NULL)
        {
            rt_log_notice("The Time of callid is err");
            goto recordfree;
        }
        /*model的真实路径拼装需要与VPW的model保存方式保持一致*/
        snprintf(model_realpath, FILE_PATH_LEN, "%s/normal/%04d-%02d-%02d/%lx_%s.model",
            vdu_dir, tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
            __this->callid, (__this->dir == 1) ? "up" : "down");
        if (!rt_file_exsit (model_realpath))
        {
            rt_log_notice("The model(%s) is not exist", model_realpath);
            goto recordfree;
        }
        //rt_log_debug("model_realpath : %s", model_realpath);
        xerror = modelist_load_one_model(model_realpath, __this->callid, cur_modelist);
        if (!xerror)
        {
            valid_models++;
        }
recordfree:
        __tmp = __this;
        __this = __this->next;
        kfree(__tmp); /*内存在vpm_get_hit_records中分配*/
    }
    end = rt_time_ms ();
    rt_log_notice ("*** Loading Model Database Okay. Result=%d/(%ld, %d), Costs=%lf sec(s)",
                valid_models, cur_modelist->sg_cur_size, max_model_num, (double)(end - begin) / 1000);
    return 0;
}

/*将结果封装到json中*/
static void vpm_json_array_add_result(int m, char **so, float *sg_score,
                                                  int bf_threshold, json_object *sample_array)
{
    int i = 0, cnt = 1;
    uint64_t callid = 0;
    json_object *array_item_node;

    for (i = 0; i < m; i++)
    {
        if ((int)sg_score[i] < bf_threshold)
        {
            continue;
        }
        sscanf(so[i], "%lu", &callid);
        array_item_node = json_object_new_object();
        json_object_object_add(array_item_node, "callid", json_object_new_int64((int64_t)callid));
        json_object_object_add(array_item_node, "score", json_object_new_int((int)sg_score[i]));
        json_object_array_add(sample_array, array_item_node);

        rt_log_debug("====================[%d]", cnt);
        rt_log_debug("sg_score       = %d", (int)sg_score[i]);
        rt_log_debug("callid         = %lu", callid);
        cnt++;
    }
}

/****************************************************************************
 函数名称  : vpm_json_boost_filter
 函数功能    : 实现高级筛选功能，主要是解析json参数，根据参数来进行二次匹
           配，  将匹配的结果返回给WEB
 输入参数    : msg_body    json消息体，需要进行解析
 输出参数    : msg_obody    用于封装json消息发送给web
 返回值     : 成功返回0，失败返回非0
 创建人     : yuansheng
 备注      :
****************************************************************************/
static int vpm_json_boost_filter(json_object *msg_body, json_object *msg_obody)
{
    float *sg_score = NULL;
    char wav_dat[FILE_PATH_LEN] = {0}, **so = NULL;
    int ret = -1, m = 0, md_index = 0, clue_id, object_id, samples = 0, hit_num = 0;
    int score = -1, bf_threshold = 0, bf_threshold1 = 0;
    uint64_t target_id;
    json_object *cid_obj, *oid_obj, *tid_obj, *score_obj, *ls, *sample_array;
    vrs_target_attr_t target_info;
    struct modelist_t *cur_modelist = NULL;
    struct vrs_trapper_t *rte = NULL;
    struct hit_records_t records = {NULL, 0, 0};

    if (NULL == msg_body || NULL == msg_obody)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "Param is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }

    rte = vrs_default_trapper();

    cid_obj = web_json_to_field(msg_body, "clueid");
    if (!cid_obj)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "json clueid is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }
    sscanf(json_object_get_string(cid_obj), "%d", &clue_id);

    oid_obj = web_json_to_field (msg_body, "objectid");
    if (!cid_obj)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "json objectid is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }
    sscanf(json_object_get_string(oid_obj), "%d", &object_id);

    memset(&target_info, 0, sizeof(target_info));
    tid_obj = web_json_to_field (msg_body, "targetid");
    if (!cid_obj)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "json targetid is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }
    sscanf(json_object_get_string(tid_obj), "%lu", &target_id);
    target_info.target_id = target_id;

    score_obj = web_json_to_field (msg_body, "score");
    if (!cid_obj)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "json score is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }
    sscanf(json_object_get_string(score_obj), "%d", &score);

    ls = web_json_to_field (msg_body, "ls");
    if (!ls)
    {
        rt_log_error(ERRNO_INVALID_ARGU, "json ls is NULL");
        ret = ERRNO_INVALID_ARGU;
        goto finish;
    }
    samples = json_object_array_length(ls);
    if ((samples > FILE_NUM_PER_TARGET) || (samples <= 0))
    {
        rt_log_error(ERRNO_INVALID_VAL, "Wav file num is invalid");
        ret = ERRNO_INVALID_VAL;
        goto finish;
    }

    /**1、创建存放用户指定的用于高级筛选的样本路径*/
    vpm_make_tmp_dir(&target_info);

    /**2、根据callid和方向找到用户指定用于二次匹配的语音文件，解密后将其          转存到临时的样本目录中         */
    ret = vpm_get_sample_wav(ls, samples, target_info.root_dir);
    if (ret != 0)
    {
        rt_log_notice("Get sample wav error");
        goto finish;
    }

    /**3、将用户之前添加的该目标样本copy一份到临时的样本目录中*/
    ret = vpm_copy_orig_sample(rte->sample_dir, &target_info);
    if (ret != 0)
    {
        rt_log_notice("Copy orig sample error");
        goto finish;
    }

    /**4、获取所有的样本信息, 根据样本信息进行交叉匹配计算阈值,并确定可合并的样本wav*/
    ret = vpm_get_bth_info(&target_info);
    if (ret != 0)
    {
        rt_log_notice("Get boost filter threshold error");
        goto finish;
    }

    /**5、将可合并的样本合并为一个大的wav*/
    snprintf(wav_dat, FILE_PATH_LEN - 1, "%s/%d-%d.wav", rte->tmp_dir, clue_id, object_id);
    ret = vpm_wav_merge(&target_info, wav_dat);
    if (ret != 0)
    {
        rt_log_notice("wav merge error");
        goto finish;
    }

    /**6、根据clueid和objectid去查找case数据库，将历史命中的callid和dir找到*/
    hit_num = vpm_get_hit_records(clue_id, object_id, &records);
    if (hit_num == 0)
    {
        rt_log_notice("The hit-recored is 0 by clueid(%d) and objectid(%d), logic err...", clue_id, object_id);
        goto finish;
    }

    /**7、根据clueid和targetid查找此线索的阈值*/
    bf_threshold = vpm_get_bf_threshold(score, &(target_info.bth));
    if (bf_threshold < 0)
    {
        rt_log_notice("vpm get boost filter threshold error(%d)", bf_threshold);
        goto finish;
    }

    /**新的需求，取当前时间至前24小时内的命中前rte->topx_num的命中记录，获取阈值 */
    if (rte->topx_switch)
    {
        bf_threshold1 = vpm_get_bth_by_topx(score, rte->topx_num, clue_id, object_id);
        rt_log_notice("The boost filter threshold (%d : %d)", bf_threshold, bf_threshold1);
        if (bf_threshold1 > bf_threshold)
        {
            bf_threshold = bf_threshold1;
        }
    }

    cur_modelist = modelist_create (hit_num);

    /**8、根据历史命中的callid和dir找到，找到对应的model加载*/
    vpm_load_hit_model(hit_num, rte->vdu_dir, &records, cur_modelist);

    sample_array = json_object_new_array();

    /**9、开始进行匹配*/
    if (likely (cur_modelist) && cur_modelist->sg_cur_size > 0)
    {
        sg_score = cur_modelist->sg_score;
        m = cur_modelist->sg_cur_size;
        so = cur_modelist->sg_owner;
        ret = rte->tool->voice_recognition_advanced_ops (wav_dat, cur_modelist, sg_score, &md_index, W_ALAW);
        if ((ret == 0) && (md_index < m))
        {
            vpm_json_array_add_result(m, so, sg_score, bf_threshold, sample_array);
        }
    }
    json_object_object_add (msg_obody, "ls", sample_array);

    modelist_destroy (cur_modelist);

finish:
    if (rt_dir_exsit(target_info.root_dir))
    {
        rt_remove_dir(target_info.root_dir);
    }

    if (rt_file_exsit (wav_dat))
    {
        remove (wav_dat);
    }

    return ret;
}

/****************************************************************************
 函数名称  : SGBooster
 函数功能    : 注册的线程回调函数，接收处理消息队列boost_mq的消息
 输入参数    : 指针param
 输出参数    : 无
 返回值     : 正常情况下不会退出
 创建人     : yuansheng
 备注      :
****************************************************************************/
void *SGBooster(void *param)
{
    const char    *jstr = NULL;
    int s = 0;
    int send_data_len1 = 0 ,send_data_len = 0 ,send_len = 0;
    char *send_data = NULL, *send_data1 = NULL;
    struct vpm_t            *vpm;
    struct vrs_trapper_t    *rte;
    struct json_hdr         jhdr;
    message                 data = NULL;
    json_object   *head, *injson, *msg_i_body, *msg_o_body;

    rte = (struct vrs_trapper_t *)param;
    vpm = rte->vpm;

    FOREVER {
        data = NULL;
        /** Recv from internal queue */
        rt_mq_recv (vpm->boost_mq, &data, &s);
        if (unlikely(!data))
        {
            continue;
        }

        rt_log_info ("%s", (char *)data);
        web_json_parser (data, &jhdr, &injson);
        msg_i_body = web_json_to_field (injson, "msg");
        msg_o_body = json_object_new_object();
        head = json_object_new_object();
        switch (jhdr.cmd) {
            case SG_X_BOOST:
                vpm_json_boost_filter (msg_i_body, msg_o_body);
                web_json_head_add (head, &jhdr, msg_o_body);
                break;
            default:
                break;
        }
        jstr = json_object_to_json_string (head);
        send_data_len = strlen(jstr);
        send_data = (char *)malloc((send_data_len / 1024 + 1) * 1024);
        send_data1 = send_data;
        bzero(send_data, ((send_data_len / 1024 + 1) * 1024));
        snprintf(send_data, ((send_data_len / 1024 + 1) * 1024), "%s\n", jstr);
        send_data_len1 = strlen(send_data);
        while(send_data_len1 > 0)
        {
            send_len = rt_sock_send (vpm->web_sock, send_data, send_data_len1);
            send_data = send_data +send_len;
            send_data_len1 -= send_len;
        }

        web_json_tokener_parse ("OUTBOUND", jstr);
        jstr = NULL;
        json_object_put (injson);
        json_object_put (msg_o_body);
        json_object_put (head);
        kfree (data);
        kfree(send_data1);
    }

    task_deregistry_id (pthread_self());

    return NULL;
}

static struct rt_task_t SGBoostTask =
{
    .module = THIS,
    .name = "SG Booster Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGBooster,
};

static struct rt_task_t SGCDBTask =
{
    .module = THIS,
    .name = "SG CDB Write Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &vrsTrapper,
    .routine = SGCDBWriter,
};

static vrs_boost_t def_booster;

vrs_boost_t *default_booster()
{
    return &def_booster;
}

static int
load_booster_merge_cfg(vrs_boost_t *booster)
{
    int xret = -1;
    ConfNode *base = NULL, *subchild = NULL;
    base = ConfGetNode("mergecfg");
    if (NULL == base) {
        xret = -1;
        goto finish;
    }

    TAILQ_FOREACH(subchild, &base->head, next) {
        if (!STRCMP(subchild->name, "min-num"))
            booster->merge_cfg.min_num = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "merge-score"))
            booster->merge_cfg.merge_score = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "bad-score"))
            booster->merge_cfg.bad_score = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "not-merge-bad-score"))
            booster->merge_cfg.not_merge_bad_score = integer_parser(subchild->val, 0, 100);
    }

finish:

    return xret;
}


/****************************************************************************
 函数名称  : load_booster_threshold_cfg
 函数功能    : 获取booster合并配置信息
 输入参数    : 无
 输出参数    : booster, 待填充阈值配置信息的booster对象
 返回值     : -1, 错误; 0, 正确
 创建人     : wangkai
 备注      :
****************************************************************************/
static int
load_booster_threshold_cfg(OUT vrs_boost_t *booster)
{
    int xret = XSUCCESS;
    ConfNode *base = NULL, *subchild = NULL;

    yaml_init();
    /* vpm allocate file */
    if (ConfYamlLoadFile(VRS_SYSTEM_BASIC_CFG)) {
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_SYSTEM_BASIC_CFG);
        goto finish;
    }

    base = ConfGetNode("threshold");
    if (NULL == base || NULL == booster) {
        xret = -1;
        goto finish;
    }

    TAILQ_FOREACH(subchild, &base->head, next) {
        if (!STRCMP(subchild->name, "default-min"))
            booster->threshold_cfg.default_min = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "default-max"))
            booster->threshold_cfg.default_max = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "explore-min"))
            booster->threshold_cfg.exploring_min = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "explore-max"))
            booster->threshold_cfg.exploring_max = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "accurate-magic"))
            booster->threshold_cfg.accurate_magic = integer_parser(subchild->val, 0, 100);
        if (!STRCMP(subchild->name, "explore-magic"))
            booster->threshold_cfg.exploring_magic = integer_parser(subchild->val, 0, 100);
    }
finish:

    return xret;
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

static int
load_vocfile_path(vrs_boost_t *booster)
{
    int xret = -1;
    ConfNode *base = NULL;

    if (!booster)
        goto finish;

    base = ConfGetNode("vpudata-path");
    if (NULL == base) {
        goto finish;
    }

    if (likely(base->val)) {
        xret = 0;
        memset(booster->vocfile_dir, 0, sizeof(booster->vocfile_dir));
        memcpy(booster->vocfile_dir, base->val, strlen(base->val));
        __sg_check_and_mkdir(booster->vocfile_dir);
    }

finish:

    return xret;
}

static int
load_casedb_cfg(vrs_boost_t *booster)
{
    int xret = -1;
    ConfNode *base = NULL, *subchild = NULL;

    if (!booster)
        goto finish;

    base = ConfGetNode("casedb");
    if (NULL == base)
    {
        xret = -1;
        goto finish;
    }

    TAILQ_FOREACH(subchild, &base->head, next) {
        if (!STRCMP(subchild->name, "dbname"))
            if (subchild->val) booster->caseDB.dbname = strdup (subchild->val);
        if (!STRCMP(subchild->name, "user"))
            if (subchild->val) booster->caseDB.usrname = strdup (subchild->val);

        if (!STRCMP(subchild->name, "passwd"))
            if (subchild->val) booster->caseDB.passwd = strdup (subchild->val);
    }

    xret = 0;
finish:

    return xret;

}


static int
boost_check_env(vrs_boost_t *booster)
{
    rt_log_notice ("min-num:%d merge-score:%d bad-score:%d not-merge-bad-score:%d", booster->merge_cfg.min_num,
        booster->merge_cfg.merge_score, booster->merge_cfg.bad_score, booster->merge_cfg.not_merge_bad_score);

    rt_log_notice ("config of threshold cfg");
    rt_log_notice ("    default_min: %d", booster->threshold_cfg.default_min);
    rt_log_notice ("    default_max: %d", booster->threshold_cfg.default_max);
    rt_log_notice ("    exploring_min: %d", booster->threshold_cfg.exploring_min);
    rt_log_notice ("    exploring_max: %d", booster->threshold_cfg.exploring_max);
    rt_log_notice ("    accurate_magic: %d", booster->threshold_cfg.accurate_magic);
    rt_log_notice ("    exploring_magic: %d", booster->threshold_cfg.exploring_magic);

    rt_log_notice ("bf-threshold:%d", booster->bf_threshold);
    rt_log_notice ("casedb:%s, user:%s, passwd:%s", booster->caseDB.dbname, booster->caseDB.usrname, booster->caseDB.passwd);
    rt_log_notice ("%-16s %-7s\"%18s\"%10s", "Checking Path of", "VPU", booster->vocfile_dir, rt_dir_exsit (booster->vocfile_dir) ? "ready" : "not ready");

    return 0;
}

int vpm_boost_conf_init(vrs_boost_t *booster)
{
    int xret = -1;

    yaml_init();
    /* vpm allocate file */
    if (ConfYamlLoadFile(VRS_VPM_CFG)) {
        rt_log_error(ERRNO_INVALID_ARGU,
                     "%s\n", VRS_VPW_CFG);
        goto finish;
    }

    rt_log_debug("vpm boost conf init!");

    if (!booster)
        goto finish;

    bzero(booster, sizeof(vrs_boost_t));

    /** bf-threshold */
    xret = ConfYamlReadInt("bf-threshold", (int *)&(booster->bf_threshold));
    if (xret != 0)
    {
        rt_log_error(ERRNO_INVALID_VAL, "Failed to read bf-threshold from the yaml file ");
        goto finish;
    }

    load_booster_merge_cfg(booster);
    load_vocfile_path(booster);
    load_casedb_cfg(booster);
    xret = 0;

finish:

    return xret;
}

int vpm_boost_init()
{
    vrs_boost_t *booster = default_booster();

    vpm_boost_conf_init(booster);

    load_booster_threshold_cfg(booster);

    boost_check_env(booster);

    tmr_start(tmr_create(1, "CaseDB Connection TMR",
                TMR_PERIODIC, vrs_oci_conn_tmr, 1, (char **)&(booster->caseDB), 15));

    task_registry (&SGBoostTask);
    task_registry (&SGCDBTask);

    return 0;
}


