//wangwenchao testEnv2.0 2016.4.11
#ifndef SPEAKID_2010_4_17
#define SPEAKID_2010_4_17
#endif
#include <stdlib.h>
#include <vector>
using namespace std;


#if defined(_WIN32) || defined(_WIN64)
    #ifdef DLL_EXPORT
        #define TIT_DECLDIR __declspec(dllexport)
    #else
        #define TIT_DECLDIR __declspec(dllimport)
    #endif
#else
    #define TIT_DECLDIR
#endif


typedef struct
{
    char*          data;
    unsigned       size;
    unsigned int spkId;
}SR_Model;


struct SR_IdentifyResult
{
    unsigned int spkId;
    float           score;

    const SR_IdentifyResult & operator=(const SR_IdentifyResult &idResult)
    {
        spkId = idResult.spkId;
        score = idResult.score;
        return *this;
    }

    const bool operator<=(const SR_IdentifyResult &idScore)
    {
        return score <= idScore.score;
    }

    const bool operator>=(const SR_IdentifyResult &idScore)
    {
        return score >= idScore.score;
    }
};

struct Wavs
{
	vector<short *> buf;
	vector<unsigned int> len;
};
// 出错定义
enum TIT_RET_CODE
{
        TIT_SPKID_SUCCESS=0,
        TIT_SPKID_LICENSE_EXPIRED,			// 1,授权过期
        TIT_SPKID_NO_INIT,					// 2,引擎没有初始化
        TIT_SPKID_INIT_AGAIN,				// 3,多次调用SpeakerVerifier_Init
        TIT_SPKID_ERROR_INVALID_INPUT,		// 4,无效输入
        TIT_SPKID_ERROR_CONFIG,				// 5,读配置文件错
        TIT_SPKID_ERROR_FEATURE,			// 6,与特征类相关的错误
        TIT_SPKID_ERROR_MODEL,				// 7,与模型类相关的错误
        TIT_SPKID_ERROR_AUDIOSEG,			// 8,与VAD相关的错误
        TIT_SPKID_ERROR_TOOSHORT_SCORE,		// 9,用于Score的语音数据太短
        TIT_SPKID_ERROR_TOOSHORT_TRAIN,		// 10,用于Train的语音数据太短
        TIT_SPKID_ERROR_ALLOC,				// 11,内存分配错
        TIT_SPKID_ERROR_FILE_OPEN,			// 12,文件打开错
        TIT_SPKID_ERROR_RECOG,				// 13,识别失败
        TIT_SPKID_ERROR_CLUSTER				// 14,聚类错误

};


#ifdef __cplusplus
extern "C" {
#endif


    // 初始化
    TIT_DECLDIR TIT_RET_CODE TIT_SPKID_Init(
        char    *p_cConfigFile,                // 配置文件
        char    *p_cCurModelPath,              // 模型路径
        int     &p_nModelLen,                  // 模型大小
        int     &p_nFeatLen,					// 特征大小
		int		&p_nIndexlen);


    // 结束
    TIT_DECLDIR TIT_RET_CODE TIT_SPKID_Exit();


    //////////////////////////////////////////////////////////////////////////
    // 训练接口 - 开始
    //////////////////////////////////////////////////////////////////////////

	// 切静音、彩振铃的Buf训练接口
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_CutAll_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // 模型数据
		char    *wavfile);

	// 只切静音的Buf训练接口
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_CutSil_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // 模型数据
		char    *wavfile);

	// 针对已处理语音的Buf训练接口
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // 模型数据
		char    *wavfile);

    //////////////////////////////////////////////////////////////////////////
    // 训练接口 - 结束
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // 识别接口 - 开始
    //////////////////////////////////////////////////////////////////////////

    // Buf接口的多个说话人识别接口, 切静音+说话人聚类
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_AddCfd_CutSil_Cluster(
        short   *p_sDat,                      // 音频数据8K-16Bit
        int     p_nDatLen,				      // 音频数据长度（采样点数目）
        void    **p_pSpeakerModel,		      // 待识别的模型数组
        float   *&p_pfSpkScore,			      // 模型得分
        int     p_nModelNum,			      // 当前加载的模型数目
        int     &p_nMatchModelIndex,          // 返回识别出的模型index，若小于零，则表示拒识
        char    *wavfile = NULL);             // 调试用, 置为 NULL

    // Buf接口的多个说话人识别接口, 只切静音
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_AddCfd_CutSil_NoCluster(
        short   *p_sDat,                      // 音频数据8K-16Bit
        int     p_nDatLen,				      // 音频数据长度（采样点数目）
        void    **p_pSpeakerModel,		      // 待识别的模型数组
        float   *&p_pfSpkScore,			      // 模型得分
        int     p_nModelNum,			      // 当前加载的模型数目
        int     &p_nMatchModelIndex,          // 返回识别出的模型index，若小于零，则表示拒识
        char    *wavfile = NULL);             // 调试用, 置为 NULL

    // Buf接口的多个说话人识别接口, 切静音+说话人聚类
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Struct_AddCfd_CutSil_Cluster(
        short               *p_sDat,                    // 音频数据8K-16Bit
        int                 p_nDatLen,				    // 音频数据长度（采样点数目）
        SR_Model            *p_pSpeakerModel,		    // 待识别的模型数组
        SR_IdentifyResult   *p_pSpkResult,	            // 模型得分
        int	                p_nModelNum,			    // 当前加载的模型数目
        int                 &p_nMatchModelIndex);	    // 返回识别出的模型index，若小于零，则表示拒识

    // Buf接口的多个说话人识别接口, 只切静音
    TIT_DECLDIR TIT_RET_CODE  TIT_SCR_Struct_AddCfd_CutSil_NoCluster(
        short               *p_sDat,                    // 音频数据8K-16Bit
        int                 p_nDatLen,				    // 音频数据长度（采样点数目）
        SR_Model            *p_pSpeakerModel,		    // 待识别的模型数组
        SR_IdentifyResult   *p_pSpkResult,	            // 模型得分
        int	                p_nModelNum,			    // 当前加载的模型数目
        int                 &p_nMatchModelIndex);	    // 返回识别出的模型index，若小于零，则表示拒识

    //////////////////////////////////////////////////////////////////////////
    // 识别接口 - 结束
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // 提特征(建索引)接口 - 开始
    //////////////////////////////////////////////////////////////////////////

    // 切静音+说话人聚类
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_CutSil_Cluster_Index(
        short   *p_sDat,		        // 音频数据8K-16Bit
        int     p_nDatLen,				// 音频数据长度（采样点数目）
        void    *p_Feat,				// 建立的索引特征
        char    *wavfile = NULL);       // 调试用, 置为 NULL

    // 只切静音
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_CutSil_NoCluster_Index(
        short   *p_sDat,		        //音频数据8K-16Bit
        int     p_nDatLen,				//音频数据长度（采样点数目）
        void    *p_Feat,				//建立的索引特征
        char    *wavfile = NULL);       // 调试用, 置为 NULL

    //////////////////////////////////////////////////////////////////////////
    // 提特征(建索引)接口 - 结束
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // 索引与模型匹配接口 - 开始
    //////////////////////////////////////////////////////////////////////////

    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Index_Match(
        void		*p_Feat,				//建立的索引特征
        void		**p_pSpeakerModel,		//待识别的模型数组
        float	    *&p_pfSpkScore,			//模型得分
        int			p_nModelNum,			//当前加载的模型数目
        int         &p_nMatchModelIndex);	//返回识别出的模型index，若小于零，则表示拒识

    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Struct_Index_Match(
        void		*p_Feat,				//建立的索引特征
        SR_Model	*p_pSpeakerModel,		//待识别的模型数组
        SR_IdentifyResult *p_pSpkResult,	//模型得分
        int			p_nModelNum,			//当前加载的模型数目
        int         &p_nMatchModelIndex);	//返回识别出的模型index，若小于零，则表示拒识

    //////////////////////////////////////////////////////////////////////////
    // 模型转换接口
    //////////////////////////////////////////////////////////////////////////

	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_For_Test(
		void    *p_pIndexModel,		      // 已注册的单个模型
		void	*p_pSpeakerModel);			//转换为测试用注册模型


#ifdef __cplusplus
}
#endif
