/*
*   cluster_prob.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: The Client connection list
*   Personal.Q
*/
#include "sysdefs.h"
#include "prefix.h"
#include "sockunion.h"
#include "conf.h"
#include "conf-yaml-loader.h"
#include "cluster_probe.h"
#include "keepalive.h"
#include "rpc_probe.h"

static int cleanup_probe_cluster = 0;
static int disconnected_probes = 0;
static INIT_MUTEX(mtx);

#define SAMPLE_INTERVAL_US    5000000

static const struct keepalive keepalive2probe =
{
    .manage = 0x06,
    .optype = 0x01,
    .result = 0x00,
    .resv = 0x00,
};

void keepalive(void *to)
{
    int fd = *(int *)(char *)to;
    char keepalive_msg[256] = {0};
    memcpy(keepalive_msg, (void *) &keepalive2probe, sizeof(keepalive2probe));
    rt_sock_send(fd, keepalive_msg, sizeof(keepalive2probe));
}

static inline void disconnected_inc()
{
    rt_mutex_lock(&mtx);
    disconnected_probes ++;
    rt_mutex_unlock(&mtx);
}

static inline void disconnected_dec()
{
    rt_mutex_lock(&mtx);
    disconnected_probes --;
    rt_mutex_unlock(&mtx);
}

static inline int disconnected_get()
{
    int disconcnt = 0;

    rt_mutex_lock(&mtx);
    disconcnt = disconnected_probes;
    rt_mutex_unlock(&mtx);

    return disconcnt;
}

static inline void *
pc_hostlist_init()
{
    struct pc_hostlist *el;

    rt_kmalloc((void **)&el, sizeof(struct pc_hostlist));
    if(likely(el)){
        el->count = 0;
        el->head = el->tail = 0;
        rt_mutex_init(&el->mtx, NULL);
    }
    
    return el;
}

static int  __attribute__((__unused__))
pc_hostlist_retrieve(struct pc_hostlist *hl, const char * desc)
{
    struct pc_host *h = NULL;
    char entry_str[4096] = {0};

    printf ("\n\n\n============= %s (%d entries) ==============\n", desc ? desc : "ANY", hl->count);
    probe_cluster_foreach_host(hl, h)
    {
        printf ("%s", entry_str);
    }

    return 0;
}

static inline struct pc_host *
pc_hostlist_find(struct pc_hostlist * hl, int port, struct prefix *prefix)
{
    struct pc_host *h = NULL;
    struct prefix *xprefix = NULL;

    probe_cluster_foreach_host(hl, h){
        xprefix = sockunion2prefix(&h->su, &h->mask);
        if (xprefix){
            if (h->port == port &&
                    !prefix_cmp(xprefix, prefix)){
                prefix_free(xprefix);
                break;
            }
            prefix_free(xprefix);
        }
    }

    return h;
}

static inline struct pc_host*
pc_hostlist_del(struct pc_hostlist * hl, struct pc_host * h)
{
    if (h->prev)
        h->prev->next = h->next;
    else
        hl->head = h->next;

    if (h->next)
        h->next->prev = h->prev;
    else
        hl->tail = h->prev;

    h->next = h->prev = NULL;
    hl->count--;

    return h;
}

static inline void
pc_hostlist_add(struct pc_hostlist *hl, struct pc_host *h)
{
    h->next = NULL;
    h->prev = hl->tail;

    if (hl->tail)
        hl->tail->next = h;
    else
        hl->head = h;

    hl->tail = h;
    hl->count++;
}

static void do_shutdown(void *xh)
{
    struct pc_host *h = xh;
    rt_sock_shdown(h->sock, NULL);
}

static inline int
pc_del_host(struct pc_hostlist *hl, const char *host, const char *port)
{
    int xp = 0;
    struct prefix p;
    struct pc_host *h = NULL;
    int er = -1;

    xp = tcp_udp_port_parse(port);
    if (xp < 0){
        rt_log_error(ERRNO_INVALID_VAL, "invalid \"%s\"\n", port);
        goto finish;
    }
    if (!str2prefix(host, &p)){
        rt_log_error(ERRNO_INVALID_VAL, "invalid \"%s\"\n", host);
        goto finish;
    }

    probelist_lock(hl);
    h = pc_hostlist_find(hl, xp, &p);
    if (h){
        if (h->sock > 0){
            disconnected_inc();
        }
        h->valid = INVALID;
        er = 0;
    }
    probelist_unlock(hl);

finish:
    return er;
}

static inline int
pc_add_host(struct pc_hostlist *hl, const char *host, const char *port)
{
    int xp = 0;
    struct pc_host *xh;
    struct pc_host *h = NULL;
    struct prefix p;
    struct in_addr mask;

    xp = tcp_udp_port_parse(port);
    if (xp < 0){
        rt_log_error(ERRNO_INVALID_VAL, "invalid \"%s\"\n", port);
        goto finish;
    }

    if (!str2prefix(host, &p)){
        rt_log_error(ERRNO_INVALID_VAL, "invalid \"%s\"\n", host);
        goto finish;
    }

    rt_kmalloc((void **)&xh, sizeof(struct pc_host));
    if(likely(xh)){
        
        xh->valid = VALID;
        xh->sock = -1;
        xh->port = xp;
        xh->su.sa.sa_family = AF_INET;
        masklen2ip(p.prefixlen, &mask);
        memcpy(&xh->su.sin.sin_addr, &p.u.prefix4, sizeof (xh->su.sin.sin_addr));
        memcpy(&xh->mask.sin.sin_addr, &mask, sizeof(xh->mask.sin.sin_addr));

        rt_mutex_init(&xh->mtx, NULL);

        probelist_lock(hl);
        h = pc_hostlist_find(hl, xp, &p);
        if (h != NULL){
            probelist_unlock(hl);
            goto finish;
        }
        pc_hostlist_add(hl, xh);
        disconnected_inc();
        probelist_unlock(hl);
    }
    
    return 0;
finish:
    free(xh);
    return -1;
}


static int
config_cluster(ConfNode *base, struct pc_hostlist *dh)
{
    char *host, *port;
    ConfNode *slave_node = NULL;

    host = port = NULL;
    TAILQ_FOREACH(slave_node, &base->head, next){
        if(!STRCMP(slave_node->name, "host")){
            host = slave_node->val;
        }
        if(!STRCMP(slave_node->name, "port")){
            port = slave_node->val;
        }
    }

    return pc_add_host(dh, host, port);
}

static void
do_kalive(uint32_t __attribute__((__unused__)) uid,
                      int __attribute__((__unused__))argc,
                      char **argv)
{
#if 1
    struct probe_cluster *prob = (struct probe_cluster *)argv;
    struct pc_host *h = NULL;
    int xret = -1;

    probelist_lock(prob->host_list);

    if (cleanup_probe_cluster){
        printf ("cleanup probe cluster\n");
        cleanup_probe_cluster = 0;
    }

    probe_cluster_foreach_host(prob->host_list, h){
        if (h->valid &&
            h->sock > 0){
            xret = rt_sock_send(h->sock, (char *)&keepalive2probe, sizeof(keepalive2probe));
            if (xret < 0) {
                rt_log_info("Disconnected with (%s:%d, sock=%d)\n",
                    sockunion_su2str(&h->su), h->port, h->sock);
                rt_sock_shdown(h->sock, NULL);
                h->sock = -1;
                disconnected_inc();
            }
        }
    }
    probelist_unlock(prob->host_list);
#else
    cleanup_probe_cluster = cleanup_probe_cluster;
    argv = argv;
#endif
    return;
}


static void
do_link_detection(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char **argv)
{
#if 1
    struct probe_cluster *prob = (struct probe_cluster *)argv;
    struct pc_host *h = NULL;
    int xret = -1;

    /** No probe should be connected to */
    if (disconnected_get() == 0){
        //rt_log_debug ("no more link\n");
        return;
    }

    probelist_lock(prob->host_list);
    probe_cluster_foreach_host(prob->host_list, h){
        if (h->valid &&
            h->sock < 0){
            xret = prob->connect(h);
            if (xret > 0)
                disconnected_dec();
        }
        if (!h->valid){
            pc_hostlist_del(prob->host_list, h);
            if (h->sock > 0){
                do_shutdown(h);
            }
            disconnected_dec();
            free(h);
        }
    }
    probelist_unlock(prob->host_list);
#else
    argv = argv;
#endif
    return;
}

void *collect_statistics(void *param)
{
    param = param;
    struct pc_host *prob = NULL;
    struct pc_hostlist *probe_list = NULL;
    int xret = -1;

#ifndef RT_TMR_ADVANCED
    /*Shield sinal of sigalrm Otherwise, the list of probe_list deadlock*/
    rt_signal_block(SIGALRM);
#endif

    probe_list = local_probe_cluster();
    FOREVER
    {
        usleep(SAMPLE_INTERVAL_US);
        if (likely(probe_list)){
            probelist_lock(probe_list);
            probe_cluster_foreach_host(probe_list, prob) {
                if (prob->valid &&
                    prob->sock > 0){
                    xret = cpb_cluster_RequestCntmsg(prob);
                    if (xret < 0){
                        rt_log_info("Disconnected with (%s:%d, sock=%d)\n",sockunion_su2str(&prob->su), prob->port, prob->sock);
                    }
                }
            }
            probelist_unlock(probe_list);
        }
    }

    return NULL;
}

static struct rt_task_t collect_statistics_task =
{
    .module = THIS,
    .name = "Local Probe Cluster DAQ Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = collect_statistics,
};

static void
probe_cluster_startup(struct probe_cluster *pb)
{
#ifdef RT_TMR_ADVANCED
    tmr_start(pb->tm_kalive);
    tmr_start(pb->tm_link_detection);
#else
    rt_timer_start(pb->tm_kalive, pb->kalive_interval * 1000,
        &do_kalive, 1, (char **)pb);
    rt_timer_start(pb->tm_link_detection, pb->link_detection_interval * 1000,
        &do_link_detection, 1, (char **)pb);
#endif
}

static int
probe_cluster_config(struct probe_cluster *dc)
{
    int xret = -1;
    ConfNode *current = NULL;
    ConfNode *conf_node = NULL;

    ConfNode *base = ConfGetNode("local-probe-cluster");
    if (NULL == base){
        rt_log_error(ERRNO_FATAL, "Can not get local probe cluster configuration\n");
        goto finish;
    }

    TAILQ_FOREACH(current, &base->head, next){
        if(!STRCMP(current->name, "keepalive-interval")){
            dc->kalive_interval = integer_parser(current->val, 0, 86400);
            if (dc->kalive_interval < 0){
                rt_log_error(ERRNO_INVALID_VAL, "keepalive interval invalid\n");
                goto finish;
            }
            //rt_log_debug("dc->kalive_interval = %d\n", dc->kalive_interval);
        }

        if(!STRCMP(current->name, "link-detection-interval")){
            dc->link_detection_interval = integer_parser(current->val, 0, 86400);
            if (dc->link_detection_interval < 0){
                rt_log_error(ERRNO_INVALID_VAL, "link-detection-interval de interval invalid\n");
                goto finish;
            }
            //rt_log_debug("dc->link_detection_interval = %d\n", dc->kalive_interval);
        }

        if(!STRCMP(current->name, "cluster")){
            TAILQ_FOREACH(conf_node, &current->head, next){
                config_cluster(conf_node, dc->host_list);
            }
        }
    }
#ifdef RT_TMR_ADVANCED
        dc->tm_link_detection = tmr_create(LocalProbeCluster, "probe cluster link detection", TMR_PERIODIC,
                                           do_link_detection, 1, (char **)dc, dc->link_detection_interval);
        if (likely((int32_t)dc->tm_link_detection == TMR_INVALID)){
            rt_log_error(ERRNO_TM_INVALID, "Can not create link-detection\n");
            goto finish;
        }
#else
        dc->tm_link_detection = rt_timer_create(LocalProbeCluster, TYPE_LOOP);
        if (dc->tm_link_detection < 0){
            rt_log_error(ERRNO_TM_INVALID, "Can not create link-detection\n");
            goto finish;
        }
#endif

#ifdef RT_TMR_ADVANCED
    dc->tm_kalive = tmr_create(LocalProbeCluster, "cluster probe keeplive ", TMR_PERIODIC,
                               do_kalive, 1,(char **)dc, dc->kalive_interval);
    if (likely((int32_t)dc->tm_kalive == TMR_INVALID)){
        rt_log_error(ERRNO_TM_INVALID, "Can not create timer for keepalive\n");
        goto finish;
    }
#else
    dc->tm_kalive = rt_timer_create(LocalProbeCluster, TYPE_LOOP);
    if (dc->tm_kalive < 0){
        rt_log_error(ERRNO_TM_INVALID, "Can not create timer for keepalive\n");
        goto finish;
    }
#endif

    xret = 0;
finish:
    return xret;
}

static int do_connect_to(void *xh)
{
    struct sockaddr_in sock_addr;
    union sockunion *su;
    struct pc_host *h = xh;

    if (!h->valid)
        return -1;

    su = &h->su;
    //su->sa.sa_family == AF_INET;
    h->sock = 0;
    h->sock = rt_clnt_sock(0, sockunion_su2str(su), h->port, su->sa.sa_family);

    if (h->sock > 0){
        if (!rt_sock_getpeername(h->sock, &sock_addr))
            rt_log_info("Connect to (%s:%d, sock=%d)\n",
                sockunion_su2str(&h->su), ntohs(sock_addr.sin_port), h->sock);
    }

    return h->sock;
}

static struct probe_cluster clst4probe = {
    .tm_kalive = -1,
    .tm_link_detection = -1,
    .kalive_interval = 0,
    .link_detection_interval = 0,
    .connect = do_connect_to,
    .shutdown = do_shutdown,
    .host_list = NULL,
};

struct pc_hostlist *local_probe_cluster()
{
    return clst4probe.host_list;
}

int probe_broadcast(char *packet, ssize_t pkt_len)
{
    struct pc_hostlist *pc = clst4probe.host_list;
    struct pc_host *h = NULL;
    int xret = -1;

    probelist_lock(pc);
    probe_cluster_foreach_host(pc, h){
        if (h->valid &&
            h->sock > 0){
            xret = rt_sock_send(h->sock, packet, pkt_len);
            if (xret < 0){
                rt_log_info("Disconnected with (%s:%d, sock=%d)\n",
                    sockunion_su2str(&h->su), h->port, h->sock);
            }else{
                xret = 0;
            }
        }
    }
    probelist_unlock(pc);

    return xret;
}

int probe_cluster_add(const char *host, const char *port)
{
    return  pc_add_host(clst4probe.host_list, host, port);
}

int probe_cluset_del(const char *host, const char *port)
{
    return  pc_del_host(clst4probe.host_list, host, port);
}

void probe_cluster_init()
{
    clst4probe.host_list = pc_hostlist_init();
    probe_cluster_config(&clst4probe);
    probe_cluster_startup(&clst4probe);

    /** collect probe statistics */
    task_registry(&collect_statistics_task);
}

