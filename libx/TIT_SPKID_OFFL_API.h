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
// ������
enum TIT_RET_CODE
{
        TIT_SPKID_SUCCESS=0,
        TIT_SPKID_LICENSE_EXPIRED,			// 1,��Ȩ����
        TIT_SPKID_NO_INIT,					// 2,����û�г�ʼ��
        TIT_SPKID_INIT_AGAIN,				// 3,��ε���SpeakerVerifier_Init
        TIT_SPKID_ERROR_INVALID_INPUT,		// 4,��Ч����
        TIT_SPKID_ERROR_CONFIG,				// 5,�������ļ���
        TIT_SPKID_ERROR_FEATURE,			// 6,����������صĴ���
        TIT_SPKID_ERROR_MODEL,				// 7,��ģ������صĴ���
        TIT_SPKID_ERROR_AUDIOSEG,			// 8,��VAD��صĴ���
        TIT_SPKID_ERROR_TOOSHORT_SCORE,		// 9,����Score����������̫��
        TIT_SPKID_ERROR_TOOSHORT_TRAIN,		// 10,����Train����������̫��
        TIT_SPKID_ERROR_ALLOC,				// 11,�ڴ�����
        TIT_SPKID_ERROR_FILE_OPEN,			// 12,�ļ��򿪴�
        TIT_SPKID_ERROR_RECOG,				// 13,ʶ��ʧ��
        TIT_SPKID_ERROR_CLUSTER				// 14,�������

};


#ifdef __cplusplus
extern "C" {
#endif


    // ��ʼ��
    TIT_DECLDIR TIT_RET_CODE TIT_SPKID_Init(
        char    *p_cConfigFile,                // �����ļ�
        char    *p_cCurModelPath,              // ģ��·��
        int     &p_nModelLen,                  // ģ�ʹ�С
        int     &p_nFeatLen,					// ������С
		int		&p_nIndexlen);


    // ����
    TIT_DECLDIR TIT_RET_CODE TIT_SPKID_Exit();


    //////////////////////////////////////////////////////////////////////////
    // ѵ���ӿ� - ��ʼ
    //////////////////////////////////////////////////////////////////////////

	// �о������������Bufѵ���ӿ�
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_CutAll_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // ģ������
		char    *wavfile);

	// ֻ�о�����Bufѵ���ӿ�
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_CutSil_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // ģ������
		char    *wavfile);

	// ����Ѵ���������Bufѵ���ӿ�
	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_New(
		const Wavs *wavs,
		void    *SpeakerModel,                // ģ������
		char    *wavfile);

    //////////////////////////////////////////////////////////////////////////
    // ѵ���ӿ� - ����
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // ʶ��ӿ� - ��ʼ
    //////////////////////////////////////////////////////////////////////////

    // Buf�ӿڵĶ��˵����ʶ��ӿ�, �о���+˵���˾���
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_AddCfd_CutSil_Cluster(
        short   *p_sDat,                      // ��Ƶ����8K-16Bit
        int     p_nDatLen,				      // ��Ƶ���ݳ��ȣ���������Ŀ��
        void    **p_pSpeakerModel,		      // ��ʶ���ģ������
        float   *&p_pfSpkScore,			      // ģ�͵÷�
        int     p_nModelNum,			      // ��ǰ���ص�ģ����Ŀ
        int     &p_nMatchModelIndex,          // ����ʶ�����ģ��index����С���㣬���ʾ��ʶ
        char    *wavfile = NULL);             // ������, ��Ϊ NULL

    // Buf�ӿڵĶ��˵����ʶ��ӿ�, ֻ�о���
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_AddCfd_CutSil_NoCluster(
        short   *p_sDat,                      // ��Ƶ����8K-16Bit
        int     p_nDatLen,				      // ��Ƶ���ݳ��ȣ���������Ŀ��
        void    **p_pSpeakerModel,		      // ��ʶ���ģ������
        float   *&p_pfSpkScore,			      // ģ�͵÷�
        int     p_nModelNum,			      // ��ǰ���ص�ģ����Ŀ
        int     &p_nMatchModelIndex,          // ����ʶ�����ģ��index����С���㣬���ʾ��ʶ
        char    *wavfile = NULL);             // ������, ��Ϊ NULL

    // Buf�ӿڵĶ��˵����ʶ��ӿ�, �о���+˵���˾���
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Struct_AddCfd_CutSil_Cluster(
        short               *p_sDat,                    // ��Ƶ����8K-16Bit
        int                 p_nDatLen,				    // ��Ƶ���ݳ��ȣ���������Ŀ��
        SR_Model            *p_pSpeakerModel,		    // ��ʶ���ģ������
        SR_IdentifyResult   *p_pSpkResult,	            // ģ�͵÷�
        int	                p_nModelNum,			    // ��ǰ���ص�ģ����Ŀ
        int                 &p_nMatchModelIndex);	    // ����ʶ�����ģ��index����С���㣬���ʾ��ʶ

    // Buf�ӿڵĶ��˵����ʶ��ӿ�, ֻ�о���
    TIT_DECLDIR TIT_RET_CODE  TIT_SCR_Struct_AddCfd_CutSil_NoCluster(
        short               *p_sDat,                    // ��Ƶ����8K-16Bit
        int                 p_nDatLen,				    // ��Ƶ���ݳ��ȣ���������Ŀ��
        SR_Model            *p_pSpeakerModel,		    // ��ʶ���ģ������
        SR_IdentifyResult   *p_pSpkResult,	            // ģ�͵÷�
        int	                p_nModelNum,			    // ��ǰ���ص�ģ����Ŀ
        int                 &p_nMatchModelIndex);	    // ����ʶ�����ģ��index����С���㣬���ʾ��ʶ

    //////////////////////////////////////////////////////////////////////////
    // ʶ��ӿ� - ����
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // ������(������)�ӿ� - ��ʼ
    //////////////////////////////////////////////////////////////////////////

    // �о���+˵���˾���
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_CutSil_Cluster_Index(
        short   *p_sDat,		        // ��Ƶ����8K-16Bit
        int     p_nDatLen,				// ��Ƶ���ݳ��ȣ���������Ŀ��
        void    *p_Feat,				// ��������������
        char    *wavfile = NULL);       // ������, ��Ϊ NULL

    // ֻ�о���
    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Buf_CutSil_NoCluster_Index(
        short   *p_sDat,		        //��Ƶ����8K-16Bit
        int     p_nDatLen,				//��Ƶ���ݳ��ȣ���������Ŀ��
        void    *p_Feat,				//��������������
        char    *wavfile = NULL);       // ������, ��Ϊ NULL

    //////////////////////////////////////////////////////////////////////////
    // ������(������)�ӿ� - ����
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // ������ģ��ƥ��ӿ� - ��ʼ
    //////////////////////////////////////////////////////////////////////////

    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Index_Match(
        void		*p_Feat,				//��������������
        void		**p_pSpeakerModel,		//��ʶ���ģ������
        float	    *&p_pfSpkScore,			//ģ�͵÷�
        int			p_nModelNum,			//��ǰ���ص�ģ����Ŀ
        int         &p_nMatchModelIndex);	//����ʶ�����ģ��index����С���㣬���ʾ��ʶ

    TIT_DECLDIR TIT_RET_CODE TIT_SCR_Struct_Index_Match(
        void		*p_Feat,				//��������������
        SR_Model	*p_pSpeakerModel,		//��ʶ���ģ������
        SR_IdentifyResult *p_pSpkResult,	//ģ�͵÷�
        int			p_nModelNum,			//��ǰ���ص�ģ����Ŀ
        int         &p_nMatchModelIndex);	//����ʶ�����ģ��index����С���㣬���ʾ��ʶ

    //////////////////////////////////////////////////////////////////////////
    // ģ��ת���ӿ�
    //////////////////////////////////////////////////////////////////////////

	TIT_DECLDIR TIT_RET_CODE TIT_TRN_Model_For_Test(
		void    *p_pIndexModel,		      // ��ע��ĵ���ģ��
		void	*p_pSpeakerModel);			//ת��Ϊ������ע��ģ��


#ifdef __cplusplus
}
#endif
