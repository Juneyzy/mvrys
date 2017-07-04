#ifndef __MODEL_H__
#define __MODEL_H__

#include <stdint.h>

#define	SG_INDEX_SIZE	1600
#define	SG_DATA_SIZE    1608
#define	SG_OWNR_SIZE	32


#define ML_FLG_NEW			(1 << 0)	/** unused */
#define ML_FLG_RLD			(1 << 1)	/** free previous mdlist after load new one */
#define ML_FLG_RIGN			(1 << 2)	/** ignore vrmt table, ignore tid */
#define ML_FLG_MOD_0	    (1 << 3)    /** only load merged model xxx-0.model */
#define ML_FLG_EXC_MOD_0    (1 << 4)    /** load model exclude xxx-0.model */

#define	W_NOALAW		0	/** file with wave head */
#define	W_ALAW		1	/** file without wave head */

#define SG_MODEL_THRESHOLD	100000
/** Sound  Groove */
struct modelist_t {
	int64_t     sg_cur_size, sg_max_size;
	float       *sg_score;
	uint8_t     **sg_data;
	char        **sg_owner;
	int         flags;
};

#define FILE_NUM_PER_TARGET    20
struct sample_file_t
{
    char sample_realpath[256];
    int  wlen1;
    int  wlen2;
    int  is_alaw;
};


#ifdef __cplusplus
extern "C" {

int WavToModelEx(struct sample_file_t *wav_file, int cnt, const char* mod_file);

int   ModelToDisk (const char *mfile, void *model, int msize);
int   ModelFromDisk (const char *mfile, void *model);
int   WavToModel (const char *wav_file, const char* mod_file, int *wlen1, int *wlen2, int is_alaw);
int   WavToModelMemory (const char *wav_file, uint8_t *mdl_cache, int *wlen1, int *wlen2, int is_alaw);
int	WavToProbability (const char* wav_file, void *, int *md_index, int is_alaw);
int WavToProbabilityAdvanced (const char* wav_file, void *, float *sg_score, int *md_index, int is_alaw);
int   VRSEngineInit (char *conf, char *path);
extern struct modelist_t *default_modelist ();
extern void default_modelist_set (struct modelist_t *__new);

}

#endif
#endif
