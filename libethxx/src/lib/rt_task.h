/*
*   task.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System task control interface of SPASR
*   Personal.Q
*/

#ifndef __RT_TASK_H__
#define __RT_TASK_H__

#include <stdint.h>
#include "rt_common.h"
#include "rt_list.h"
#include "rt_sync.h"

struct rt_task_t {
    /** Not used */
    int module;

#define PID_INVALID -1
	rt_pthread pid;

#define TASK_NAME_SIZE 64
	char name[TASK_NAME_SIZE + 1];

#define INVALID_CORE (-1)
	/** proc core, kernel schedule if eque INVALID_CORE */
	int core;

	/** attr of current task */
	rt_pthread_attr *attr;

#define KERNEL_SCHED    0
	/** priority of current task */
	int prio;

	/** argument count */
	int args;

	/** arguments */
	void *argvs;

	/** Executive entry */
	void * (*routine)(void *);

	/** allowed or forbidden */
	int recycle;

	struct list_head   list;
	struct hlist_head	hlist;

};

extern void task_spawn(char *desc, uint32_t prio, void *attr, void * (*func)(void *), void *arg);
extern void task_spawn_quickly(struct rt_task_t *task);

extern void task_registry(struct rt_task_t *task);
extern void task_deregistry(struct rt_task_t *task);
extern void task_deregistry_named(const char *desc);
extern void task_deregistry_id(rt_pthread pid);

extern struct rt_task_t  *task_query_id (rt_pthread pid);

extern void task_run();

extern void task_detail_foreach();

#endif
