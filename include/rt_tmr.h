/*
*   rt_tmr.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System Advanced Timer Control Interface of SPASR
*   Personal.Q
*/


#ifndef __RT_TMR_ADVANCED_H__
#define __RT_TMR_ADVANCED_H__

#include "rt_list.h"

#ifdef RT_TMR_ADVANCED

struct timer_t{

#define TMR_INVALID  -1

    int module;

    /** unique id */
    uint32_t uid;

    /** Make sure that allocate desc member with malloc like function. */
    char *desc;

    /** in ms */
    int64_t interval;

    int64_t curr_ticks;
    
    enum {TMR_PERIODIC, TMR_ASHOT} type;
    
    enum {TMR_STOPPED, TMR_STARTED} status;
        
    void (*routine)(uint32_t uid, int argc, char **argv);
    
    int argc;
    
    char **argv;

    int recycle;
    
    struct list_head list;
};

extern void tmr_start(uint32_t uid);
extern void tmr_stop(uint32_t uid);
extern void tmr_registry(struct timer_t *this);
extern void tmr_deregistry(struct timer_t *this);

/**
* routine: must be a reentrant function.
* An unknown error ocurrs if a thread-safe function called as a routione.
*/
extern uint32_t tmr_create(int module,
                const char *desc, int type,
                void (*routine)(uint32_t, int, char **), int argc, char **argv, int delay_ms);

#endif
#endif