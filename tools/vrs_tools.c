
#include "sysdefs.h"
#include "vrs.h"


#define	logd(format, ...) fprintf(stdout, "(%s,%d) >> "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define	loge(format, ...) fprintf(stdout, "(%s,%d) >> "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define	logi(format, ...) fprintf(stdout, "(%s,%d) >> "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)

static LIST_HEAD(sg_original_list);
static INIT_MUTEX(sg_original_lock);

static LIST_HEAD(sg_batch_list);
static INIT_MUTEX(sg_batch_lock);

struct rt_vrstool_t	VRSBatcher = {
	.sg_batch_counter = ATOMIC_INIT (0),
	.sg_batch = ATOMIC_INIT (0),
	.sg_batch_size = ATOMIC_INIT (0),
	.categories = ATOMIC_INIT (0),
	.category_entries = ATOMIC_INIT (0),
	.modelist		=	NULL,
	.modelist_lock	= 	PTHREAD_MUTEX_INITIALIZER,
	.category_dir	=	"./data.cat",
	.model_dir	=	"./temp.model",
	.batch_dir	=	"./data",
	.threshold	=	65,
	.circulating_factor = ATOMIC_INIT (0),
	.allowded_max_tasks = MAX_INWORK_CORES,
	.cur_tasks	=	4,
};

static __rt_always_inline__ int sg_file_size (const char *file_path)
{
	int filesize = -1;
	struct stat statbuff;

	if (!stat(file_path, &statbuff))
		filesize = statbuff.st_size;

	return filesize;
}

static __rt_always_inline__ void _mkdir (const char * path)
{
	if (!rt_dir_exsit (path)) {
		mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}
}

static __rt_always_inline__ void sg_sprintf_model (const char *token, const char *root_path, char *model, size_t s)
{
#if 0
	target_id tid;
	int	vid;

	vrmt_random (&tid, &vid);
	snprintf (model, s -1, "%s/%lu-%d.model", root_path, tid, vid);
#else
	snprintf (model, s -1, "%s/%s.model", root_path, token);
#endif
}

static __rt_always_inline__
int massive_model_load (struct rt_vrstool_t	 *tool)
{
	int xerror;
	uint64_t	begin, end;

	begin	=	rt_time_ms ();
	xerror = modelist_load_advanced_implicit (tool->model_dir, 	(ML_FLG_RLD | ML_FLG_RIGN),
							&tool->sg_valid_models, &tool->sg_valid_models_size, &tool->sg_valid_models_total, &tool->sg_models_total);
	end		=	rt_time_ms ();

	logi ("*** Loading Model Database(\"%s\") Okay. Result=%ld/(%ld, %ld), Bytes=%ld(%fMB), Costs=%lf sec(s)",
		tool->model_dir, tool->sg_valid_models, tool->sg_valid_models_total, tool->sg_models_total, tool->sg_valid_models_size, (double)tool->sg_valid_models_size / (1024 * 1024),  (double)(end - begin) / 1000);

	return xerror;
}

static __rt_always_inline__ struct rt_vrstool_t	*vrstools ()
{
	return (struct rt_vrstool_t	*)&VRSBatcher;
}

void vrstools_config (struct rt_vrstool_t	*tool)
{
	char		logfile[256];

	_mkdir (tool->category_dir);

	snprintf (logfile, 255, "%s/%s", tool->category_dir, "category_result");
	tool->fp = fopen (logfile, "a+");
	if (unlikely (!tool->fp)) {
		loge ("%s",
			strerror(errno));
	}

	int	i	=	0;
	for (i = 0; i < tool->allowded_max_tasks; i++) {
		rt_mutex_init (&tool->wlock[i], NULL);
		INIT_LIST_HEAD (&tool->wqe[i]);
	}
}

void vrstools_free (struct rt_vrstool_t	*tool)
{
	kfree (tool->batch_dir);
	kfree (tool->model_dir);
	kfree (tool->category_dir);
}


static __rt_always_inline__
void massive_list_add_entry (const char *root_path, const char *file, struct sg_original_data_t *_this)
{
	sprintf (_this->sgo_root, "%s", root_path);
	sprintf (_this->sgo_wav, "%s", file);

	INIT_LIST_HEAD (&_this->list);
	list_add_tail (&_this->list, &sg_original_list);
}

static __rt_always_inline__ void massive_list_destroy ()
{
	struct sg_original_data_t	*_this, *p;

	rt_mutex_lock (&sg_original_lock);
	list_for_each_entry_safe (_this, p, &sg_original_list, list) {
		list_del (&_this->list);
		kfree (_this->sgo_model);
		kfree (_this);
	}
	rt_mutex_unlock (&sg_original_lock);
}


int	massive_model_convert (struct rt_vrstool_t	*tool)
{
	char	*model_path = "./temp.model";
	DIR  *pDir;
	struct dirent *ent;
	int	wlen1, wlen2, sg_cur_size = 0, mdl_cur_size = 0, l;
	int64_t	bytes = 0;
	char	model_file[256], wave_file[256] = {0};
	uint64_t	begin = 0, end = 0;
	uint64_t	callid;
	char		dir[5], suffix[5];
	const int	s = sizeof (struct sg_original_data_t);

	pDir = opendir (tool->batch_dir);
	if (unlikely(!pDir)) {
		loge (
	                "%s, %s", strerror(errno), tool->batch_dir);
		goto finish;
	}

	if (likely (tool->model_dir))
		model_path = (char *)tool->model_dir;
	else
        	tool->model_dir = model_path;

	if (!rt_dir_exsit (model_path))
		mkdir (model_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	begin	=	rt_time_ms ();

	/** SG original file list */
	while ((ent = readdir (pDir)) != NULL) {

		if (!strcmp (ent->d_name,".") ||
		        !strcmp (ent->d_name,".."))
		    continue;
		if (strstr (ent->d_name, "up"))
			sscanf (ent->d_name, "%lu_%2s.%3s", &callid, dir, suffix);
		else
			sscanf (ent->d_name, "%lu_%4s.%3s", &callid, dir, suffix);

		struct sg_original_data_t *_this;

		_this = (struct sg_original_data_t *) kmalloc (s, MPF_CLR, -1);
		if (unlikely (!_this))
			continue;

		_this->sgo_model = (uint8_t *) kmalloc (SG_DATA_SIZE, MPF_CLR, -1);
		if (unlikely (!_this->sgo_model)) {
			kfree (_this);
			continue;
		}

		sg_cur_size ++;

		sg_sprintf_model (ent->d_name, model_path, model_file, 256);
		snprintf (wave_file, 255, "%s/%s", tool->batch_dir, ent->d_name);

		if (WavToModelMemory (wave_file, _this->sgo_model, &wlen1, &wlen2, W_NOALAW) ||
			ModelToDisk (model_file, _this->sgo_model, SG_DATA_SIZE)) {
			kfree (_this->sgo_model);
			kfree (_this);
			continue;

		} else {

			massive_list_add_entry (tool->batch_dir, ent->d_name, _this);
			mdl_cur_size ++;
			l = sg_file_size (wave_file);
			if (l > 0)
				bytes += l;
		}
	}

	atomic64_add (&tool->sg_batch, mdl_cur_size);
	atomic64_add (&tool->sg_batch_size, bytes);

	closedir (pDir);

	end		=	rt_time_ms ();

finish:

	logi ("*** Convert Models: %d(M)/%d(W) from \"%s\" to \"%s\", Bytes=%ld(%fMB), Cost=%f sec(s)",
					mdl_cur_size, sg_cur_size, tool->batch_dir, model_path,
					bytes, (double)bytes / (1024 * 1024),
					(double)(end - begin) / 1000);

	return mdl_cur_size;
}

int __massive_category (const char *category_dir, int mdl_index, char **so, float *s, int m, int threshold,
	struct sg_original_data_t *_this)
{
	int i = 0;
	int __cate = 0;
	char	category_cmd[1024], _category_file[1024];
	char _category_dir[256];

	snprintf (_category_dir, 256, "%s/category_%d_%s", category_dir, mdl_index, _this->sgo_wav);
	_mkdir (_category_dir);

	for (i = 0 ; i < m; i ++) {

		if (s[i] > threshold) {

			/** Target file **/
			snprintf (_category_file, 1023, "%s/%f-%s.wav", _category_dir, s[i], so[i]);

			/** Source file */
			snprintf (category_cmd, 1024, "cp %s/%s.wav %s", _this->sgo_root, so[i], _category_file);

			do_system (category_cmd);

			__cate ++;
		}
	}

	return __cate;
}

static __rt_always_inline__
int massive_category (struct rt_vrstool_t	*tool)
{
	struct sg_original_data_t	*_this, *p;
	char logfile[256] = {0};
	float *sg_score = NULL;
	char **so = NULL, sgo_wav[512];
	int md_index = 0, xerror = -1, m = 0;
	int	cate = 0;
	struct modelist_t	*cur_modelist;

	_mkdir (tool->category_dir);

	snprintf (logfile, 255, "%s/%s", tool->category_dir, "category_result");
	tool->fp = fopen (logfile, "a+");
	if (unlikely (!tool->fp)) {
		loge ("%s",
			strerror(errno));
		return 0;
	}

	rt_mutex_lock (&modelist_lock);
	cur_modelist = default_modelist ();
	if (likely (cur_modelist) &&
		cur_modelist->sg_cur_size > 0) {
		sg_score = kmalloc(sizeof (float) * cur_modelist->sg_cur_size, MPF_CLR, -1);
		m = cur_modelist->sg_cur_size;
		so = cur_modelist->sg_owner;
	}

	rt_mutex_lock (&sg_original_lock);
	list_for_each_entry_safe (_this, p, &sg_original_list, list) {
		memset (sg_score, 0, sizeof (float) * cur_modelist->sg_cur_size);
		list_del (&_this->list);
			snprintf (sgo_wav, 127, "%s/%s", tool->batch_dir, _this->sgo_wav);
			xerror = WavToProbabilityAdvanced (sgo_wav, cur_modelist, sg_score, &md_index, W_ALAW);
			if ((xerror == 0) &&
				(md_index < m)) {
					cate = __massive_category (tool->category_dir, md_index, so, sg_score, m, tool->threshold, _this);
					if (cate >= 0)
						atomic64_add (&tool->category_entries, cate);
			}
			atomic64_inc (&tool->categories);
			atomic64_inc (&tool->sg_batch_counter);
	}

	rt_mutex_unlock (&sg_original_lock);
	rt_mutex_unlock (&modelist_lock);

	fclose (tool->fp);

	kfree (sg_score);

	return 0;
}

static __rt_always_inline__
void batch_list_add_entry (struct sg_original_data_t *_this, struct rt_vrstool_t *tool)
{
	tool = tool;

	INIT_LIST_HEAD (&_this->list);

	rt_mutex_lock (&sg_batch_lock);
	list_add_tail (&_this->list, &sg_batch_list);
	rt_mutex_unlock (&sg_batch_lock);
}

static __rt_always_inline__
void batch_wqe_add_entry (struct sg_original_data_t *_this, struct rt_vrstool_t *tool)
{
	int64_t	cur_circulating_factor = atomic64_inc (&tool->circulating_factor);
	int	hval	=	cur_circulating_factor % tool->cur_tasks;

	INIT_LIST_HEAD (&_this->list);
	INIT_LIST_HEAD (&_this->wqe_list);

	rt_mutex_lock (&sg_batch_lock);
	list_add_tail (&_this->list, &sg_batch_list);
	rt_mutex_unlock (&sg_batch_lock);


	rt_mutex_lock (&tool->wlock[hval]);
	list_add_tail (&_this->wqe_list, &tool->wqe[hval]);
	rt_mutex_unlock (&tool->wlock[hval]);
	tool->wcnt[hval] ++;

	//loge ("%s, enqueue(%d)", _this->sgo_wav, hval);
}

static __rt_always_inline__
void batch_list_destroy (struct	rt_vrstool_t	*tool)
{
	struct sg_original_data_t	*_this, *p;

	int	i	=	0;

	for (i = 0; i < tool->cur_tasks; i ++) {
		rt_mutex_lock (&tool->wlock[i]);
		list_for_each_entry_safe (_this, p, &tool->wqe[i], wqe_list) {
			list_del (&_this->wqe_list);
		}
		rt_mutex_unlock (&tool->wlock[i]);
	}

	rt_mutex_lock (&sg_batch_lock);
	list_for_each_entry_safe (_this, p, &sg_batch_list, list) {
		list_del (&_this->list);
		kfree (_this->sgo_model);
		kfree (_this);
	}
	rt_mutex_unlock (&sg_batch_lock);
}

static __rt_always_inline__
int	batch_list_load (struct rt_vrstool_t *tool)
{
	DIR  *pDir;
	struct dirent *ent;
	struct	sg_original_data_t	*_this;
	const int	s = sizeof (struct sg_original_data_t);
	int	size = 0;
	char	realpath[512];
	int64_t	sg_batch, sg_batch_size;
	uint64_t	begin, end;

	sg_batch = sg_batch_size = 0;

	pDir = opendir (tool->batch_dir);
	if (unlikely(!pDir)) {
		loge ("%s, %s",
			strerror(errno), tool->batch_dir);
		return -1;
	}

	begin	=	rt_time_ms ();

	/** SG original file list */
	while ((ent = readdir (pDir)) != NULL) {
		if (!strcmp (ent->d_name,".") ||
		        !strcmp (ent->d_name,".."))
		    continue;

		_this = (struct sg_original_data_t *) kmalloc (s, MPF_CLR, -1);
		if (unlikely (!_this))
			continue;

		_this->sgo_model = (uint8_t *)kmalloc (SG_DATA_SIZE, MPF_CLR, -1);
		if (unlikely (!_this->sgo_model)) {
			kfree (_this);
			continue;
		}

		sprintf (_this->sgo_wav, "%s", ent->d_name);
		if (tool->cur_tasks > 0)
			batch_wqe_add_entry (_this, tool);
		else
			batch_list_add_entry (_this, tool);

		sg_batch ++;

		snprintf (realpath, 511, "%s/%s", tool->batch_dir, _this->sgo_wav);
		size = sg_file_size (realpath);
		if (size > 0)
			sg_batch_size += size;

	}

	closedir (pDir);

	end		=	rt_time_ms ();

	atomic64_add (&tool->sg_batch, sg_batch);
	atomic64_add (&tool->sg_batch_size, sg_batch_size);

	char	wqe_summary[64] = {0};
	int i, l = 0;
	l += snprintf (wqe_summary + l, 64 -l - 1, "WQE(");
	for (i = 0; i < tool->cur_tasks; i ++) {
		l += snprintf (wqe_summary + l, 64 -l - 1, "%d, ", tool->wcnt[i]);
	}
	l += snprintf (wqe_summary + l, 64 -l - 1, ")");

	logd ("*** Batch Load: %ld wave(s), Bytes=%ld (%fMB), %s, Cost=%f sec(s)",
					atomic64_add (&tool->sg_batch, 0), atomic64_add (&tool->sg_batch_size, 0),
					(double)atomic64_add (&tool->sg_batch_size, 0) /(1024 * 1024),
					wqe_summary,
					(double)(end - begin) / 1000);

	return sg_batch;
}

static __rt_always_inline__ int
__batch_category (FILE *fp, int md_index, char **so, float *s, int m, int threshold,
	struct sg_original_data_t *_this)
{
	char *log, *logx;
	int l = 0, size, i = 0, offset;
	int __cate = 0;

	size = 128 * m;
	log = (char *)kmalloc (size, MPF_CLR, -1);
	if (unlikely (!log))
		return -1;

	logx = (char *)kmalloc (size + 256, MPF_CLR, -1);
	if (unlikely (!logx))
		return -1;

	for (i = 0 ; i < m; i ++) {
		if (s[i] > threshold) {
			l += snprintf (log + l, size - 1 - l, "\t(%d, \"%s\", %f) ", i, so[i], s[i]);
			__cate ++;
		}
	}

	if (__cate > 1 ) {
		offset = snprintf (logx, size + 256, "\n\"%s (%d)\", %d\n", _this->sgo_wav, md_index, __cate);
		snprintf (logx + offset, size + 256, "%s", log);
		//printf (logx);
		rt_file_write(fp, logx, l, NULL);
	}

	kfree (log);
	kfree (logx);

	return __cate;
}


int __batch_category_x (const char *category_dir, int mdl_index, char **so, float *s, int m, int threshold,
	struct sg_original_data_t *_this)
{
	int i = 0;
	int __cate = 0;
	char	category_cmd[1024], _category_file[1024];
	char _category_dir[256];

	snprintf (_category_dir, 256, "%s/category_%d_%s", category_dir, mdl_index, _this->sgo_wav);
	_mkdir (_category_dir);

	for (i = 0 ; i < m; i ++) {

		if (s[i] > threshold) {

			/** Target file **/
			snprintf (_category_file, 1023, "%s/%f-%s.wav", _category_dir, s[i], so[i]);

			/** Source file */
			snprintf (category_cmd, 1024, "cp %s/%s.wav %s", _this->sgo_root, so[i], _category_file);

			do_system (category_cmd);

			__cate ++;
		}
	}

	return __cate;
}


static __rt_always_inline__ int
batch_category (struct rt_vrstool_t	*tool)
{
	struct sg_original_data_t	*_this, *p;
	char logfile[256] = {0};
	float *sg_score = NULL;
	char **so = NULL, sgo_wav[512];
	int md_index = 0, xerror = -1, m = 0;
	int	cate = 0;
	char	tm[128] = {0};
	struct modelist_t	*cur_modelist;

	_mkdir (tool->category_dir);

	rt_curr_tms2str ("%Y%m%d%H%M%S", tm, 128);

	snprintf (logfile, 255, "%s/%s-%s", tool->category_dir, "category_result", tm);
	tool->fp = fopen (logfile, "a+");
	if (unlikely (!tool->fp)) {
		loge ("%s",
			strerror(errno));
		return 0;
	}

	rt_mutex_lock (&modelist_lock);
	cur_modelist = default_modelist ();
	if (likely (cur_modelist) &&
		cur_modelist->sg_cur_size > 0) {
		sg_score = kmalloc(sizeof (float) * cur_modelist->sg_cur_size, MPF_CLR, -1);
		m = cur_modelist->sg_cur_size;
		so = cur_modelist->sg_owner;
	}

	rt_mutex_lock (&sg_batch_lock);
	list_for_each_entry_safe (_this, p, &sg_batch_list, list) {
		memset (sg_score, 0, sizeof (float) * cur_modelist->sg_cur_size);
		list_del (&_this->list);
			snprintf (sgo_wav, 511, "%s/%s", tool->batch_dir, _this->sgo_wav);

			xerror = WavToProbabilityAdvanced (sgo_wav, cur_modelist, sg_score, &md_index, W_ALAW);
			if ((xerror == 0) &&
				(md_index < m)) {
					cate = __batch_category (tool->fp, md_index, so, sg_score, m, tool->threshold, _this);
					if (cate >= 0)
						atomic64_add (&tool->category_entries, cate);
					atomic64_inc (&tool->categories);
			}
			atomic64_inc (&tool->sg_batch_counter);
	}

	rt_mutex_unlock (&sg_batch_lock);
	rt_mutex_unlock (&modelist_lock);

	fclose (tool->fp);
	kfree (sg_score);

	return 0;
}

static void *
rt_batch_category_routine (void __attribute__((__unused__))*argvs)
{
	struct	rt_vrstool_t	*tool;
	struct sg_original_data_t	*_this, *p;
	float *sg_score = NULL;
	char **so = NULL, sgo_wav[512];
	int md_index = 0, xerror = -1, m = 0;
	int	cate = 0, wqe;
	struct modelist_t	*cur_modelist;

	tool = vrstools ();

	wqe = (*(int *)argvs % tool->cur_tasks);

	cur_modelist = default_modelist ();
	if (likely (cur_modelist) &&
			cur_modelist->sg_cur_size > 0) {
		sg_score = kmalloc (sizeof (float) * cur_modelist->sg_cur_size, MPF_CLR, -1);
		m = cur_modelist->sg_cur_size;
		so = cur_modelist->sg_owner;
	}

	while (1) {
		rt_mutex_lock (&tool->wlock[wqe]);
		list_for_each_entry_safe (_this, p, &tool->wqe[wqe], wqe_list) {

			list_del (&_this->wqe_list);
			snprintf (sgo_wav, 127, "%s/%s", tool->batch_dir, _this->sgo_wav);

			xerror = WavToProbabilityAdvanced (sgo_wav, cur_modelist, sg_score, &md_index, W_ALAW);
			if ((xerror == 0) &&
				(md_index < m)) {
					cate = __batch_category (tool->fp, md_index, so, sg_score, m, tool->threshold, _this);
					if (cate >= 0)
						atomic64_add (&tool->category_entries, cate);
				atomic64_inc (&tool->categories);
			}
			atomic64_inc (&tool->sg_batch_counter);
		}
		rt_mutex_unlock (&tool->wlock[wqe]);
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static  int wqe_set[MAX_INWORK_CORES] = {0};

void vrstools_init_task (struct rt_vrstool_t *tool)
{
	int	i;
	struct rt_task_t	*task;

	for (i =0; i < tool->cur_tasks; i ++) {
		if (tool->cur_tasks < tool->allowded_max_tasks ) {
			task	=	(struct rt_task_t	*) kmalloc (sizeof (struct rt_task_t), MPF_CLR, -1);
			if (likely (task)) {
				sprintf (task->name, "SG%d Batch Category Task", i);
				wqe_set[i] = i;
				task->module = THIS;
				task->core = INVALID_CORE;
				task->prio = KERNEL_SCHED;
				task->argvs = (void *)&wqe_set[i];
				task->recycle = ALLOWED;
				task->routine = rt_batch_category_routine;

				task_registry (task);
			}
		}
	}
}

int vrstools_batch_category (struct	rt_vrstool_t	*tool)
{
	uint64_t	begin, end;

	batch_list_load (tool);

	begin = rt_time_ms ();
	batch_category (tool);

	while (atomic64_add (&tool->sg_batch_counter, 0) != \
			atomic64_add (&tool->sg_batch, 0));
		usleep (50000);

	end = rt_time_ms ();

	logd ("*** Batch Category: %ld wave(s), Bytes=%ld (%fMB), Category=%ld (entries=%ld), Cost=%f sec(s)",
					atomic64_add (&tool->sg_batch, 0), atomic64_add (&tool->sg_batch_size, 0),
					(double)atomic64_add (&tool->sg_batch_size, 0) /(1024 * 1024),
					atomic64_add (&tool->categories, 0), atomic64_add (&tool->category_entries, 0),
					(double)(end - begin) / 1000);

	batch_list_destroy (tool);

	return 0;

}

int vrstools_model_convert(struct rt_vrstool_t *tool)
{
	char	*model_path = "./temp.model";
	DIR  *pDir;
	struct dirent *ent;
	int	wlen1, wlen2, sg_cur_size = 0, mdl_cur_size = 0, l;
	int64_t	bytes = 0;
	char	model_file[512], wave_file[512] = {0};
	uint64_t	begin = 0, end = 0;
	uint64_t	callid;
	char		dir[5], suffix[5];
	uint8_t	*sgo_model = NULL;

	pDir = opendir (tool->batch_dir);
	if (unlikely(!pDir)) {
		loge ("%s, %s",
			strerror(errno), tool->batch_dir);
		goto finish;
	}

	if (likely (tool->model_dir))
		model_path = (char *)tool->model_dir;

	if (!rt_dir_exsit (model_path))
		mkdir (model_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	begin	=	rt_time_ms ();

	sgo_model = (uint8_t *)kmalloc (SG_DATA_SIZE, MPF_CLR, -1);
	if (unlikely (!sgo_model)) {
		loge ("%s, %s",
			strerror(errno), tool->batch_dir);
		goto finish;
	}

	/** SG original file list */
	while ((ent = readdir (pDir)) != NULL) {

		if (!strcmp (ent->d_name,".") ||
		        !strcmp (ent->d_name,".."))
		    continue;
		if (strstr (ent->d_name, "up"))
			sscanf (ent->d_name, "%lu_%2s.%3s", &callid, dir, suffix);
		else if (strstr (ent->d_name, "down"))
			sscanf (ent->d_name, "%lu_%4s.%3s", &callid, dir, suffix);
		else {
			loge ("Unreadable %s", ent->d_name);
			continue;
		}

		sg_cur_size ++;

		sg_sprintf_model (ent->d_name, model_path, model_file, 512);
		snprintf (wave_file, 511, "%s/%s", tool->batch_dir, ent->d_name);

		if (WavToModelMemory (wave_file, sgo_model, &wlen1, &wlen2, W_NOALAW) ||
			ModelToDisk (model_file, sgo_model, SG_DATA_SIZE)) {
			continue;

		} else {

			mdl_cur_size ++;
			l = sg_file_size (wave_file);
			if (l > 0)
				bytes += l;
		}
	}

	closedir (pDir);

	end		=	rt_time_ms ();

finish:

	kfree (sgo_model);

	logi ("*** Convert Models: %d(M)/%d(W) from \"%s\" to \"%s\", Bytes=%ld(%fMB), Cost=%f sec(s)",
					mdl_cur_size, sg_cur_size, tool->batch_dir, model_path,
					bytes, (double)bytes / (1024 * 1024),
					(double)(end - begin) / 1000);

	return mdl_cur_size;
}

int vrstools_massive_category (struct rt_vrstool_t	*tool)
{
	uint64_t	begin, end;

	massive_model_convert (tool);

	massive_model_load (tool);

	begin	=	rt_time_ms ();
	massive_category (tool);

	while (atomic64_add (&tool->sg_batch_counter, 0) != \
			atomic64_add (&tool->sg_batch, 0));
		usleep (100);

	end		=	rt_time_ms ();

	logd ("*** Massive Category: %ld wave(s), Bytes=%ld (%fMB), Category=%ld(entries=%ld), Cost=%f sec(s)",
					atomic64_add (&tool->sg_batch, 0), atomic64_add (&tool->sg_batch_size, 0),
					(double)atomic64_add (&tool->sg_batch_size, 0) /(1024 * 1024),
					atomic64_add(&tool->categories, 0), atomic64_add(&tool->category_entries, 0),
					(double)(end - begin) / 1000);

	massive_list_destroy ();

	return 0;
}

int vrstools_batch_category_adv (struct	rt_vrstool_t	*tool)
{
	uint64_t	begin, end;

	vrstools_init_task (tool);

	massive_model_load (tool);

	batch_list_load (tool);
	task_run ();	/** unused in this commit. */

	begin = rt_time_ms ();

	while (atomic64_read (&tool->sg_batch_counter) != \
			atomic64_read (&tool->sg_batch))
		usleep (500000);


	end = rt_time_ms ();

	logd ("*** Batch Category: %ld wave(s), Bytes=%ld (%fMB), Category=%ld (entries=%ld), Cost=%f sec(s)",
					atomic64_add (&tool->sg_batch, 0), atomic64_add (&tool->sg_batch_size, 0),
					(double)atomic64_add (&tool->sg_batch_size, 0) /(1024 * 1024),
					atomic64_add (&tool->categories, 0), atomic64_add (&tool->category_entries, 0),
					(double)(end - begin) / 1000);

	batch_list_destroy (tool);

	return 0;

}

static void vrstool_argv_parser(char **argvs, int n_args, struct rt_vrstool_t *tool)
{
    int i = 0;

    for(i = 0; i < n_args;  i++){
        if (!STRNCMP(argvs[i], "wave-dir", 8)){
            tool->batch_dir = argvs[i] + 9;
        }
        if (!STRNCMP(argvs[i], "model-dir", 9)){
            tool->model_dir = argvs[i] + 10;
        }
        if (!STRNCMP(argvs[i], "category-dir", 12)){
            tool->category_dir = argvs[i] + 13;
        }
    }
}

struct option long_opts[] =
{
    {"massive-category", 0, 0, 'm'},
    {"batch-category",   0, 0, 'b'},
    {"model-convert",    0, 0, 'c'},
    {"thread",           1, 0, 't'},
    {"threshold",        1, 0, 's'},
    {0, 0, 0, 0},
};

static int vrstool_parse_opts(int argc, char *argv[], struct rt_vrstool_t *tool)
{
    char opt = '\0';
    char **argvs = NULL;

    while ((opt = getopt_long(argc, argv, "mbct:s:", long_opts, NULL)) != -1)
    {
        switch(opt){
            case 'm':
                tool->job = VRSTOOL_MASSIVE_MODEL;
                break;
            case 'b':
                tool->job = VRSTOOL_BATCH_MODEL;
                break;
            case 'c':
                tool->job = VRSTOOL_CONVERT_MODEL;
                break;
            case 't':
                tool->cur_tasks = integer_parser(optarg, 0, tool->allowded_max_tasks);
                break;
            case 's':
                tool->threshold = integer_parser(optarg, 0, 100);
                break;
            case '?':
            default:
                break;
        }
    }
    if (optind == argc){
        goto finish;
    }

    size_t n_args = argc - optind;
    argvs = argv + optind;
    vrstool_argv_parser(argvs, n_args, tool);

finish:
    return 0;
}

static void *
vrstools_summary (void __attribute__((__unused__))*argvs)
{
	struct	rt_vrstool_t	*tool;
	tool = vrstools ();

	FOREVER{
		sleep (5);
		logd ("*** Batch Category: %ld/%ld",
					atomic64_add (&tool->sg_batch_counter, 0), atomic64_add (&tool->sg_batch, 0));
	}

	task_deregistry_id(pthread_self());

	return NULL;
}

static struct rt_task_t vrstoolSummary = {
	.module = THIS,
	.name = "VRS Tool Summary Task",
	.core = INVALID_CORE,
	.prio = KERNEL_SCHED,
	.argvs = NULL,
	.routine = vrstools_summary,
	.recycle = FORBIDDEN,
};

int main (int  __attribute__((__unused__))argc,
          char  __attribute__((__unused__))*argv[])
{
	struct rt_vrstool_t* tool = vrstools();

	vrstool_parse_opts(argc, argv, tool);
	VRSEngineInit ("SpkSRE.cfg", "/usr/local/etc/vpw");

	task_registry (&vrstoolSummary);

	vrstools_config (tool);

	switch(tool->job){
		case VRSTOOL_MASSIVE_MODEL:
			vrstools_massive_category (tool);
			break;
		case VRSTOOL_BATCH_MODEL:
			vrstools_batch_category_adv (tool);
			break;
		case VRSTOOL_CONVERT_MODEL:
			vrstools_model_convert (tool);
			break;
		default:
			break;
	}

	while (1) {
		sleep(1000);
	}

	return 0;
}

