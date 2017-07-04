#ifndef __VRS_MODEL_H__
#define __VRS_MODEL_H__

#include "model.h"

int	WavToProbability (const char* wav_file, void *, int *md_index, int is_alaw);
int WavToProbabilityAdvanced (const char* wav_file, void *, float *sg_score, int *md_index, int is_alaw);
int   WavToModelMemory (const char *wav_file, uint8_t *mdl_cache, int *wlen1, int *wlen2, int is_alaw);
int   WavToModel (const char *wav_file, const char* mod_file, int *wlen1, int *wlen2, int is_alaw);
int   ModelToDisk (const char *mfile, void *model, int msize);
int   VRSEngineInit (char *conf, char *path);
int   ModelFromDisk (const char *mfile, void *model);
int   WavToModelEx(struct sample_file_t *wav_file, int cnt, const char* mod_file);

enum {
	SG_X_REG = 101,
	SG_X_ADD = 102,
	SG_X_DEL = 103,
	SG_X_MASS_QUERY = 104,
	SG_X_CATE = 105,
	SG_X_TARGET_QUERY = 106,
	SG_X_BOOST = 107
};

extern rt_mutex 	modelist_lock;

extern struct modelist_t *default_modelist ();

extern void default_modelist_set (struct modelist_t *__new);

/** Create a Model List, sg_max_size (<= SG_MODEL_THRESHOLD). */
extern struct modelist_t *modelist_create (int64_t sg_max_size);

/** Destroy an existent Model List */
extern void modelist_destroy (struct modelist_t *m);

/** Get model count for a specific directory. */
extern int sg_get_max_models (char *model_realpath, time_t tt);

/** 从上传的样本目录里获取有效model数目 */
extern int sg_get_upload_models_num (char *model_upload, int quiet);

/** Load model from root_path to internal Model List (default Model List) */
extern int modelist_load_advanced_implicit  (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total, int64_t *sg_models_total);

/** Load model from root_path, Delta-Shaped */
extern int modelist_load_advanced_explicit (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					struct modelist_t *list,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total, int64_t *sg_models_total, time_t cur_time);

/** Load a model file to Model List */
extern int modelist_delta_load_model_to_memory (const char *root_path, const char *wave, struct modelist_t *list, int flags,
                              int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total, int64_t *sg_models_total);

extern int modelist_load_one_model(char *model_realpath, uint64_t callid, struct modelist_t *cur_list);
extern int modelist_load_by_user (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					struct modelist_t *list /** created outside. */,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total,
				    int64_t *sg_models_total, time_t tt);

#endif
