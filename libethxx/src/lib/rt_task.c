/*
*   task.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System task control interface of SPASR
*   Personal.Q
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "rt_task.h"
#include "rt_sync.h"
#include "rt_logging.h"
#include "rt_common.h"
#include "rt_stdlib.h"
#include "rt_string.h"

#define	TASK_FLG_INITD		(1 << 0)
struct rt_tasklist_t{
	rt_mutex  lock;
	int count;
	struct list_head	head;
	/** Search with task desc */
	struct hlist_head	hhead;

	int flags;
};

static struct rt_tasklist_t task_list = {
	.flags = 0,
	.count = 0,
};

static __rt_always_inline__ void chk_init(struct rt_tasklist_t *tasklist)
{
	if (!tasklist || (tasklist && !tasklist->flags)) {
		INIT_LIST_HEAD(&tasklist->head);
		INIT_HLIST_HEAD(&tasklist->hhead);
		rt_mutex_init(&tasklist->lock, NULL);
		tasklist->flags |= TASK_FLG_INITD;
		rt_sync_init();
	}
}

void task_registry(struct rt_task_t *task)
{
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	if(likely(task)){
		rt_mutex_lock(&tasklist->lock);
		list_add_tail(&task->list, &tasklist->head);
		tasklist->count ++;
		rt_mutex_unlock(&tasklist->lock);
	}
	
	return;
}

void task_deregistry(struct rt_task_t *task)
{
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	if(likely(task)){
		rt_mutex_lock(&tasklist->lock);
		list_del(&task->list);
		tasklist->count --;
		rt_mutex_unlock(&tasklist->lock);

		if(task->recycle == ALLOWED)
			kfree(task);
	}
	
	return;
}


void task_deregistry_named(const char *desc)
{
	struct rt_task_t *task = NULL, *p;
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	if(likely(desc)){
		list_for_each_entry_safe(task, p, &tasklist->head, list){
			if (!STRCMP(desc, task->name)){
				rt_log_debug("deregistry a task (\"%s\":%ld)", 
							task->name, task->pid);
				task_deregistry(task);
				return;
			}
		}
	}
}

void task_deregistry_id(rt_pthread pid)
{
	struct rt_task_t *task = NULL, *p;
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	list_for_each_entry_safe(task, p, &tasklist->head, list){
		if (pid == task->pid){
			rt_log_debug("deregistry a task (\"%s\":%ld)", 
						task->name, task->pid);
			task_deregistry(task);
			return;
		}
	}
}

struct rt_task_t  *task_query_id (rt_pthread pid)
{
	struct rt_task_t *task = NULL, *p;
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	list_for_each_entry_safe(task, p, &tasklist->head, list){
		if (pid == task->pid){
			rt_log_debug("query a task (\"%s\":%ld)", 
						task->name, task->pid);
			return task;
		}
	}

	return NULL;
}


/** Thread description should not be same with registered one. */
void task_spawn(char __attribute__((__unused__))*desc, 
		uint32_t __attribute__((__unused__))prio, void __attribute__((__unused__))*attr,  void * (*func)(void *), void *arg)
{
	struct rt_task_t *task = NULL, *p;
	struct rt_tasklist_t *tasklist = &task_list;

	if(unlikely(!desc)){
		rt_log_error(ERRNO_INVALID_ARGU, 
		    "Task description is null, task will not be created");
		return;
	}

	chk_init(tasklist);
	rt_mutex_lock(&tasklist->lock);
	list_for_each_entry_safe(task, p, &tasklist->head, list){
		if(!STRCMP(desc, task->name)){
			rt_log_warning(ERRNO_TASK_EXSIT, 
				"The same task (%s, %ld)", task->name, task->pid);
			goto finish;
		}
	}
	rt_mutex_unlock(&tasklist->lock);

	task = (struct rt_task_t *)kmalloc(sizeof(struct rt_task_t), MPF_CLR, -1);
	if(unlikely(!task)){
		rt_log_warning(ERRNO_TASK_CREATE,
			"No memory spawning a task %s", desc);
		goto finish;
	}
	
	task->prio = prio;
	task->attr = attr;
	task->routine = func;
	task->argvs = arg;
	INIT_LIST_HEAD(&task->list);
	INIT_HLIST_HEAD(&task->hlist);
	memcpy(task->name, desc, strlen(desc));

	rt_unsync();
	if (!pthread_create(&task->pid, task->attr, task->routine, task->argvs) &&
	    		 (!pthread_detach(task->pid))) {
		rt_sync();
		goto registry;
	}
	rt_sync();

	rt_log_error(ERRNO_TASK_CREATE, 
		"pthread_create or pthread_detach error");	
	goto finish;

registry:
	task_registry(task);

finish:
	return;
}

void task_spawn_quickly(struct rt_task_t *task)
{
	task_spawn(task->name, task->prio, task->attr, task->routine, task->argvs);
}


static void 
task_detail(struct rt_task_t *task, 
                        int __attribute__((__unused__))flags)
{
	if(likely(task)){
		printf("\t\"%64s\"%20ld\n", task->name, task->pid);
	}
}

void task_foreach_lineup(struct rt_tasklist_t *tasklist,
				int __attribute__((__unused__))flags, void (*routine)(struct rt_task_t *, int))
{
	struct rt_task_t *task = NULL, *p;

	if(likely(tasklist)){
		list_for_each_entry_safe(task, p, &tasklist->head, list){
		        if(likely(routine))
					routine(task, flags);
		}
	}
}

void task_detail_foreach()
{
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);
	
	printf("\r\nTask(s) %d Preview\n", 
			tasklist->count);

	printf ("\t%64s%20s\n", "DESCRIPTION", "IDENTI");
		task_foreach_lineup(tasklist, 0, task_detail);
	printf("\r\n\r\n");
}

void task_run()
{
	struct rt_task_t *task = NULL, *p;
	struct rt_tasklist_t *tasklist = &task_list;

	chk_init(tasklist);

	list_for_each_entry_safe(task, p, &tasklist->head, list){
		/** check if the task has been started */
		if (task->pid > 0)
			continue;
		
		rt_unsync();
		if (pthread_create(&task->pid, task->attr, task->routine, task->argvs)){
			rt_log_error(ERRNO_THRD_CREATE, 
				"%s", strerror(errno));
			goto finish;
		}

		if (pthread_detach(task->pid)){
			rt_log_error(ERRNO_THRD_DETACH, 
				"%s", strerror(errno));
			goto finish;
		}
		rt_sync();
	}
	sleep(1);
finish:
	rt_sync();
	task_detail_foreach();
	return;
}


