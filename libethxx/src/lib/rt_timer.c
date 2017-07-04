/*
*   timer.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The timer control system
*   Personal.Q
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#include "rt_common.h"
#include "rt_timer.h"
#include "rt_task.h"
#include "rt_logging.h"
#include "rt_sync.h"

#define SYS_MAX_TIMER_NUM   20
#define SYS_TICK_MULTIPLE   1
#define ESYS_MODULE_MAX     100

static int timer_errno __attribute__((__unused__)) = 0;

typedef enum
{
    STIDLE = 0,
    STSTART,
    STSTOP,
} timer_state;

typedef struct tSysTimerNode
{
    int timer_id;
    int module_id;
    enum tm_type timer_type;
    timer_state eState;
    int wTickOffset;
    unsigned short dwTimerLimit;
    void (*pvTimeFunc)(uint32_t uid, int argc, char **argv) ;
    int   argc;
    char **argv;
    struct tSysTimerNode *ptPre;
    struct tSysTimerNode *ptNext;

} TSYS_TIMER_NODE;

/* Timer management */
typedef struct
{
    int byInitd;
    int byIdleNum;
    int byUseNum;
    TSYS_TIMER_NODE *ptTimerIdle;
    TSYS_TIMER_NODE *ptTimerUse;

} TSYS_TIMER_MA;


static rt_pthread pThreadTmrMgrId __attribute__((__unused__)) = 0;

static struct itimerval TimerVal;

const long  dwDiv = 1000;
static long g_dwTimeTickCout;
/*Mutex lock.*/
static INIT_MUTEX(TmrMutexVal);
/*Tick lock.*/
static INIT_MUTEX(Ticklock);

unsigned short g_dwSysOldTicks;
unsigned short g_dwSysCurTicks;

static TSYS_TIMER_MA  g_tSysTimerTable = {
    .byInitd = uninitialized,
    .byIdleNum = 0,
    .byUseNum = 0,
    .ptTimerIdle = NULL,
    .ptTimerUse = NULL,
};

TSYS_TIMER_NODE  g_atSysTimerNode[SYS_MAX_TIMER_NUM];


static void rt_tm_sighandler(long timer_sig)
{
    timer_sig = timer_sig;
    TSYS_TIMER_NODE  *ptTimer = NULL;
    g_dwTimeTickCout++;
    g_dwTimeTickCout = 0 ;
    rt_mutex_lock(&Ticklock);
    g_dwSysCurTicks++;
    g_dwSysOldTicks = g_dwSysCurTicks;
    rt_mutex_unlock(&Ticklock);
    rt_mutex_lock(&TmrMutexVal);
    ptTimer = g_tSysTimerTable.ptTimerUse;

    while (ptTimer != NULL)
    {
        if (STSTART == ptTimer->eState)
        {
            if (g_dwSysOldTicks >= ptTimer->dwTimerLimit)
            {
                ptTimer->pvTimeFunc(0, ptTimer->argc, ptTimer->argv);

                if (TYPE_ONE == ptTimer->timer_type)
                {
                    ptTimer->eState = STSTOP;
                }
                else
                {
                    ptTimer->dwTimerLimit = g_dwSysOldTicks + ptTimer->wTickOffset;
                }
            }
        }

        ptTimer = ptTimer->ptNext;
    }

    rt_mutex_unlock(&TmrMutexVal);
}


int rt_timer_delete(int timer_id)
{
    TSYS_TIMER_NODE  *ptTimer = NULL;
    TSYS_TIMER_NODE  *ptTimerPre = NULL;
    TSYS_TIMER_NODE  *ptTimerNext = NULL;

    if ((timer_id >= SYS_MAX_TIMER_NUM) ||
        (STIDLE == g_atSysTimerNode[timer_id].eState) ||
        (0 == g_tSysTimerTable.byUseNum))
        return -1;
    
    ptTimer = (TSYS_TIMER_NODE *) &g_atSysTimerNode[timer_id];
    rt_mutex_lock(&TmrMutexVal);
    g_tSysTimerTable.byUseNum--;

    if (0 == g_tSysTimerTable.byUseNum)
    {
        g_tSysTimerTable.ptTimerUse = NULL;
    }
    else
    {
        ptTimerPre = ptTimer->ptPre;
        ptTimerNext = ptTimer->ptNext;

        if ((NULL != ptTimerPre) && (NULL != ptTimerNext))
        {
            ptTimerPre->ptNext = ptTimerNext;
            ptTimerNext->ptPre = ptTimerPre;
        }
        else if (NULL == ptTimerPre)
        {
            ptTimerNext->ptPre = NULL;
            g_tSysTimerTable.ptTimerUse = ptTimerNext;
        }
        else if (NULL == ptTimerNext)
        {
            ptTimerPre->ptNext = NULL;
        }
        else
        {
            return -1;
        }
    }

    memset(&g_atSysTimerNode[timer_id], 0, sizeof(TSYS_TIMER_NODE));
    g_atSysTimerNode[timer_id].timer_id = timer_id;

    if (NULL == g_tSysTimerTable.ptTimerIdle)
    {
        g_tSysTimerTable.ptTimerIdle = ptTimer;
    }
    else
    {
        ptTimer->ptNext = g_tSysTimerTable.ptTimerIdle;
        g_tSysTimerTable.ptTimerIdle = ptTimer;
    }

    g_tSysTimerTable.byIdleNum++;
    rt_mutex_unlock(&TmrMutexVal);
    return 0;
}

//Timer_delay: (ms)
int rt_timer_start(int timer_id, int timer_delay_ms, void *timer_func, int argc, char **argv)
{
    int  wTickOffset = 0;
    unsigned int g_dwSysCurTicksTmp = 0;

    if ((timer_id >= SYS_MAX_TIMER_NUM) ||
        (NULL == timer_func) ||
        (STIDLE == g_atSysTimerNode[timer_id].eState))
        return -1;

    wTickOffset = timer_delay_ms / 200;
    //wTickOffset = timer_delay_ms/SYS_TICK_MULTIPLE;
    rt_mutex_lock(&Ticklock);
    g_dwSysCurTicksTmp = g_dwSysCurTicks;
    rt_mutex_unlock(&Ticklock);
    rt_mutex_lock(&TmrMutexVal);
    g_atSysTimerNode[timer_id].eState = STSTART;
    g_atSysTimerNode[timer_id].wTickOffset = wTickOffset;
    g_atSysTimerNode[timer_id].dwTimerLimit = g_dwSysCurTicksTmp + wTickOffset;
    g_atSysTimerNode[timer_id].pvTimeFunc = timer_func;
    g_atSysTimerNode[timer_id].argc = argc;
    g_atSysTimerNode[timer_id].argv = argv;
    rt_mutex_unlock(&TmrMutexVal);
    return 0;
}

int rt_timer_stop(int timer_id)
{
    if ((timer_id >= SYS_MAX_TIMER_NUM) ||
        (STIDLE == g_atSysTimerNode[timer_id].eState))
        return -1;

    rt_mutex_lock(&TmrMutexVal);
    g_atSysTimerNode[timer_id].eState = STSTOP;
    rt_mutex_unlock(&TmrMutexVal);
    return 0;
}

void *rt_timer_proc(void *pv_para __attribute__((__unused__)))
{
    signal(SIGALRM, (void *) rt_tm_sighandler);
    TimerVal.it_interval.tv_sec = 0;/**/
    TimerVal.it_interval.tv_usec = dwDiv * 200;
    TimerVal.it_value.tv_sec = 0;
    TimerVal.it_value.tv_usec = dwDiv * 200;
    setitimer(ITIMER_REAL, &TimerVal, NULL);
    rt_log_debug("  T I M E R   S T A R T E D ...\r\n");

    for (;;)
    {
        //wait for kernel
        sleep(1000);
    }

    return NULL;
}

static struct rt_task_t rt_timer_proc_task =
{
    .module = THIS,
    .name = "SysTimer Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = rt_timer_proc,
};

int rt_timer_init(void)
{
    int byI = 0;
    g_dwSysOldTicks = 0;
    
    rt_mutex_lock(&TmrMutexVal);
    
    memset(g_atSysTimerNode, 0, sizeof(g_atSysTimerNode));
    for (byI = 0; byI < SYS_MAX_TIMER_NUM; byI++)
    {
        g_atSysTimerNode[byI].timer_id = byI;

        if ((SYS_MAX_TIMER_NUM - 1) == byI)
        {
            g_atSysTimerNode[byI].ptNext = NULL;
        }
        else
        {
            g_atSysTimerNode[byI].ptNext = (TSYS_TIMER_NODE *) &g_atSysTimerNode[byI + 1];
        }
    }

    g_tSysTimerTable.byIdleNum = SYS_MAX_TIMER_NUM;
    g_tSysTimerTable.byUseNum = 0;
    g_tSysTimerTable.ptTimerIdle = (TSYS_TIMER_NODE *) g_atSysTimerNode;
    g_tSysTimerTable.ptTimerUse = NULL;
    g_tSysTimerTable.byInitd = initialized; /** timer has been init */
    
    rt_mutex_unlock(&TmrMutexVal);

    task_registry(&rt_timer_proc_task);
                
    return 0;
}

int rt_timer_create(int module_id, enum tm_type timer_type)
{
    TSYS_TIMER_NODE  *ptTimer = NULL;

    if (!g_tSysTimerTable.byInitd){
        rt_timer_init();
    }
    
    if ((module_id >= ESYS_MODULE_MAX) ||
        (0 == g_tSysTimerTable.byIdleNum))
       return -1;

    rt_mutex_lock(&TmrMutexVal);
    ptTimer = g_tSysTimerTable.ptTimerIdle;
    g_tSysTimerTable.byIdleNum--;

    if (0 == g_tSysTimerTable.byIdleNum)
    {
        g_tSysTimerTable.ptTimerIdle = NULL;
    }
    else
    {
        g_tSysTimerTable.ptTimerIdle = ptTimer->ptNext;
    }

    g_tSysTimerTable.byUseNum++;

    if (NULL == g_tSysTimerTable.ptTimerUse)
    {
        ptTimer->ptPre = NULL;
        ptTimer->ptNext = NULL;
    }
    else
    {
        ptTimer->ptPre = NULL;
        ptTimer->ptNext = g_tSysTimerTable.ptTimerUse;
        g_tSysTimerTable.ptTimerUse->ptPre = ptTimer;
    }

    g_tSysTimerTable.ptTimerUse = ptTimer;
    rt_mutex_unlock(&TmrMutexVal);
    ptTimer->module_id = module_id;
    ptTimer->timer_type = timer_type;
    ptTimer->eState = STSTOP;
    ptTimer->wTickOffset = 0;
    ptTimer->dwTimerLimit = 0;
    ptTimer->pvTimeFunc = NULL;
    ptTimer->argc = 0;
    ptTimer->argv = NULL;
    return (ptTimer->timer_id);
}

