#ifndef __CLUSTER_PROB_H__
#define __CLUSTER_PROB_H__

#include "probe.h"

struct pc_hostlist{

    rt_mutex mtx;

    struct pc_host *head, *tail;

    int count;
};

struct probe_cluster {
#ifdef RT_TMR_ADVANCED
    uint32_t tm_kalive;
#else
    int tm_kalive;
#endif
#ifdef RT_TMR_ADVANCED
    uint32_t tm_link_detection;
#else
    int tm_link_detection;
#endif
    int kalive_interval;
    int link_detection_interval;
    struct pc_hostlist *host_list;
    int (*connect)(void *h);
    void (*shutdown)(void *);
};

extern struct pc_hostlist *local_probe_cluster();
extern void probe_cluster_init();

extern int probe_cluster_add(const char *host, const char *port);

extern int probe_cluset_del(const char *host, const char *port);

extern int probe_broadcast(char *packet, ssize_t pkt_len);

#define probe_cluster_foreach_host(el, entry)  \
    for(entry = ((struct pc_hostlist*)(el))->head; entry; entry = entry->next)

static inline void probe_lock(struct pc_host *prob){
    rt_mutex_lock(&prob->mtx);
}
static inline void probe_unlock(struct pc_host *prob){
    rt_mutex_unlock(&prob->mtx);
}

static inline void probelist_lock(struct pc_hostlist *prob_list){
    rt_mutex_lock(&prob_list->mtx);
}
static inline void probelist_unlock(struct pc_hostlist *prob_list){
    rt_mutex_unlock(&prob_list->mtx);
}

#endif
