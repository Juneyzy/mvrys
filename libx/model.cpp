#include <stdio.h>
#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "rt_common.h"
#include "model.h"
#include "TIT_SPKID_OFFL_API.h"
#include "wav.h"
using namespace std;

#define kfree(ptr)	if(likely(ptr)) {\
							free(ptr); ptr = NULL;\
					}


static struct modelist_t   *ModeList;

struct modelist_t *default_modelist ()
{
	return ModeList;
}

void default_modelist_set (struct modelist_t *__new)
{
	ModeList = __new;
}

int   ModelToDisk (const char *mfile, void *model, int msize)
{

	FILE *fp;
	size_t s;

	/** no such file or derectory */
	TIT_RET_CODE xerror = TIT_SPKID_ERROR_FILE_OPEN;

	fp = fopen(mfile, "wb");
	if (likely(fp)) {
		s = fwrite(model, 1, msize, fp);
		fclose(fp);
		if ((int)s == msize)
			xerror = (TIT_RET_CODE)0;
	}
	
	return (int)xerror;
}

int   ModelFromDisk (const char *mfile, void *model)
{

	FILE *fp;
	size_t s;

	/** no such file or derectory */
	TIT_RET_CODE xerror = TIT_SPKID_ERROR_FILE_OPEN;

	fp = fopen(mfile, "r");
	if (likely(fp)) {
		s = fread (model, 1, SG_DATA_SIZE, fp);
		fclose(fp);
		if ((int)s == SG_DATA_SIZE)
			xerror = (TIT_RET_CODE)0;
	}
	
	return (int)xerror;
}

int   VRSEngineInit (char *conffile, char* pathname)
{
	int modelsize = 0, featsize = 0, indexsize = 0;

	return (-TIT_SPKID_Init(conffile, pathname, modelsize, featsize, indexsize));
}

int   WavToModelMemory (const char *wav_file, uint8_t *mdl_cache, int *wlen1, int *wlen2, int is_alaw)
{

	Wavs wstack;
	int s1, s2, nchl = 1;
	short *b1, *b2;
	char md_index[SG_INDEX_SIZE] = {0};
	TIT_RET_CODE xerror;
	bool result = false;
	
	xerror = TIT_SPKID_ERROR_MODEL;

	if (is_alaw)
		result = CreateWavALaw (wav_file, nchl, b1, s1, b2, s2);
	else
		result = CreateWav (wav_file, nchl, b1, s1, b2, s2);

	if (result) {
		wstack.len.push_back(s1);
		wstack.buf.push_back(b1);
		xerror = TIT_TRN_Model_CutAll_New (&wstack, (void*)md_index, NULL);
		if (xerror == TIT_SPKID_SUCCESS) {
			xerror = TIT_TRN_Model_For_Test ((void*)md_index, mdl_cache);
		}
		
		*wlen1 = s1;
		*wlen2 = s2;
		kfree(b1);
	}
	
	return (-xerror);
}

int   WavToModel (const char *wav_file, const char* mod_file, int *wlen1, int *wlen2, int is_alaw)
{
	TIT_RET_CODE xerror;
	uint8_t	mdl_cache [SG_DATA_SIZE] = {0};

	xerror = (TIT_RET_CODE)WavToModelMemory (wav_file, mdl_cache, wlen1, wlen2, is_alaw);
	if (!xerror)
		xerror = (TIT_RET_CODE)ModelToDisk (mod_file, (void*)mdl_cache, SG_DATA_SIZE);

	return (-xerror);
}

int WavToModelEx(struct sample_file_t *wav_file, int cnt, const char* mod_file)
{
	int i;
	int valid_cnt = 0;
	int s1, s2, nchl = 1;
	bool result = false;
	short *b1, *b2;
	short *pbuf[10] = {0};
	char md_index[SG_INDEX_SIZE] = {0};
	char mdl_cache[SG_DATA_SIZE] = {0};
	Wavs wstack;
	TIT_RET_CODE xerror = TIT_SPKID_ERROR_MODEL;
	struct sample_file_t *__this = NULL;

	memset(pbuf, 0, sizeof(pbuf));
	for (i = 0; i < cnt; i++)
	{
		__this = (struct sample_file_t *)(wav_file + i);
		if (0 != access(__this->sample_realpath, F_OK))
		{
			printf("file(%s) is not exist\n", __this->sample_realpath);
			continue;
		}
		if (__this->is_alaw)
			result = CreateWavALaw (__this->sample_realpath, nchl, b1, s1, b2, s2);
		else
			result = CreateWav (__this->sample_realpath, nchl, b1, s1, b2, s2);
		if (result) 
		{
			wstack.len.push_back(s1);
			wstack.buf.push_back(b1);
			__this->wlen1 = s1;
			__this->wlen2 = s2;
			pbuf[valid_cnt] = b1;
			valid_cnt++;
		}
	}
	xerror = TIT_TRN_Model_CutAll_New (&wstack, (void*)md_index, NULL);
	if (xerror == TIT_SPKID_SUCCESS) 
	{
		xerror = TIT_TRN_Model_For_Test ((void*)md_index, mdl_cache);
	}
	
	if (xerror == TIT_SPKID_SUCCESS)
	{
		xerror = (TIT_RET_CODE)ModelToDisk (mod_file, (void*)mdl_cache, SG_DATA_SIZE);
	}
	for (i = 0; i < valid_cnt; i++)
	{
		kfree(pbuf[i]);
	}

	return (-xerror);
}

int WavToProbability (const char* wav_file, void *model_list, int *md_index, int is_alaw)
{
	TIT_RET_CODE  xerror;
	char 	featbuf[3202];
	short  *b1, *b2;
	int    s1, s2, mdlindex, nchl = 1;
	bool result = false;
	
	xerror = TIT_SPKID_ERROR_MODEL;

	struct modelist_t *mlist = (struct modelist_t *)model_list;
	
	if (unlikely(!mlist))
		goto finish;
	
	if (is_alaw) {
		result = CreateWavALaw (wav_file, nchl, b1, s1, b2, s2);
		if (result)
			xerror = TIT_SCR_Buf_CutSil_NoCluster_Index ((short*)b1, s1, (void*)featbuf, NULL);
	} 

	else {
		result = CreateWav (wav_file, nchl, b1, s1, b2, s2);
		if (result)
			xerror = TIT_SCR_Buf_CutSil_Cluster_Index ((short*)b1, s1, (void*)featbuf, NULL);	/** same with vpm-1.0 */
	}

	if (xerror == TIT_SPKID_SUCCESS) {
		xerror = TIT_SCR_Index_Match ((void*)featbuf, (void**)mlist->sg_data, mlist->sg_score, mlist->sg_cur_size, mdlindex);
		*md_index = mdlindex;
	}

	if (result)
		kfree(b1);

finish:
	return (-xerror);
}

int WavToProbabilityAdvanced (const char* wav_file, void *model_list, float *sg_score, int *md_index, int is_alaw)
{
	TIT_RET_CODE  xerror;
	char 	featbuf[3202];
	short  *b1, *b2;
	int    s1, s2, mdlindex, nchl = 1;
	bool result = false;
	
	xerror = TIT_SPKID_ERROR_MODEL;

	struct modelist_t *mlist = (struct modelist_t *)model_list;
	
	if (unlikely(!mlist))
		goto finish;
	
	if (is_alaw) {
		result = CreateWavALaw (wav_file, nchl, b1, s1, b2, s2);
		if (result)
			xerror = TIT_SCR_Buf_CutSil_NoCluster_Index ((short*)b1, s1, (void*)featbuf, NULL);
	} 

	else {
		result = CreateWav (wav_file, nchl, b1, s1, b2, s2);
		if (result)
			xerror = TIT_SCR_Buf_CutSil_Cluster_Index ((short*)b1, s1, (void*)featbuf, NULL);	/** same with vpm-1.0 */
	}

	if (xerror == TIT_SPKID_SUCCESS) {
		xerror = TIT_SCR_Index_Match ((void*)featbuf, (void**)mlist->sg_data, sg_score, mlist->sg_cur_size, mdlindex);
		*md_index = mdlindex;
	}

	if (result)
		kfree(b1);

finish:
	return (-xerror);
}


