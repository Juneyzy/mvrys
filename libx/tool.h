#ifndef __TOOL_H__
#define __TOOL_H__

#define	SG_NAME_SIZE	128
struct sg_original_data_t {
	char		sgo_root[512];
	char		sgo_wav[SG_NAME_SIZE];	/** file without realpath */
	uint8_t			*sgo_model;
	void				*tool;
	struct list_head	list, wqe_list;
};

enum vrstool_job {
	VRSTOOL_DEFAULT_MODEL = 0,
	VRSTOOL_MASSIVE_MODEL,
	VRSTOOL_BATCH_MODEL,
	VRSTOOL_CONVERT_MODEL,
};

#define	MAX_INWORK_CORES		16
struct rt_vrstool_t {
	int   (*engine_init) (char *conffile, char* pathname);
	int	(*model_cvrto_ops) (const char *wav_file, const char* mod_file, int *wlen1, int *wlen2, int is_alaw);
	int	(*model_cvrto_cache_ops) (const char *wav_file, uint8_t *mdl_cache, int *wlen1, int *wlen2, int is_alaw);
	int	(*voice_recognition_ops) (const char* wav_file, void *mlist, int *md_index, int is_alaw);
	int	(*voice_recognition_advanced_ops) (const char* wav_file, void *mlist, float *sg_score, int *md_index, int is_alaw);
	
	atomic64_t	sg_batch,	sg_batch_counter,	sg_batch_size;
	atomic64_t	categories, category_entries;
	
	FILE	*fp;
	int	threshold;
	enum vrstool_job job;
	char	*category_dir, *model_dir, *batch_dir;
	int64_t	sg_valid_models, sg_valid_models_size, sg_valid_models_total, sg_models_total;

	atomic64_t	circulating_factor;	/** used for round robin */

	int			allowded_max_tasks, cur_tasks;

	rt_mutex	modelist_lock;
	void	*modelist;	/** model list */
	
	struct list_head	wqe[MAX_INWORK_CORES];
	rt_mutex			wlock[MAX_INWORK_CORES];
	int				wcnt[MAX_INWORK_CORES];
};

extern struct rt_vrstool_t *engine_default_ops  ();

#endif

