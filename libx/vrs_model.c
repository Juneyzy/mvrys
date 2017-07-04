
#include "sysdefs.h"
#include "vrs_model.h"
#include "vrs_rule.h"

INIT_MUTEX(modelist_lock);

static __rt_always_inline__ int sg_abstract_owner (const char *string, char *sg_owner)
{
	/* model formate: "%lu-%d" when upload with CBP, massive formate : %callid  **/
	return sscanf (string, "%[^.]", sg_owner);
}

static __rt_always_inline__ int sg_abstract_owner_ids (const char *string, target_id *tid, sg_id *vid)
{
	/* model formate: "%lu-%d" when upload with CBP, massive formate : %callid  **/
	if (sscanf (string, "%lu-%u.%*s", tid, vid) != 2)
		return -1;

	return 0;
}

int sg_get_max_models (char *model_realpath, time_t tt)
{
	DIR  *pDir;
	struct dirent *ent;
	int64_t total_models = 0;
	long long callid = 0;

	pDir = opendir (model_realpath);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
		             "%s, %s", strerror(errno), model_realpath);
		return 0;
	}

	while ((ent = readdir(pDir)) != NULL) {

		if (!strcmp(ent->d_name, ".") ||
		    		!strcmp (ent->d_name, "..")){
		    continue;
		}

		if(strstr(ent->d_name, "model") == NULL){
			continue;
		}

		if(-1 == sscanf(ent->d_name, "%llx%*s", &callid)){
			continue;
		}

		// 10 min
		if((tt - (callid >> 32 & 0xffffffff)) < 600){
			continue;
		}

		total_models ++;
	}

	closedir(pDir);

	return total_models;
}

int sg_get_upload_models_num (char *model_upload, int quiet)
{
	DIR  *pDir;
	struct dirent *ent;
	int64_t total_models = 0;
	uint64_t tid = 0;

	pDir = opendir (model_upload);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
		             "%s, %s", strerror(errno), model_upload);
		return 0;
	}

	while ((ent = readdir(pDir)) != NULL) {

		if (!strcmp(ent->d_name, ".") ||
		    		!strcmp (ent->d_name, "..")){
		    continue;
		}

		if(strstr(ent->d_name, "model") == NULL){
			continue;
		}

		if(1 != sscanf(ent->d_name, "%lu-%*s", &tid)){
			continue;
		}

		if (!quiet)
            rt_log_debug("tid = %lu", tid);

		total_models ++;
	}

	closedir(pDir);

	return total_models;
}


int sg_load_model_memory (const char *root, const char *model, struct modelist_t *list, int flags)
{
	target_id	tid;
	sg_id	vid;
	int xerror = -1, slot;
	char    path[256] = {0};
	struct vrmt_t	*_vrmt = NULL;
	int	wlen1, wlen2;

	if (unlikely (!model))
		goto finish;

	if (!(flags & ML_FLG_RIGN)) {
		if (sg_abstract_owner_ids (model, &tid, &vid) < 0)
			goto finish;
	}

	xerror = 0;

	if (!(flags & ML_FLG_RIGN))
		xerror = vrmt_query (tid, &slot, (struct vrmt_t **)&_vrmt);

	if (!xerror) {
        snprintf (path, 256 - 1, "%s/%s", root, model);
        xerror = WavToModelMemory(path, list->sg_data[list->sg_cur_size], &wlen1, &wlen2, W_NOALAW);
        if (0 != xerror)
        {
            rt_log_error(ERRNO_FATAL, "WavToModelMemory(%s) error(%d)", path, xerror);
		    goto finish;
		}
		else {
			/* model formate: "%lu-%d" when upload with CBP, massive formate : %callid  **/
			sg_abstract_owner (model, list->sg_owner[list->sg_cur_size]);
			list->sg_cur_size ++;
			xerror = 0;
		}
	}

finish:
	return xerror;
}

static __rt_always_inline__ int sg_load_model_file (const char *root, const char *model, struct modelist_t *list, int flags)
{
	target_id	tid;
	sg_id	vid;
	int xerror = -1, slot;
	char    path[256] = {0};
	struct vrmt_t	*_vrmt = NULL;

	if (unlikely (!model))
		goto finish;

	if (!(flags & ML_FLG_RIGN)) {
		if (sg_abstract_owner_ids (model, &tid, &vid) < 0)
			goto finish;
	}

	xerror = 0;

	if (!(flags & ML_FLG_RIGN))
		xerror = vrmt_query (tid, &slot, (struct vrmt_t **)&_vrmt);

	if (!xerror) {
		snprintf (path, 256 - 1, "%s/%s", root, model);
		if ( 0 != (xerror = ModelFromDisk (path, list->sg_data[list->sg_cur_size])) ) {
		    rt_log_error(xerror, "ModelFromDisk error");
			goto finish;
		}
		else {
			/* model formate: "%lu-%d" when upload with CBP, massive formate : %callid  **/
			sg_abstract_owner (model, list->sg_owner[list->sg_cur_size]);
			list->sg_cur_size ++;
			xerror = 0;
		}
	}

finish:

	return xerror;
}


void modelist_destroy(struct modelist_t *m)
{
	int i = 0;

	if(unlikely(!m))
		return;

	for(i = 0; i < m->sg_max_size; i ++) {
			kfree(m->sg_owner[i]);
			kfree(m->sg_data[i]);
	}

	kfree(m->sg_owner);
	kfree(m->sg_data);
	kfree(m->sg_score);

	kfree(m);
}

struct modelist_t *modelist_create (int64_t sg_max_size)
{
	int i = 0;
	struct modelist_t *mlnew;

	mlnew = (struct modelist_t *)kmalloc(sizeof(struct modelist_t), MPF_CLR, -1);
	if (unlikely(!mlnew)) {
		rt_log_error(ERRNO_FATAL,
	                "%s", strerror(errno));
		return NULL;
	}

	mlnew->sg_max_size = sg_max_size;

	mlnew->sg_owner = (char **)kmalloc(sizeof(char *) * mlnew->sg_max_size, MPF_CLR, -1);
	if (unlikely(!mlnew->sg_owner)){
		rt_log_error(ERRNO_FATAL,
              			"%s", strerror(errno));
		modelist_destroy (mlnew);
		return NULL;
	}
	mlnew->sg_data = (uint8_t **)kmalloc(sizeof(uint8_t *) * mlnew->sg_max_size, MPF_CLR, -1);
	if(unlikely(!mlnew->sg_data)){
		rt_log_error(ERRNO_FATAL,
              			"%s", strerror(errno));
		modelist_destroy (mlnew);
		return NULL;
	}

	rt_log_notice ("Allocating SGx Models & Scores Cache... %ld", mlnew->sg_max_size);
	for(i = 0; i < mlnew->sg_max_size; i ++) {
		mlnew->sg_owner[i] = (char *)kmalloc(SG_OWNR_SIZE, MPF_CLR, -1);
		mlnew->sg_data[i] = (uint8_t *)kmalloc(SG_DATA_SIZE, MPF_CLR, -1);
	}

	mlnew->sg_score = (float *)kmalloc((sizeof(float) * mlnew->sg_max_size), MPF_CLR, -1);
	if(unlikely(!mlnew->sg_score)){
		rt_log_error(ERRNO_FATAL,
              			"%s", strerror(errno));
		modelist_destroy (mlnew);
		return NULL;
	}

	return mlnew;
}

/** load model from a specific path */
int modelist_load_advanced_implicit (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total, int64_t *sg_models_total)
{
	DIR  *pDir;
	struct dirent *ent;
	struct modelist_t   *mlnew = NULL, *mlcurr =NULL;
	struct vrmt_t	*_vrmt = NULL;
	target_id	tid;
	sg_id	vid;
	int	slot = 0;
	uint64_t	begin, end;
	int64_t	bytes = 0,  sg_cur_size = 0, total_models = 0,  valid_models = 0;
	int	xerror;
	begin	=	rt_time_ms ();

	pDir = opendir(root_path);
	if (unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		return -1;
	}

	while ((ent = readdir(pDir)) != NULL) {
		if(!strcmp(ent->d_name,".") ||
		        !strcmp(ent->d_name,".."))
		    continue;
		sscanf(ent->d_name, "%lu-%u.%*s", &tid, &vid);

		total_models ++;

		if (flags & ML_FLG_RIGN)
			sg_cur_size ++;
		else
			if (!vrmt_query (tid, &slot, (struct vrmt_t **)&_vrmt))
				sg_cur_size ++;
	}

	closedir(pDir);

	/** sg_owner & sg_data can not be allocate when sg_cur_size equal with 0. And some unknown errors maybe occurs.
		To avoid the uncertainty during its running, modelist->sg_cur_size should be checked after checking modlist */
	if (!sg_cur_size) {
		kfree(mlnew);
		rt_log_notice ("Empty \"%s\"", root_path);
		goto finish;
	}

	mlnew = modelist_create (sg_cur_size);
	if(unlikely(!mlnew))
		return -1;

	pDir = opendir (root_path);
	if (unlikely (!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		modelist_destroy (mlnew);
		return -1;
	}

	/** load SG data and owner */
	while ((ent = readdir(pDir)) != NULL) {
		if (!strcmp(ent->d_name, ".") ||
		        !strcmp (ent->d_name, ".."))
			continue;

		total_models ++;

		xerror = sg_load_model_file (root_path, ent->d_name, mlnew, flags);
		if (!xerror) {
			valid_models ++;
			sg_cur_size ++;
			bytes += SG_DATA_SIZE;
		}
	}

	closedir(pDir);

rt_mutex_lock (&modelist_lock);
	mlcurr = default_modelist();
	default_modelist_set (mlnew);
	if(flags & ML_FLG_RLD ) /** reload model, so we need return memory back to system first. */
		modelist_destroy (mlcurr);
rt_mutex_unlock (&modelist_lock);

finish:
	end	=	rt_time_ms ();
	rt_log_notice ("*** Loading Model Database(\"%s\") Okay. Result=%ld/(%ld, %ld), Bytes=%ld(%fMB), Costs=%lf sec(s)",
		root_path, valid_models, mlnew->sg_cur_size, total_models, bytes, (double)bytes / (1024 * 1024),  (double)(end - begin) / 1000);

	*sg_valid_models = valid_models;
	*sg_valid_models_size = bytes;
	*sg_valid_models_total = mlnew->sg_cur_size;
	*sg_models_total = total_models;

	return valid_models;
}

int modelist_delta_load_model_to_memory (const char *root_path, const char *wave, struct modelist_t *list, int flags,
                              int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total, int64_t *sg_models_total)
{
	int64_t	bytes = 0,  sg_cur_size = 0, valid_models = 0;
	int	xerror;

	if (unlikely (!list)) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist invalid (%p)", list);
		return -1;
	}

	if (flags & ML_FLG_RLD ||
		flags & ML_FLG_NEW) {
		list->sg_cur_size  = 0;
		*sg_valid_models = 0;
		*sg_valid_models_size = 0;
		*sg_valid_models_total = 0;
		*sg_models_total = 0;
	}

	/** Modelist full */
	if (list->sg_cur_size >= list->sg_max_size) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist memory not enough (%ld, %ld)",
	                		list->sg_cur_size, list->sg_max_size);
		return -1;
	}


	if ((list->sg_cur_size + sg_cur_size) > list->sg_max_size) {
	        rt_log_error(ERRNO_FATAL,
	                    "Modelist memory not enough (%ld, %ld)",
	                    (list->sg_cur_size + sg_cur_size), list->sg_max_size);
	        goto finish;
	}

	xerror = sg_load_model_memory (root_path, wave, list, flags);
	if (!xerror) {
			valid_models ++;
			sg_cur_size ++;
			bytes += SG_DATA_SIZE;
	} else {
	    return -1;
	}

finish:
	*sg_valid_models += valid_models;
	*sg_valid_models_size += bytes;
	*sg_valid_models_total += sg_cur_size;
	*sg_models_total = 1;

	return valid_models;
}


/** load model from root_path, Delta-Shaped */
int modelist_load_advanced_explicit (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					struct modelist_t *list /** created outside. */,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total,
				    int64_t *sg_models_total, time_t tt)
{

	DIR  *pDir;
	struct dirent *ent;

	uint64_t	begin, end;
	int64_t	bytes = 0,  sg_cur_size = 0, total_models = 0,  valid_models = 0;
	int	xerror;
	long long callid = 0;
	begin	=	rt_time_ms ();

	if (unlikely (!list)) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist invalid (%p)", list);
		return -1;
	}

	if (flags & ML_FLG_RLD ||
		flags & ML_FLG_NEW) {
		list->sg_cur_size  = 0;
		*sg_valid_models = 0;
		*sg_valid_models_size = 0;
		*sg_valid_models_total = 0;
		*sg_models_total = 0;
	}

	/** Modelist full */
	if (list->sg_cur_size >= list->sg_max_size) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist memory not enough (%ld, %ld)",
	                		list->sg_cur_size, list->sg_max_size);
		return -1;
	}

	/** sg_owner & sg_data can not be allocate when sg_cur_size equal with 0. And some unknown errors maybe occurs.
		To avoid the uncertainty during its running, modelist->sg_cur_size should be checked after checking modlist */
	/** precheck memory for Delta-models */
	if ((list->sg_cur_size + sg_cur_size) > list->sg_max_size) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist memory not enough (%ld, %ld)",
	                (list->sg_cur_size + sg_cur_size), list->sg_max_size);
		goto finish;
	}

	pDir = opendir (root_path);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		return -1;
	}

	/** load SG data and owner */
	while ((ent = readdir(pDir)) != NULL) {

		if (!strcmp(ent->d_name, ".") ||
		        !strcmp (ent->d_name, "..")){
			continue;
		}

		if ((flags & ML_FLG_RIGN)) {

			if(STRSTR(ent->d_name, "model") == NULL){
				continue;
			}

			if(-1 == sscanf(ent->d_name, "%llx%*s", &callid)){
				continue;
			}

			// 10 min
			if((tt - (callid >> 32 & 0xffffffff)) < 600){
				continue;
			}
		}

		total_models ++;

		xerror = sg_load_model_file (root_path, ent->d_name, list, flags);
		if (!xerror) {
			valid_models ++;
			sg_cur_size ++;
			bytes += SG_DATA_SIZE;
		}
	}

	closedir(pDir);

finish:
	end	=	rt_time_ms ();
	rt_log_notice ("*** Delta Loading Model Database(\"%s\") Okay. Result=%ld/(%ld, %ld), Bytes=%ld(%fMB), Costs=%lf sec(s)",
		root_path, valid_models, list->sg_cur_size, total_models, bytes, (double)bytes / (1024 * 1024),  (double)(end - begin) / 1000);

	*sg_valid_models += valid_models;
	*sg_valid_models_size += bytes;
	*sg_valid_models_total += sg_cur_size;
	*sg_models_total += total_models;

	return valid_models;
}

int modelist_load_one_model(char *model_realpath, uint64_t callid, struct modelist_t *cur_list)
{
    int xerror = -1;

    if (unlikely (!model_realpath))
    {
        goto finish;
    }
    xerror = ModelFromDisk(model_realpath, cur_list->sg_data[cur_list->sg_cur_size]);
    if (xerror != 0)
    {
        rt_log_error(ERRNO_FATAL, "%s, %s", strerror(errno), model_realpath);
        goto finish;
    }
    sprintf(cur_list->sg_owner[cur_list->sg_cur_size], "%lu", callid),
    cur_list->sg_cur_size++;
finish:
    return xerror;
}

int modelist_load_by_user (const char *root_path, int flags /** ML_FLG_RIGN: load model without rule file */,
					struct modelist_t *list /** created outside. */,
					int64_t *sg_valid_models, int64_t *sg_valid_models_size, int64_t *sg_valid_models_total,
				    int64_t *sg_models_total, time_t tt)
{

	DIR  *pDir;
	struct dirent *ent;

	uint64_t	begin, end;
	int64_t	bytes = 0,  sg_cur_size = 0, total_models = 0,  valid_models = 0;
	int	xerror;
	long long callid = 0;
	begin	=	rt_time_ms ();

	if (unlikely (!list)) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist invalid (%p)", list);
		return -1;
	}

	if (flags & ML_FLG_RLD ||
		flags & ML_FLG_NEW) {
		list->sg_cur_size  = 0;
		*sg_valid_models = 0;
		*sg_valid_models_size = 0;
		*sg_valid_models_total = 0;
		*sg_models_total = 0;
	}

	/** Modelist full */
	if (list->sg_cur_size >= list->sg_max_size) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist memory not enough (%ld, %ld)",
	                		list->sg_cur_size, list->sg_max_size);
		return -1;
	}

	/** sg_owner & sg_data can not be allocate when sg_cur_size equal with 0. And some unknown errors maybe occurs.
		To avoid the uncertainty during its running, modelist->sg_cur_size should be checked after checking modlist */
	/** precheck memory for Delta-models */
	if ((list->sg_cur_size + sg_cur_size) > list->sg_max_size) {
		rt_log_error(ERRNO_FATAL,
	                "Modelist memory not enough (%ld, %ld)",
	                (list->sg_cur_size + sg_cur_size), list->sg_max_size);
		goto finish;
	}

	pDir = opendir (root_path);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		return -1;
	}

	/** load SG data and owner */
	while ((ent = readdir(pDir)) != NULL) {

		if (!strcmp(ent->d_name, ".") ||
		        !strcmp (ent->d_name, "..")){
			continue;
		}

        if (flags & ML_FLG_MOD_0) {
            if(!STRSTR(ent->d_name, "-0.model")){
                continue;
            }
        }
        if (flags & ML_FLG_EXC_MOD_0) {
            if(STRSTR(ent->d_name, "-0.model") || !STRSTR(ent->d_name, "model")){
                continue;
            }
        }

		if ((flags & ML_FLG_RIGN)) {
			if(-1 == sscanf(ent->d_name, "%llx%*s", &callid)){
				continue;
			}

			// 10 min
			if((tt - (callid >> 32 & 0xffffffff)) < 600){
				continue;
			}
		}

		total_models ++;

		xerror = sg_load_model_file (root_path, ent->d_name, list, flags);
		if (!xerror) {
			valid_models ++;
			sg_cur_size ++;
			bytes += SG_DATA_SIZE;
		}
	}

	closedir(pDir);

finish:
	end	=	rt_time_ms ();
	rt_log_notice ("*** Delta Loading Model Database(\"%s\") Okay. Result=%ld/(%ld, %ld), Bytes=%ld(%fMB), Costs=%lf sec(s)",
		root_path, valid_models, list->sg_cur_size, total_models, bytes, (double)bytes / (1024 * 1024),  (double)(end - begin) / 1000);

	*sg_valid_models += valid_models;
	*sg_valid_models_size += bytes;
	*sg_valid_models_total += sg_cur_size;
	*sg_models_total += total_models;

	return valid_models;
}


