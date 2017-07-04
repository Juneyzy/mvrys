/*
*   rt_tmr.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System Advanced Timer Control Interface of SPASR
*   Personal.Q
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>

#include "rt_common.h"
#include "rt_sync.h"
#include "rt_logging.h"
#include "rt_atomic.h"
#include "rt_stdlib.h"
#include "rt_string.h"
#include "rt_tmr.h"
#include "rt_task.h"
#include "rt_hash.h"

#ifdef RT_TMR_ADVANCED
static int init;
static INIT_MUTEX(tmrlist_lock);
static LIST_HEAD(tmrlist);

static volatile atomic64_t  g_sys_cur_ticks = ATOMIC_INIT(0);

#define tmr_precision  50000
/** The macro is not Recommended when creating a timer with api tmr_create */
#define tmr_internal_trans(sec) (((sec * 1000 * 1000)/(tmr_precision)))

static void 
tmr_default_routine(uint32_t uid, int __attribute__((__unused__))argc, 
                char __attribute__((__unused__))**argv)
{
    static int count[2] = {0};

    count[uid%2] ++;
    printf ("default timer [%u, %d] routine has occured\n", uid, count[uid%2]);
}

static __rt_always_inline__ void *
tmr_alloc()
{
    struct timer_t *t;

    t = kmalloc(sizeof(struct timer_t), MPF_CLR, -1);
    if(likely(t)){
        t->uid = TMR_INVALID;
        t->routine = tmr_default_routine;
        t->interval = 3; //tmr_internal_trans(3);
        t->recycle = ALLOWED;
        t->type = TMR_ASHOT;
        t->status = TMR_STOPPED;
    }
    return t;
}

static __rt_always_inline__ void
tmr_free(struct timer_t *t)
{
    if(t->recycle == ALLOWED){
        kfree(t->desc);
        kfree(t);
    }
}

static __rt_always_inline__ uint32_t
tmr_uid_alloc(int __attribute__((__unused__))module, 
                const char *desc, 
                size_t s)
{
    HASH_INDEX hval = -1;

    if (unlikely(!desc) || 
        likely(s < 1))
        goto finish;
    
    hval = hash_data((void *)desc, s);
    
finish:
    return hval;
}


struct timer_t  *
tmr_set(uint32_t uid,
                void (*proc)(struct timer_t *t))
{
    struct timer_t *_this;

    rt_mutex_lock(&tmrlist_lock);
    list_for_each_entry(_this, &tmrlist, list){
        if (likely(uid == _this->uid)){
            if(likely(proc))
                proc(_this);
            break;
        }
    }
    rt_mutex_unlock(&tmrlist_lock);
    
    return _this;
}

static void tmr_enable(struct timer_t *t)
{
    t->status = TMR_STARTED;
}

static void tmr_disable(struct timer_t *t)
{
    t->status = TMR_STOPPED;
}

static void tmr_delete(struct timer_t *t)
{
    if(likely(t)){
        list_del(&t->list);
        tmr_free(t);
    }
}

void tmr_start(uint32_t uid)
{
    tmr_set(uid, tmr_enable);
}

void tmr_stop(uint32_t uid)
{
    tmr_set(uid, tmr_disable);
}

static void *
tmr_daemon(void __attribute__((__unused__))*pv_par )
{
    rt_log_debug("  T M R   S T A R T E D ...");    

    FOREVER{
        sleep(86400);
    }
    
    task_deregistry_id(pthread_self());
    return NULL;
}

void tmr_registry(struct timer_t *this)
{
    struct timer_t *_this = NULL;

    if(unlikely(!this))
        goto finish;
    
    rt_mutex_lock(&tmrlist_lock);
    
    list_for_each_entry(_this, &tmrlist, list){
        if ((likely(this->module == _this->module)) &&
                        likely(!STRCMP(this->desc, _this->desc))){
            rt_log_error(ERRNO_TM_MULTIPLE_REG, 
                "The same timer (%d: %s, %d)", 
                _this->module, _this->desc, _this->uid);
            goto err;
        }
    }

    if(likely((int32_t)this->uid == TMR_INVALID)){
        this->uid = tmr_uid_alloc(this->module, this->desc, strlen(this->desc));
        list_add(&this->list, &tmrlist);
    }
    
err:
    rt_mutex_unlock(&tmrlist_lock);
    
finish:
    return;
}

void tmr_deregistry(struct timer_t *this)
{
    struct timer_t *_this = NULL;

    rt_mutex_lock(&tmrlist_lock);

    if(likely((int32_t)this->uid != TMR_INVALID)){
        list_for_each_entry(_this, &tmrlist, list){
            if ((likely(this->module == _this->module)) &&
                            likely(!STRCMP(this->desc, _this->desc))){
                    tmr_delete(this);
                    break;
            }
        }
    }
    rt_mutex_unlock(&tmrlist_lock);
}

static struct rt_task_t tmr_daemon_task =
{
    .module = THIS,
    .name = "Advanced Timer Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = tmr_daemon,
};

static void tmr_handler()
{
    int64_t old_ticks = 0;
    struct timer_t *_this = NULL;

    old_ticks = atomic64_inc(&g_sys_cur_ticks);
    rt_mutex_lock(&tmrlist_lock);
        list_for_each_entry(_this, &tmrlist, list){
            if (likely(_this->status == TMR_STARTED)){
                if (likely(old_ticks >= _this->curr_ticks)) {
                    _this->routine(_this->uid, _this->argc, _this->argv);
                    if (likely(TMR_ASHOT == _this->type))
                        _this->status= TMR_STOPPED;
                    else
                        _this->curr_ticks = old_ticks + _this->interval;
                }
            }
        }
        rt_mutex_unlock(&tmrlist_lock);
        
}

static void realtimer_init()
{
    static struct itimerval tmr;
    
    signal(SIGALRM, (void *) tmr_handler);
    tmr.it_interval.tv_sec = 1;
    tmr.it_interval.tv_usec = 0;
    tmr.it_value.tv_sec = 1;
    tmr.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &tmr, NULL);
    
}

/**
* routine: must be a reentrant function.
* An unknown error ocurrs if a thread-safe function called as a routione.
*/
uint32_t tmr_create(int module,
                const char *desc, int type,
                void (*routine)(uint32_t, int, char **), int argc, char **argv, int sec)
{
    struct timer_t *_this;
    uint32_t uid = TMR_INVALID;
    
    if(unlikely(!desc)){
        rt_log_error(ERRNO_INVALID_ARGU, 
            "Timer description is null, task will not be created");
        goto finish;
    }

    rt_mutex_lock(&tmrlist_lock);
    list_for_each_entry(_this, &tmrlist, list){
        if(likely(!STRCMP(desc, _this->desc))){
            rt_log_warning(ERRNO_TM_EXSIT, 
                "The same timer (%s, %d)", _this->desc, _this->uid);
            goto err;
        }

    }
    
    _this = (struct timer_t *)tmr_alloc();
    if(unlikely(!_this)){
        rt_log_error(ERRNO_MEM_ALLOC, 
            "Alloc a timer");
        goto err;
    }
    
    _this->module = module;
    _this->desc = strdup(desc);
    _this->curr_ticks = 0;
    _this->interval = sec;
    _this->routine = routine ? routine : tmr_default_routine;
    _this->type = type;
    _this->uid = uid = tmr_uid_alloc(_this->module, _this->desc, strlen(_this->desc));
    _this->recycle = ALLOWED;
    _this->status = TMR_STOPPED;
    _this->argc = argc;
    _this->argv = argv;

    list_add_tail(&_this->list, &tmrlist);

    if(likely(init == 0)){
        task_spawn_quickly(&tmr_daemon_task);
        init = 1;
        //sleep(1);
        realtimer_init();
    }
    
err:
    rt_mutex_unlock(&tmrlist_lock);
finish:    
    return uid;
}

static void 
tmr_naughty_boy(uint32_t uid, int __attribute__((__unused__))argc, 
                char __attribute__((__unused__))**argv)
{
    uid = uid;
    struct timer_t *_this;

    printf("%s\n", __func__);
    list_for_each_entry(_this, &tmrlist, list){
        if(_this->uid != uid){
            /** Nauty boy will disable other timer. */
            //tmr_disable(_this);
        }
    }
}

void
tmr_test0()
{
    uint32_t uid = tmr_create(1, "hhhh", TMR_PERIODIC,
                                                        tmr_naughty_boy, 0, NULL, 10);
    printf("%u\n", uid);
    
    tmr_start(uid);
    
}
#endif

