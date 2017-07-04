#include "sysdefs.h"
#include "vrs_rule.h"

extern int boost_get_threshold(uint64_t tid, int t, int *trhld, const char *thr_path);

#define	V_HTARGET_BITS			12
#define	V_HTARGET_BUCKETS		(1 << V_HTARGET_BITS)

#define CLUE_TABLE_INIT(clue, clue_id, flags, score) {\
		(clue)->id = (clue_id);\
		(clue)->flags = (flags);\
		(clue)->score = (score);\
	}

int	default_global_score = 60;
static LIST_HEAD(vrmt_list);
static INIT_MUTEX(vrmt_lock);
static atomic_t	vrmt_target_cnt = ATOMIC_INIT(0);
static atomic_t	vrmt_clue_cnt = ATOMIC_INIT(0);

static struct hlist_head vrmt_hlist[V_HTARGET_BUCKETS];

static __rt_always_inline__ uint64_t hash_64(uint64_t val, unsigned int bits)
{
	uint64_t hash = val;

	/*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
	uint64_t n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;

	/* High bits are more random, so use them. */
	return hash >> (64 - bits);
}


static __rt_always_inline__ struct hlist_head *target_hash(target_id tid)
{
	int index;

	index = hash_64 (tid, V_HTARGET_BITS);

	return &vrmt_hlist[index];
}

static __rt_always_inline__ void vrmt_cset_init(struct vrmt_t *_this)
{
	int i;
	struct clue_t *clue;

	vrmt_for_each_clue(i, _this) {
		clue = &_this->clue[i];
		clue->id = INVALID_CLUE;
	}
}

static __rt_always_inline__ void vrmt_init()
{
	int	hash;

	atomic_set (&vrmt_target_cnt, 0);
	atomic_set (&vrmt_clue_cnt, 0);

	for (hash = 0; hash < V_HTARGET_BUCKETS; hash ++)
		INIT_HLIST_HEAD (&vrmt_hlist[hash]);
}

static __rt_always_inline__ struct vrmt_t *__vrmt_new (target_id tid)
{
	struct vrmt_t	*_this;
	const int scale_size	=	sizeof (struct vrmt_t);

	_this = (struct vrmt_t *)kmalloc(scale_size, MPF_CLR, -1);
	if (likely(_this)) {
		_this->id = tid;
		_this->target_score = default_global_score;
		vrmt_cset_init (_this);
		INIT_HLIST_NODE(&_this->hlist);
		hlist_add_head (&_this->hlist, target_hash (tid));
		atomic_add (&vrmt_target_cnt, 1);
	}

	return _this;
}

int vrmt_query (target_id tid, int *slot, struct vrmt_t **_vrmt)
{
	int	xerror = (-ERRNO_NO_ELEMENT);
	struct vrmt_t	*vrmt = NULL;
	struct hlist_node *p, *_this;
	struct hlist_head *head;

	if(likely((int64_t)tid == (int64_t)INVALID_TARGET)) {
		rt_log_error(ERRNO_INVALID_ARGU,
				"Invalid id %lu", tid);
		return xerror;
	}

	head = target_hash(tid);
rt_mutex_lock (&vrmt_lock);
	hlist_for_each_entry_safe (vrmt, _this, p, head, hlist) {
		if (vrmt->id == tid) {
			xerror = XSUCCESS;
			if (likely(slot))
				*slot = 1;
			if (likely(_vrmt))
				*_vrmt = vrmt;
			break;
		}
	}
rt_mutex_unlock (&vrmt_lock);
	return xerror;
}

int vrmt_query_lockless (target_id tid, int *slot, struct vrmt_t **_vrmt)
{
	int	xerror = (-ERRNO_NO_ELEMENT);
	struct vrmt_t	*vrmt = NULL;
	struct hlist_node *p, *_this;
	struct hlist_head *head;

	if(likely((int64_t)tid == (int64_t)INVALID_TARGET)) {
		rt_log_error(ERRNO_INVALID_ARGU,
				"Invalid id %lu", tid);
		return xerror;
	}

	head = target_hash(tid);
	hlist_for_each_entry_safe (vrmt, _this, p, head, hlist) {
		if (vrmt->id == tid) {
			xerror = XSUCCESS;
			if (likely(slot))
				*slot = 1;
			if (likely(_vrmt))
				*_vrmt = vrmt;
			break;
		}
	}

	return xerror;
}

int vrmt_query_copyout (target_id tid, int *slot, struct vrmt_t *_vrmt)
{
	int	xerror = (-ERRNO_NO_ELEMENT);
	struct vrmt_t	*vrmt;
	struct hlist_node *p, *_this;
	struct hlist_head *head;

	if(likely((int64_t)tid == (int64_t)INVALID_TARGET)) {
		rt_log_error(ERRNO_INVALID_ARGU,
				"Invalid id %lu", tid);
		return xerror;
	}

	head = target_hash(tid);
rt_mutex_lock (&vrmt_lock);
	hlist_for_each_entry_safe(vrmt, _this, p, head, hlist) {
		if (vrmt->id == tid) {
			xerror = XSUCCESS;
			if (likely(slot))
				*slot = vrmt->slot;
			if (likely(_vrmt))
				memcpy (_vrmt,  vrmt, sizeof (struct vrmt_t));
			break;
		}
	}
rt_mutex_unlock (&vrmt_lock);

	return xerror;
}

static __rt_always_inline__ int vrmt_add_clue (struct vrmt_t *_this, uint64_t tid, int clue_id, int flags, int score, const char *thr_path)
{
	int xerror = (-ERRNO_RANGE), i;
	struct clue_t	*clue;
	int threshold = 0;

	vrmt_for_each_clue (i, _this) {
		clue = &_this->clue[i];
		if (!valid_clue (clue->id)) {
			if ( !(score >= 0 && score <= 100) )  {
				boost_get_threshold(tid, score, &threshold, thr_path);
				score = threshold;
			} else {
                rt_log_notice("vrs senior: tid=%lu, type=%d, trhld=%d, clue_id=%d", tid, score, score, clue_id);
			}
			CLUE_TABLE_INIT(clue, clue_id, flags, score);
			_this->clues ++;
			atomic_inc (&vrmt_clue_cnt);
			xerror = XSUCCESS;
			goto finish;
		}
	}
	rt_log_debug ("Target %12lu add (%6u:%d:%3d), clues=%2d", _this->id, clue->id, clue->flags, clue->score, _this->clues);

finish:
	return xerror;
}

int vrmt_load(const char *vrmt_file,
					int __attribute__((__unused__)) option, const char *thr_path)
{

	char 	readline[1024] = {0};
	int		clue_id, flags, score, result = 0, xerror = XSUCCESS;
	uint64_t	begin, end;
	FILE* 		fp;
	target_id		tid;
	struct vrmt_t	*_this;


rt_mutex_lock (&vrmt_lock);

	begin = rt_time_ms ();

	vrmt_init();

	fp = fopen(vrmt_file, "r");
	if (unlikely(!fp))  {
		xerror = (-ERRNO_ACCESS_DENIED);
	    	rt_log_error(ERRNO_INVALID_ARGU,
				"%s", strerror(errno));
		goto finish;
	}

	while (fgets(readline, 1024, fp)) {

		tid = clue_id = flags = score = 0;
		if (sscanf(readline, "%u %lu %d %d", &clue_id, &tid, &flags, &score) == -1)
			continue;

		result = vrmt_query_lockless (tid, NULL, &_this);
		if (result == 0) {
			if (likely(_this))
				vrmt_add_clue (_this, tid, clue_id, flags, score, thr_path);
		} else {
			if (result < 0) {
				_this = __vrmt_new (tid);
				if (likely(_this))
					vrmt_add_clue (_this, tid, clue_id, flags, score, thr_path);
			}
		}
	}

	fclose(fp);

finish:
rt_mutex_unlock (&vrmt_lock);
	end = rt_time_ms ();
	rt_log_notice ("*** Loading VRMT Table(%s) %s. Targets=%d, Clues=%d, Costs=%lf sec(s)",
					vrmt_file, xerror ? "failure" : "success",
					atomic_add (&vrmt_target_cnt, 0), atomic_add (&vrmt_clue_cnt, 0),
					(double)(end - begin) / 1000);

	return xerror;
}


int vrmt_generate (const char *vrmt_file)
{

	target_id	tid;
	FILE 	*fp;
	int		clue_id, i = 0;
	int		score = 0;

	fp = fopen(vrmt_file, "w+");
	if(unlikely(!fp)) {
	    	rt_log_error(ERRNO_INVALID_ARGU,
				"%s", strerror(errno));
		return -1;
	}

	while(i ++ < MAX_CLUES_PER_TARGET * MAX_TARGETS) {
		tid = random();
		clue_id = random();
		score = random();
		fprintf(fp, "%u %lu %d %d\n", (clue_id % MAX_CLUES_PER_TARGET + 1), (tid % MAX_TARGETS + 1), i, score % 100);
	}

	fclose(fp);

	return 0;
}

void vrmt_random (target_id *tid, int *vid)
{
	*tid  = (random() % MAX_TARGETS + 1);
	*vid = (random() % 10 + 1);
}

