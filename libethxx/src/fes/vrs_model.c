
#include "sysdefs.h"
#include "vrs_model.h"
#include "vrs_rule.h"

#define TIT_TRN_Model_CutAll(a, b, c, d)
#define TIT_SCR_Buf_AddCfd_CutSil_Cluster(a, b, c, d, e, f, g)

#define	SG_DATA_SIZE    2048
#define	SG_OWNR_SIZE	256

/** Sound  Groove */
struct model_t {
	char    *sg_owner;
	uint8_t    sg_data[SG_DATA_SIZE];
	struct  list_head   list;
};

struct owner_t {
	int	tid;
	int	vid;
};

struct modelist_t {
	int     sg_cur_size;
	struct  list_head   head;

	rt_mutex    lock;

	float		*sg_score;
	uint8_t	*sg_data;
	char		*sg_owner;
	//struct owner_t *owner;
};

struct modelist_t   *ModeList;

typedef struct _chunk_riff {
	unsigned int ck_id;		/* chunk id - "RIFF"*/
	unsigned int ck_size;		/* chunk size, not include itself, file length-8*/
	unsigned int file_tag;	/* file tag "WAVE"*/
} chunk_riff;

typedef struct _chunk_fmt {
	unsigned int ck_id;		/* chunk id - "fmt "*/
	unsigned int ck_size;		/* chunk size, not include itself*/
	unsigned short fmt_code;	/* format code. WAVE_FORMAT_xxx */
	unsigned short nChannel;	/* number of interleaved sound track*/
	int	 nSamplesPerSec;	/*Sampling rate(blocks per second) */
	unsigned int nAvgBytesPerSec;	/*Data rate*/
	unsigned short nBlockAlign;	/*Data block size(bytes)*/
	unsigned short wBitsPerSample;	/*Bits per sample*/
} chunk_fmt;

typedef struct _chunk_fact{
	unsigned int ck_id;		/* chunk id - "fact"*/
	unsigned int ck_size;		/* chunk size, not include itself */
	unsigned int nSample;		/* number of samples*/
} chunk_fact;

typedef struct _chunk_data{
	unsigned int ck_id;		/* chunk id - "data"*/
	unsigned int ck_size;		/* chunk size, not include itself */
} chunk_data;


enum command
{
	ADD = 104,
	DEL = 105,
	MASS = 107,
	COMP = 108
};

#define WAVE_FORMAT_PCM		0x0001	/* PCM */
#define WAVE_FORMAT_ALAW 		0x0006	/* - bit ITU - T G.711 A - law */
#define WAVE_FORMAT_MULAW 	0x0007 	/* - bit ITU - T G.711 µ - law */

#define ML_FLG_NEW  (1 << 0)
#define ML_FLG_RLD   (1 << 1)

static inline void modelist_destroy(struct modelist_t *m)
{
	if(!likely(!m))
		return;

	struct model_t *_this, *p;

	rt_mutex_lock(&m->lock);

	list_for_each_entry_safe(_this, p, &m->head, list) {
	    list_del(&_this->list);
	    kfree(_this->sg_owner);
	    kfree(_this);
	}
	
	kfree(m->sg_owner);
	kfree(m->sg_data);
	kfree(m->sg_score);

   	rt_mutex_unlock(&m->lock);
	rt_mutex_destroy(&m->lock);

	rt_log_debug("free ML: %p", m);
}

static inline struct modelist_t *modelist_create()
{	
	struct modelist_t *mlnew;
	
	mlnew = (struct modelist_t *)kmalloc(sizeof(struct modelist_t), MPF_CLR, -1);
	if(likely(mlnew)) {
		printf("ML new: %p\n", mlnew);
		INIT_LIST_HEAD(&mlnew->head);
		rt_mutex_init(&mlnew->lock, NULL);
	
	    	return mlnew;
	}
	
	rt_log_error(ERRNO_FATAL,
	                "%s", strerror(errno));
	return mlnew;
}

/** load model from a specific path */
int modelist_load(const char *root_path, struct modelist_t **ml, int flags)
{
	int i = 0, k = 0;
	DIR  *pDir;
	struct dirent *ent;
	struct modelist_t   *mlnew = NULL, *mlcurr =NULL;
	char    path[256] = {0};
	char    sg_owner[256] = {0};
	FILE    *fp;
	target_id	tid;
	sg_id	vid;
	int	slot = 0;
	struct vrmt_t	*_vrmt = NULL;
	
	mlnew = modelist_create();
	if(unlikely(!mlnew))
		return -1;
	
	pDir = opendir(root_path);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		return -1;
	}
	
	while((ent = readdir(pDir)) != NULL) {
		if(!strcmp(ent->d_name,".") || 
		        !strcmp(ent->d_name,".."))
		    continue;
		sscanf(ent->d_name, "%lu-%u.%*s", &tid, &vid);

		if (!vrmt_lookup(tid, &slot, (struct vrmt_t **)&_vrmt))
			mlnew->sg_cur_size ++;
	}

	closedir(pDir);
	
	mlnew->sg_owner = (char *)kmalloc(SG_OWNR_SIZE * mlnew->sg_cur_size, MPF_CLR, -1);
	if(unlikely(!mlnew->sg_owner)){
		return -1;
	}
	
	mlnew->sg_data = (void *)kmalloc(SG_DATA_SIZE * mlnew->sg_cur_size, MPF_CLR, -1);
	if(unlikely(!mlnew->sg_data)){
		kfree(mlnew->sg_owner);
		return -1;
	}

	mlnew->sg_score = (float *)kmalloc((sizeof(float) * mlnew->sg_cur_size), MPF_CLR, -1);
	if(unlikely(!mlnew->sg_score)){
		kfree(mlnew->sg_owner);
		kfree(mlnew->sg_data);
		return -1;
	}

	pDir = opendir(root_path);
	if(unlikely(!pDir)) {
		rt_log_error(ERRNO_FATAL,
	                "%s, %s", strerror(errno), root_path);
		kfree(mlnew->sg_owner);
		kfree(mlnew->sg_data);
		kfree(mlnew->sg_score);
		return -1;
	}
	
	i = k = 0;
	/** load SG data and owner */
	while((ent = readdir(pDir)) != NULL) {
		if(!strcmp(ent->d_name, ".") || 
		        !strcmp(ent->d_name, ".."))
			continue;

		snprintf(path, 256 - 1, "%s/%s", root_path, ent->d_name);
		
		fp = fopen(path, "r");
		if(unlikely(!fp))
			continue; 
		
		i = k;
		if(likely(i < mlnew->sg_cur_size)) {
			memset64(sg_owner, 0, 256);
			sscanf(ent->d_name, "%[^.].%*s", sg_owner);
			memcpy64((void *)(&mlnew->sg_owner[0] + i * SG_OWNR_SIZE), sg_owner, strlen(sg_owner));

			if(fread((void *)&mlnew->sg_data[i * SG_DATA_SIZE], 1, SG_DATA_SIZE, fp) < SG_DATA_SIZE)
				printf("__%s, %d, %d, %s, %s\n", __func__, __LINE__, i, sg_owner, (char *)(void *)&mlnew->sg_data[i * SG_DATA_SIZE]);

			k ++;
		}
		fclose(fp);
	}

	closedir(pDir);

	mlcurr = *ml;
	*ml = mlnew;

	if(flags & ML_FLG_RLD ) 
		/** reload model, so we need return memory back to system first. */
		modelist_destroy(mlcurr);

	return 0;
}

int modelist_score(float **score, int *m)
{
	*score = ModeList->sg_score;
	*m = ModeList->sg_cur_size;

	return 0;
}

/** */
int modelist_match(const char *wfile)
{
	wfile = wfile;

	/** model index */
	return 1;
}

void modelist_test()
{
	printf("%p\n", ModeList);
	modelist_load("../../vrs", (struct modelist_t **)&ModeList, ML_FLG_RLD);
	printf("%p\n", ModeList);

	modelist_load("../../vrs", (struct modelist_t **)&ModeList, ML_FLG_RLD);
	printf("%p\n", ModeList);
	
}

