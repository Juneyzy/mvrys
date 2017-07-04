/*
*   routers.c
*   Created by Tsihang <qihang@semptian.com>
*   2 Mar, 2016
*   Func: Lockless Chain
*   Personal.Q
*/

#include "sysdefs.h"
#include "routers.h"

static LIST_HEAD(routerlist);
static INIT_MUTEX(routerlist_lock);

static inline struct port_t *port_new(int rid, int pid)
{
    struct port_t *p;

    p = kmalloc(sizeof(struct port_t), MPF_CLR, -1);
    if(likely(p)){
        p->recycle = ALLOWED;
        p->pid = pid;
        p->rid = rid;
    }

    return p;
}

static inline void port_release(struct port_t *p)
{
    if(likely(p) &&
            likely(p->recycle == ALLOWED)){
        kfree(p);
    }
}

static inline void rp_del(struct router_t *r, 
                        struct port_t *p,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{
    if(likely(p)){
        list_del(&p->list);
        port_release(p);
        r->ports --;
    }
}

static inline void rp_add(struct router_t *r, 
                        struct port_t *p,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{
    if(likely(p)){
        list_add_tail(&p->list, &r->plist);
        r->ports ++;
    }
}

static inline void rp_print(struct router_t *r,
                        struct port_t *p,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*args)
{
    if(likely(r)){
        if(likely(p)){
            printf("(%d:%d)\n", r->rid, p->pid);
        }
    }
}

struct port_t *rport_lookup(struct router_t *r, int pid)
{
    struct port_t *port, *p;
    
    rt_mutex_lock(&r->plock);
    list_for_each_entry_safe(port, p, &r->plist, list){
        if(likely(port->pid == pid)){
            rt_mutex_unlock(&r->plock);
            return port;
        } 
    }
        
    rt_mutex_unlock(&r->plock);
    return NULL;
}

/*
*   a port_t will be returned back if exist.
*/
static inline struct port_t  *rport_add(struct router_t *router,
                        int p,
                        void *arg,
                        struct port_t *(*lookup)(struct router_t *r, int pid))
{  
    struct port_t *_this = NULL;

    if(likely(router)){
        
        if(likely(p != PID_INVALID) &&
            likely(p != PID_MAX)){
            if(lookup) _this = lookup(router, p);
            if(unlikely(!_this)){
                    _this = port_new(router->rid, p);
                    if(likely(_this)){
                        rt_mutex_lock(&router->plock);
                        rp_add(router, _this, R_CTRL_PORT_ADD, arg);
                        rt_mutex_unlock(&router->plock);
                    }
            }
        }
    }
    return _this;
}

/** p: a specific port, all port will be deleted when p = PID_MAX */
static inline void rport_del(struct router_t *router,
                        int p,
                        void *arg)
{  
    if(likely(router)){
        struct port_t *_this, *port;

        if(likely(p != PID_INVALID) &&
            likely(p != PID_MAX)){
            _this = rport_lookup(router, p);
            if(likely(_this)){
                rp_del(router, _this, R_CTRL_PORT_DEL, arg);
            }
        }
        else{
                if(likely(p == PID_MAX)){
                    rt_mutex_lock(&router->plock);
                    list_for_each_entry_safe(_this, port, &router->plist, list){
                        rp_del(router, _this, R_CTRL_PORT_DEL, arg);
                    }
                    rt_mutex_unlock(&router->plock);
                }
        }
    }
}

static inline void rport_print(struct router_t *router,
                        int p,
                        void *arg)
{  
                
    if(likely(router)){
        struct port_t *_this, *port;

        if(likely(p != PID_INVALID) &&
            likely(p != PID_MAX)){
            _this = rport_lookup(router, p);
            if(likely(_this)){
                rp_print(router, _this, R_CTRL_PORT_CHK, arg);
            }
        }
        else{
            if(likely(p == PID_MAX)){
                rt_mutex_lock(&router->plock);
                list_for_each_entry_safe(_this, port, &router->plist, list){
                    rp_print(router, _this, R_CTRL_PORT_CHK, arg);
                }
                rt_mutex_unlock(&router->plock);
            }
        }
    }
}


struct port_t *rport_lookup1(struct router_t *router, 
                        int pid,
                        void *argument,
                        void (* doxx)(struct router_t *r, struct port_t *p, int flags, void *arg))
{
    struct port_t *port, *p;
    
    rt_mutex_lock(&router->plock);
    list_for_each_entry_safe(port, p, &router->plist, list){
        if(likely(port->pid == pid)){
            doxx=doxx;
            argument=argument;
            rt_mutex_unlock(&router->plock);
            return port;
        } 
    }
        
    rt_mutex_unlock(&router->plock);
    return NULL;
}


static inline struct router_t *
router_new(int rid)
{
    struct router_t *r;

    r = kmalloc(sizeof(struct router_t), MPF_CLR, -1);
    if(likely(r)){
        r->recycle = ALLOWED;
        r->rid = rid;
        INIT_LIST_HEAD(&r->plist);
        rt_mutex_init(&r->plock, NULL);
    }

    return r;
}

static inline void router_release(struct router_t *r)
{
     if(likely(r) &&
                (likely(r->recycle == ALLOWED))){
         kfree(r);
    }
}

static inline void
router_add(struct router_t *r)
{
    if(likely(r)){
        list_add_tail(&r->rlist, &routerlist);
    }
}

static inline void
router_del(struct router_t *r)
{
    if(likely(r)){
        rport_del(r, PID_MAX, NULL);
        list_del(&r->rlist);
        router_release(r);
    }
}

void router_ctrl(int rid,
                        int pid,
                        int opctrl,
                        routine_t routine, 
                        void *argument)
{
    struct router_t *_this, *p, *this;
    struct port_t *port = NULL;
    
    if(likely(rid == RID_INVALID)){
        rt_log_warning(ERRNO_WARNING, 
            "Invalid router (rid=%d)", rid);
        return;
    }
    
    rt_mutex_lock(&routerlist_lock);
    list_for_each_entry_safe(_this, p, &routerlist, rlist){
        if(likely(_this->rid == rid))
            goto find;
    }
    
    /** _this maybe not null after list_for_each_entry_safe */
    _this = NULL;
    
    if(likely(opctrl & R_CTRL_DEL) ||
        likely(opctrl & R_CTRL_MOD) ||
        likely(opctrl & R_CTRL_CHK) ||
        likely(opctrl & R_CTRL_UD) ||
                likely(opctrl & R_CTRL_PORT_ADD) ||
                likely(opctrl & R_CTRL_PORT_DEL) ||
                likely(opctrl & R_CTRL_PORT_MOD) ||
                likely(opctrl & R_CTRL_PORT_CHK)){
        rt_log_warning(ERRNO_WARNING, 
            "No such router or port (%d:%d)", rid, pid);
        goto finish;
    }
    
    if(likely(opctrl & R_CTRL_ADD)){
        this = router_new(rid);
        if(likely(this)){
            if (routine) routine(this, NULL, opctrl, argument);
            router_add(this);
         }
    }

find:
                        
    if(likely(_this)){
  
        if(likely(opctrl & R_CTRL_ADD)){
            rt_log_warning(ERRNO_WARNING, 
                "The same router (%d)", _this->rid);
            goto finish;
        }
            
        if(likely(opctrl & R_CTRL_DEL)){
            if (routine) routine(_this, port, opctrl, argument);
            router_del(_this);
        }
            
        if(likely(opctrl & R_CTRL_PORT_ADD)){
            port = rport_lookup(_this, pid);
            if(likely(port)){
                rt_log_warning(ERRNO_WARNING, 
                        "The same port (%d:%d)", _this->rid, pid);
                goto finish;
            }
            port = rport_add(_this, pid, NULL, NULL);
            if (routine) routine(_this, port, opctrl, argument); 
        }

        if(likely(opctrl & R_CTRL_PORT_DEL)){
            port = rport_lookup(_this, pid);
            if(unlikely(!port)){
                rt_log_warning(ERRNO_WARNING, 
                        "No such port (%d:%d)", _this->rid, pid);
                goto finish;
            }
            if (routine) routine(_this, port, opctrl, argument);   
            rport_del(_this, pid, NULL);
        }
    }

finish:
    rt_mutex_unlock(&routerlist_lock);
}

void router_for_each_port(int rid, 
                        routine_t routine, 
                        void *argument)
{
    struct router_t *router, *p;
    
    rt_mutex_lock(&routerlist_lock);
    list_for_each_entry_safe(router, p, &routerlist, rlist){
        if(likely(router->rid == rid) && likely(routine)){
            struct port_t *port, *pp;
            rt_mutex_lock(&router->plock);
            list_for_each_entry_safe(port, pp, &router->plist, list){
                routine((void *)router, port, 0, argument);
            }
            rt_mutex_unlock(&router->plock);
        }
    }
    rt_mutex_unlock(&routerlist_lock);
}

void router_for_each(int pid, 
                        routine_t routine,
                        void *argument)
{
    struct router_t *_this, *p;

    if(likely(routine)){

        rt_mutex_lock(&routerlist_lock);
        list_for_each_entry_safe(_this, p, &routerlist, rlist)
            routine((void *)_this, NULL, pid, argument);
        rt_mutex_unlock(&routerlist_lock);

    }    
}

