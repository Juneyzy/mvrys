#include "sysdefs.h"
#include "conf.h"
#include "routers.h"


struct port_param_t{
    int     rid;        /** which router this port belong to */
    int     pid;        /** port id */
    char    ipstr[32], macstr[32];
};

/************************************ TEST FUNCTION BELOW **********************************************/

static inline void rport_config(struct router_t *router,
                        struct port_t __attribute__((__unused__))*port,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{
    struct port_param_t *p = (struct port_param_t *)arg;
    
    if(likely(router)){
        if(likely(arg)){
            printf("add port: (%d:%d), %s,  %s\n", p->rid, p->pid, p->ipstr, p->macstr);
            port->rid = p->rid;
            port->pid = p->pid;
            port->ip = 0;
            port->mac = 0;
        }
    }
}

static inline void router_xprint(struct router_t *router,
                        struct port_t __attribute__((__unused__))*port,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{  
    if(likely(router)){
        if(likely(port))
            printf("show: (%d:%d)\n", router->rid, port->pid);
        else
            printf("show: (%d)\n", router->rid);
    }
}

static inline void router_add_r(struct router_t *router,
                        struct port_t __attribute__((__unused__))*port,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{
    if(likely(router)){
        printf("add router: (%d)\n", router->rid);
    }
}

static inline void router_del_r(struct router_t *router,
                        struct port_t __attribute__((__unused__))*port,
                        int __attribute__((__unused__))flags,
                        void __attribute__((__unused__))*arg)
{  
    if(likely(router)){
        printf("del router: (%d)\n", router->rid);
    }
}

void
router_config(ConfNode *base, int rid)
{
    ConfNode *local;
    int times = 0;
    struct port_param_t   *port;
    
    router_ctrl(rid, PID_INVALID, R_CTRL_ADD, router_add_r, (void *)"create router");
    port = kmalloc(sizeof(struct port_param_t), MPF_CLR, -1);
    port->pid = PID_INVALID;
    port->rid = rid;
    
    TAILQ_FOREACH(local, &base->head, next){
        ConfNode *pval, *pval1;
        TAILQ_FOREACH(pval, &local->head, next) {
            times = 0;
            if(!STRCMP(pval->name, "interface")){
                TAILQ_FOREACH(pval1, &pval->head, next) {
                    if(!times && isalldigit(pval1->name)){
                        port->pid = integer_parser(pval1->name, 0, INT_MAX);
                        if (port->pid < 0){
                            rt_log_error(ERRNO_INVALID_VAL, "Router interface ID invalid\n");
                            goto finish;
                        }
                    }
                    if(times == 1){
                        memcpy(&port->ipstr[0], pval1->name, strlen(pval1->name));
                    }
                    if(times == 2){
                        memcpy(port->macstr, pval1->name, strlen(pval1->name));
                    }
                    times ++;
                }
            }
         }
        router_ctrl(rid, port->pid, R_CTRL_PORT_ADD, rport_config, (void *)port);
    }
    
finish:
    return;
}

 int
routers_config()
{
    int xret = -1;
    ConfNode *base, *local;
    int rid = -1;

     base = ConfGetNode("routers");
     if(base){
        TAILQ_FOREACH(local, &base->head, next){
            ConfNode *pval;
            TAILQ_FOREACH(pval, &local->head, next) {
                 if(!STRCMP(pval->name, "rid")){
                    rid = integer_parser(pval->val, 0, INT_MAX);
                    if (rid < 0){
                        rt_log_error(ERRNO_INVALID_VAL, "Router ID invalid\n");
                        goto finish;
                    }
                    
                    
                 }
                
                if(!STRCMP(pval->name, "interfaces")){
                    router_config(pval, rid);
                 }
             }
            router_for_each_port(rid, router_xprint, NULL);
        }
     }
     
goto finish;

finish:
    return xret;
}


void router_test()
{
    ConfYamlLoadFromFile("../conf/routers.yaml");
    routers_config();

}


