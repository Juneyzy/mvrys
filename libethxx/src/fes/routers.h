/*
*   routers.h
*   Created by Tsihang <qihang@semptian.com>
*   2 Mar, 2016
*   Func: Lockless Chain
*   Personal.Q
*/

#ifndef __ROUTERS_H__
#define __ROUTERS_H__

#include "rt_list.h"
#include "rt_sync.h"

#define R_CTRL_ADD  (1 << 0)
#define R_CTRL_DEL  (1 << 1)
#define R_CTRL_MOD  (1 << 2)
#define R_CTRL_CHK  (1 << 3)
#define R_CTRL_UD   (1 << 4)    /** user defined */

#define R_CTRL_PORT_ADD (1 << 10)
#define R_CTRL_PORT_DEL (1 << 11)
#define R_CTRL_PORT_MOD (1 << 12)
#define R_CTRL_PORT_CHK (1 << 13)

struct  leaked_stats_t {
    atomic_t    ai_l2_in_arp,   ai_l2_out_arp;
    atomic_t    ai_l2_in_unknown,   ai_l2_out_unknown;

    atomic_t    ai_l3_in_icmp,  ai_l3_out_icmp;
    atomic_t    ai_l3_in_igmp,  ai_l3_out_igmp;
    atomic_t    ai_l3_in_unknown,   ai_l3_out_unknown;

    atomic_t    ai_l4x_in_telnet,   ai_l4x_out_telnet;
    atomic_t    ai_l4x_in_ftp,  ai_l4x_out_ftp;
    atomic_t    ai_l4x_in_tftp, ai_l4x_out_tftp;
    atomic_t    ai_l4x_in_ssh,  ai_l4x_out_ssh;
    atomic_t    ai_l4x_in_http, ai_l4x_out_http;
    atomic_t    ai_l4x_in_unknown,  ai_l4x_out_unknown;
    
};

struct port_t{

#define PID_INVALID -1
#define PID_MAX         INT_MAX

    int     rid;        /** which router this port belong to */

    int     pid;        /** port id */

    int     ip;
    char    *ipstr, *macstr;

    int     mac;

    int     flags;

    int     recycle;

    atomic_t    __inpkts, __outpkts;

    struct  leaked_stats_t   __leaked;
    
    struct list_head    list;
};

struct router_t {

#define RID_INVALID -1
    int     rid;
    
    int     ports;      /** The number of ports for this router */

    int     flags;

    int     recycle;

    rt_mutex    plock;              /** port list lock */
    
    struct list_head    plist;      /** list of ports */

    struct list_head    rlist;       /** list of routers */
};

typedef void (*routine_t)(struct router_t *r, struct port_t *p, int flags, void *args) ;

extern void router_create(int rid);
extern void router_destroy(int rid);
extern void router_for_each(int pid, 
                        routine_t routine,
                        void *argument);
extern void router_for_each_port(int rid, 
                        routine_t routine, 
                        void *argument);

extern void router_ctrl(int rid,
                        int pid,
                        int opctrl,
                        routine_t routine, 
                        void *argument);


#endif
