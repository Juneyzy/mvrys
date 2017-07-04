#ifndef __VPM_BOOST_H__
#define __VPM_BOOST_H__

#include "vrs_oci.h"
#include "vrs_senior.h"

#define FILE_PATH_LEN   256
#define FILE_NAME_LEN   128

#define TARGET_QUERY_SCORE  55

/**
 * 以下的值都可以通过vpm.yaml配置
 */
// 均衡模式默认的最低分数
#define DEFAULT_MIN     35
// 均衡模式默认的最高分数
#define DEFAULT_MAX     52
// 搜寻模式默认的最低分数
#define EXPLORING_MIN   30
// 搜寻模式默认的最大分数
#define EXPLORING_MAX   47
// 精准模式默认的魔数, 用在DEFAULT模式下上调的值
#define ACCURATE_MAGIC  10
// 搜寻模式默认的魔数, 比DEFAULT模式低的值
#define EXPLORING_MAGIC 5

#define WAV_FILE_MERGE      1
#define WAV_FILE_NO_MERGE   0

typedef struct __threshold_cfg {
    int default_min;
    int default_max;
    int exploring_min;
    int exploring_max;
    int accurate_magic;
    int exploring_magic;
} threshold_cfg_t;

struct sample_merge_cfg {
   /* 1, 用户累计上传的同一目标声纹多于min_num个时候需交叉匹配算阈值;
    * 2, 均衡模式的阈值取前min_num个交叉匹配平均分最高的样本和其他所有样本匹配的最低分*/
    int min_num;
    int merge_score; // 声纹>=min_num个时，一个样本和其他所有上传的该目标声纹匹配的平均分大于等于merge_score就合并
    int bad_score; // 声纹>=min_num个时，一个样本和其他所有上传的该目标声纹匹配的平均分小于bad_score就认为质量差
    int not_merge_bad_score; // 在少于min_num个的时候，1到min_num-1个样本通过拆分算出来的得分低于not_merge_bad_score就认为质量差
};

typedef struct __vrs_boost{
    struct sample_merge_cfg merge_cfg;

    threshold_cfg_t threshold_cfg;

    /** 案件语音存储路径，与vpu的存储语音配置保持一致*/
    char vocfile_dir[128];

    /** 高级筛选的阈值，当前设置为可配置*/
    int bf_threshold;

    /** 连接案件数据库需要的参数 */
    struct vrs_db_info caseDB;
} vrs_boost_t;

/** 每个WAV样本的属性 */
typedef struct __vrs_sample_attr
{
    char    wav_name[FILE_NAME_LEN];
    char    wav_path[FILE_PATH_LEN];    /** wav的完整路径，包括样本名 */

    int     is_alaw;      /** wav文件是否有WAVE头部 */
    int     file_size;    /** wav文件合并时用到*/

    int     avg_score;    /** 这个样本与同目标下的其他样本交叉匹配分数的平均值 */
    int     min_score;    /** 这个样本与同目标下的其他样本交叉匹配分数的最小值 */

    int     merge_flag;   /** 标记这个样本是否会用于合并 */
    int     status;       /** 标记这个样本的状态*/
}vrs_sample_attr_t;

/** 每个目标的属性 */
typedef struct __vrs_target_attr
{
    uint64_t     target_id;
    int          sample_cnt;

    char         root_dir[FILE_PATH_LEN];

    vrs_sample_attr_t samples[FILE_NUM_PER_TARGET];
    boost_threshold_t bth;
}vrs_target_attr_t;

typedef struct __wav_header {
    uint8_t riff[4];                //4 byte , 'RIFF'
    uint32_t file_size;             //4 byte , 文件长度
    uint8_t riff_type[4];           //4 byte , 'WAVE'

    uint8_t fmt[4];                 //4 byte , 'fmt '
    uint32_t fmt_size;              //4 byte , 数值为16或18，18则最后又附加信息
    uint16_t fmt_tag;               //2 byte , 编码方式，一般为0x0001
    uint16_t fmt_channel;           //2 byte , 声道数目，1--单声道；2--双声道
    uint32_t fmt_samples_persec;    //4 byte , 采样频率
    uint32_t avg_bytes_persec;      //4 byte , 每秒所需字节数,
                                               //Byte率=采样频率*音频通道数*每次采样得到的样本位数/8
    uint16_t block_align;           //2 byte , 数据块对齐单位(每个采样需要的字节数),
                                               //块对齐=通道数*每次采样得到的样本位数/8
    uint16_t bits_persample;        //2 byte , 每个采样需要的bit数

    uint8_t data[4];                //4 byte , 'data'
    uint32_t data_size;             //4 byte ,
}wav_header_t;

#define WAV_HEADER    44


enum {
    add_st_null      = 0,
    add_st_normal    = 1,
    add_st_redundant = 2,
    add_st_less_30s  = 3,       /** 有效语音小于30s，此时不会建立model */
    add_st_less_60s     = 4,    /** 有效语音小于60s，此时只会向用户提示，会建立model */
    add_st_few_similar  = 5,    /** 相似度很差，只有两个样本时会用到 */
    add_st_less_similar = 6,    /** 相似度差，只有两个样本时会用到     */
    add_st_tiny_similar = 7,    /** 相似度很差，只有三个及三个以上样本时会用到，此时该样本不会用于合并 */
    add_st_poor_similar = 8,    /** 相似度差，只有三个及三个以上样本时会用到，此时该样本会用于合并 */
    add_st_less_m       = 9
};

#define WAV_AVG_BYTES_PERSEC    8000

#define FRAME_LEN    160

#define FRAME_OFFSET (FRAME_LEN * 2 / 5)

enum FRAME_TYPE
{
    FRAME_UNKNOW,
    FRAME_VOICE,
    FRAME_SMOOTH_VOICE,
    FRAME_SILENCE
};


//------------------------------------------------------------------------------------
// all public methods for other modules.
extern int vpm_boost_init();
extern vrs_boost_t *default_booster();
extern int check_redundant_target_wav(uint64_t tid, json_object *arr);
extern int check_current_file_redundant(uint64_t tid, char *wavname, json_object *arr);
extern int vpm_comp_analysis_samples(vrs_target_attr_t *target_info);
extern int vpm_judge_samples_similarity(vrs_target_attr_t *target_info);

extern int vpm_get_wav_file_info(vrs_sample_attr_t *__this);
extern int vpm_wav_merge(vrs_target_attr_t *target_info, char *merge_realpath);
extern void vpm_wav_header_init(wav_header_t *header, int file_size);

extern void vpm_json_assembling_result(char *filename, int status, json_object *arr);
extern int vpm_get_wav_status(char *fullpath);

#endif //__VPM_BOOST_H__
