/*
*   timer.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The timer control system
*   Personal.Q
*/

#ifndef __SYS_TIMER_H__
#define __SYS_TIMER_H__

enum tm_type{
    TYPE_ONE,
    TYPE_LOOP
} ;

enum timer{
    ManagementAgent,
    FrontEndSystem,
    BackEndSystem,
    LocalProbeCluster,
    LocalDecoderCluster,
    PcapCapture,
    SG,
    RT_MAX_TM_GROUP,
};

int rt_timer_create(int , enum tm_type);
int rt_timer_delete(int );
int rt_timer_start(int , int, void *, int , char **);
int rt_timer_stop(int );

#endif

