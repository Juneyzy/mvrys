/*
*   mq.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Message Queue for inter-thread communication
*   Personal.Q
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "rt_common.h"
#include "rt_sync.h"
#include "rt_task.h"
#include "rt_mq.h"
#include "rt_atomic.h"
#include "rt_logging.h"
#include "rt_stdlib.h"
#include "rt_string.h"

#define OS_WRAP_BITS				12
#define OS_WRAP_MAX_TASKS  		(1 << OS_WRAP_BITS)
#define OS_WRAP_MAX_QUEUES		(OS_WRAP_MAX_TASKS * 2)
#define OS_WRAP_MAX_SEMS			(OS_WRAP_MAX_TASKS * 10)

struct rt_fifo_data_block {
	struct list_head list;
	/* address where the message stored */
	message   data;
	/** message size */
	size_t s;
};

struct rt_fifo_scb {

	uint64_t unique_id;
	
	rt_cond cond;
	rt_mutex mtx;

	int data_blk_factor;

	struct hlist_node hlist;
} ;

struct rt_fifo_block {

	/** Name of the queue */
	char *desc;

	rt_mutex fb_lock;
	
	/** */
	uint64_t unique_id;

	uint32_t	hlist_entry;	/** hlist_entry = hash (fb.unique_id) % OS_WRAP_MAX_TASKS */

	struct rt_fifo_scb *scb;
	
	int	data_blks;		/** Total data blocks */
	struct list_head head;	/** db.list */
};


struct rt_fifo_ctrl_block {
	
	int	fifo_cursor;	/** list_cursor always means the number of lived fifos. */
	
	struct rt_fifo_block   *fifos;

	/** mutex of the FIFO block */
	rt_mutex fifo_lock;
	
	struct hlist_head scb_hash_table [OS_WRAP_MAX_TASKS];

	int initd;
} ;

static struct rt_fifo_ctrl_block FIFOs = {
	.fifos = NULL,
	.fifo_cursor = (0),
	.initd = uninitialized,
	.fifo_lock = PTHREAD_MUTEX_INITIALIZER,
};


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

static __rt_always_inline__ int 
rt_mq_chk_id (int id)
{
	if (id < 0 ||id > OS_WRAP_MAX_QUEUES){
		rt_log_error(ERRNO_MQ_INVALID,
			"%d", id);
		return -1;
	}
	
	return 0;
}

static __rt_always_inline__ struct rt_fifo_scb *__fifo_scb_alloc (uint64_t cur_fifo_cursor)
{
	struct rt_fifo_scb *_this;

	_this = (struct rt_fifo_scb *)kmalloc(sizeof(struct rt_fifo_scb), MPF_CLR, -1);
	if (likely(_this)) {
		_this->unique_id = cur_fifo_cursor + 1;
		_this->data_blk_factor = 0;
		rt_mutex_init(&_this->mtx, NULL);
		rt_cond_init(&_this->cond, NULL);
		INIT_HLIST_NODE (&_this->hlist);
	}

	return _this;
}

static __rt_always_inline__ struct rt_fifo_scb *
__fifo_alloc (const char *desc, uint64_t fifo_cursor,
				struct rt_fifo_block *fb)
{
	uint64_t cur_fifo_cursor = 0;
	struct rt_fifo_scb *scb = NULL;
	
	cur_fifo_cursor = fifo_cursor;
	if ((int64_t)cur_fifo_cursor >= OS_WRAP_MAX_SEMS) {
		rt_log_error(ERRNO_MQ_NO_RESOURCE, 
			"%lu (Max=%d)", cur_fifo_cursor, OS_WRAP_MAX_SEMS);
		goto finish;
	 }

	scb = __fifo_scb_alloc (cur_fifo_cursor);
	if (likely(scb)) {
		INIT_LIST_HEAD (&fb->head);
		fb->desc = strdup (desc);
		fb->hlist_entry = hash_64 (scb->unique_id, OS_WRAP_BITS);
		fb->unique_id = scb->unique_id;
		fb->scb = scb;
		fb->data_blks = 0;
		rt_mutex_init (&fb->fb_lock, NULL);
	}
	
finish:
	return scb;
}


static __rt_always_inline__ void
rt_fifo_wakeup (struct rt_fifo_scb *scb)
{
	rt_mutex_lock(&scb->mtx);
	
	scb->data_blk_factor ++;
	if (scb->data_blk_factor >= 1)
		rt_cond_signal(&scb->cond);

	rt_mutex_unlock(&scb->mtx);
}

static __rt_always_inline__ void
rt_fifo_stupor (struct rt_fifo_scb *scb)
{
	rt_mutex_lock(&scb->mtx);
	
	if (scb->data_blk_factor == 0)
		rt_cond_wait(&scb->cond, &scb->mtx);

	if (scb->data_blk_factor >= 1)
		scb->data_blk_factor --;

	rt_mutex_unlock(&scb->mtx);
}

static __rt_always_inline__ struct rt_fifo_data_block *
db_new (message msg, int size)
{
	struct rt_fifo_data_block *db;
	
	db = (struct rt_fifo_data_block *) kmalloc (sizeof (struct rt_fifo_data_block), MPF_CLR, -1);
	if (likely (db)) {
		db->data = msg;
		db->s = size;
		INIT_LIST_HEAD (&db->list);
	}

	return db;
}

static __rt_always_inline__ void
db_release (struct rt_fifo_data_block *db)
{
	kfree (db);
}

mq_errno rt_fifo_push (MQ_ID qid, message msg, int s)
{
	struct rt_fifo_block *fb= NULL;
	struct rt_fifo_ctrl_block *fcb = &FIFOs;
	struct rt_fifo_data_block *db;

	mq_errno xerror = MQ_ID_INVALID;
	int	idx;
	
	if (unlikely (!msg))
		goto finish;

	idx = qid -1;
	
	/* If current queue is reaching to maxinum */
	if (rt_mq_chk_id(idx) < 0)
	    goto finish;

	fb = &fcb->fifos[idx];

	db = db_new (msg, s);
	if (likely (db)) {
		rt_mutex_lock (&fb->fb_lock);
		list_add_tail (&db->list, &fb->head);
		fb->data_blks ++;
		rt_mutex_unlock (&fb->fb_lock);
	}

	rt_fifo_wakeup (fb->scb);
	xerror = MQ_SUCCESS;
	
finish:
	return xerror;
}

mq_errno rt_fifo_pop (MQ_ID qid, message *msg, int *s)
{
	struct rt_fifo_block *fb= NULL;
	struct rt_fifo_ctrl_block *fcb = &FIFOs;
	struct rt_fifo_data_block *pos, *n;

	mq_errno xerror = MQ_ID_INVALID;
	int	idx;
	
	if (unlikely (!msg))
		goto finish;

	idx = qid -1;
	
	/* If current queue is reaching to maxinum */
	if (rt_mq_chk_id(idx) < 0)
	    goto finish;

	fb = &fcb->fifos[idx];

	rt_fifo_stupor (fb->scb);

	rt_mutex_lock (&fb->fb_lock);
	list_for_each_entry_safe (pos, n, &fb->head, list) {
		list_del (&pos->list);
		break;
	}
	rt_mutex_unlock (&fb->fb_lock);

	if (!pos)
		xerror = MQ_FAILURE;
	else {
		xerror = MQ_SUCCESS;
		*msg = pos->data;
		*s = pos->s;
		db_release (pos);
	}
	
finish:
	return xerror;
}

MQ_ID rt_fifo_create (const char *desc)
{
	struct rt_fifo_block *fb= NULL;
	struct rt_fifo_ctrl_block *fcb = &FIFOs;
	int i = 0;
	MQ_ID id = MQ_ID_INVALID;
	
	if (unlikely(!desc)) {
		return id;
	}

	if (unlikely (!fcb->initd)) {
		fcb->fifos = (struct rt_fifo_block *)kmalloc(sizeof (struct rt_fifo_block) * OS_WRAP_MAX_QUEUES, MPF_CLR, -1);
		for (i = 0; i < OS_WRAP_MAX_TASKS; i ++) {
			INIT_HLIST_HEAD (&fcb->scb_hash_table[i]);
		}
		
		rt_mutex_init(&fcb->fifo_lock, NULL);
		fcb->initd = initialized;
		rt_log_notice ("M E S S A G E     Q U E U E    I N I T I A L I Z E D ...");
	}

	rt_mutex_lock(&fcb->fifo_lock);
	
	/* If current queue is reaching to maxinum */
	if (rt_mq_chk_id (fcb->fifo_cursor) < 0)
	    goto finish;

	fb = &(fcb->fifos[fcb->fifo_cursor]);

	__fifo_alloc (desc, fcb->fifo_cursor, fb);
	if (unlikely (!fb->scb)){
	    goto finish;
	}

	id = fb->unique_id;
	fcb->fifo_cursor ++;
	
finish:
	rt_mutex_unlock(&fcb->fifo_lock);
	return id;
}

int rt_fifo_destroy (MQ_ID qid)
{
	struct rt_fifo_block *fb= NULL;
	struct rt_fifo_ctrl_block *fcb = &FIFOs;

	int xerror = MQ_ID_INVALID;
	int	idx;
	
	idx = qid -1;
	
	/* If current queue is reaching to maxinum */
	if (rt_mq_chk_id(idx) < 0)
	    goto finish;

	fb = &fcb->fifos[idx];

	fb = fb;
	
finish:

	return xerror;
}

#if 0
struct fifo_test {
	MQ_ID id;
	atomic64_t	in_inc, out_inc;
};

struct fifo_test ft = {
	.in_inc = ATOMIC_INIT (0),
	.out_inc = ATOMIC_INIT (0),
};

struct fifo_test ftx = {
	.in_inc = ATOMIC_INIT (0),
	.out_inc = ATOMIC_INIT (0),
};

static void *fifo_summary (void *param)
{
	param = param;
	
	FOREVER {
		sleep (3);
		rt_log_info ("MQ=%ld, (%ld-%ld), Leakage=%ld", ft.id, atomic64_add (&ft.in_inc, 0), atomic64_add (&ft.out_inc, 0),
			atomic64_add (&ft.in_inc, 0) - atomic64_add (&ft.out_inc, 0));
		rt_log_info ("MQ=%ld, (%ld-%ld), Leakage=%ld", ftx.id, atomic64_add (&ftx.in_inc, 0), atomic64_add (&ftx.out_inc, 0),
			atomic64_add (&ftx.in_inc, 0) - atomic64_add (&ftx.out_inc, 0));

	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static void *fifo_out (void *param)
{
	struct fifo_test  *ftx = (struct fifo_test *)param;
	message	data = NULL;
	int	s = 0;
	
	FOREVER {
		data = NULL;
		/** Recv from internal queue */
		rt_fifo_pop (ftx->id, &data, &s);
		if (likely (data)) {
			kfree (data);
			atomic64_inc (&ftx->out_inc);
		}
	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static __rt_always_inline__ void message_clone (char *ijstr, size_t l, void **clone)
{	
	*clone = kmalloc (l + 1, MPF_CLR, -1);
	if (*clone) {
		memcpy64 (*clone, ijstr, l);
	}
}

static void *fifo_in (void *param)
{
	struct fifo_test  *ftx = (struct fifo_test *)param;
	message	data = NULL;
	int	s = 0;
	
	FOREVER {
		usleep (10);
		data = NULL;
		message_clone ((char *)__func__, strlen ((char *)__func__), &data);
		s = strlen (__func__);
		if (MQ_SUCCESS != rt_fifo_push (ftx->id, data, (int)s)) {
			kfree (data);
		}
		else
			atomic64_inc (&ftx->in_inc);
	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static void *fifo_inx (void *param)
{
	struct fifo_test  *ftx = (struct fifo_test *)param;
	message	data = NULL;
	int	s = 0;
	
	FOREVER {
		usleep (10);
		data = NULL;
		message_clone ((char *)__func__, strlen (__func__), &data);
		s = strlen (__func__);
		if (MQ_SUCCESS != rt_fifo_push (ftx->id, data, (int)s)) {
			kfree (data);
		}
		else
			atomic64_inc (&ftx->in_inc);
	}

	task_deregistry_id (pthread_self());
	
	return NULL;
}

static struct rt_task_t inTask = {
    .module = THIS,
    .name = "In Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ft,
    .routine = fifo_in ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t outTask = {
    .module = THIS,
    .name = "Out Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ft,
    .routine = fifo_out ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t inxTask = {
    .module = THIS,
    .name = "Inx Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ftx,
    .routine = fifo_inx ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t outxTask = {
    .module = THIS,
    .name = "Outx Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ftx,
    .routine = fifo_out ,
    .recycle = FORBIDDEN,
};

static struct rt_task_t summaryTask = {
    .module = THIS,
    .name = "Summary Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = fifo_summary ,
    .recycle = FORBIDDEN,
};

void rt_fifo_test1 ()
{
	ft.id = rt_fifo_create ("ssssssss");

	task_registry (&outTask);

	task_registry (&inTask);

	inxTask.argvs = (void *)&ft;
	task_registry (&inxTask);

	task_registry (&summaryTask);
}

void rt_fifo_test2 ()
{
	ft.id = rt_fifo_create ("ssssssss");
	ftx.id = rt_fifo_create ("ssssssss");

	
	task_registry (&inTask);
	task_registry (&outTask);
	
	task_registry (&inxTask);
	task_registry (&outxTask);

	task_registry (&summaryTask);
}

void rt_fifo_test3 ()
{
	int i = 0;
	MQ_ID id = MQ_ID_INVALID;
	
	for (i = 0; i < 100000;  i ++ ) {
		id = rt_fifo_create ("sssss");
		if ((long)id < 0)
			break;
	}

	rt_log_info ("Max FIFOs = %d", i);

}

void rt_fifo_test ()
{
	rt_fifo_test1 ();
	rt_fifo_test2 ();
	rt_fifo_test3 ();

	
	task_run ();

	FOREVER {
		sleep (100);
	}
}
#endif

