/*
*   serv.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The service registry interface for lwrpc
*   Personal.Q
*/

#ifndef __LWRPC_SVC_H__
#define __LWRPC_SVC_H__

#include "rt_rbtree.h"

typedef struct
{
    unsigned short int serv_id;
    int (*srvFunc)(unsigned char *, unsigned char *, int *, int *); /* Entry of rpc response */
    void *data;
    struct rb_node rb;          /* Where this node located in the rb root */

} serv_element;

extern void serv_init(void);
extern int serv_register(
    unsigned short int ,
    int (*srvFunc)(unsigned char *, unsigned char *, int *, int *));
extern int serv_unregister(unsigned short int);
extern serv_element *serv_find(unsigned short int serv_id);
extern int serv_run(unsigned short int , unsigned char *, unsigned char *, int *, int *);
extern int serv_count(void);


#endif


