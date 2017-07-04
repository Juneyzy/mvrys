#include "sysdefs.h"
#include "prefix.h"
#include "sockunion.h"
#include "conf.h"
#include "lwrpc.h"
#include "service.h"
#include "conf-yaml-loader.h"
#include "cluster_decoder.h"
#include "cluster_probe.h"

static int disconnected_decoders;
static INIT_MUTEX(mtx);

struct dc_host {
    char *desc;

    int valid;

    int sock;

    /** connection port of slave */
    uint16_t port;

    union sockunion su;

    rt_mutex mtx;

    int args;
    void **argv;

    /**
        which cluster does the host blong to, if local decoder in MASTER mode,
        cluster=itself(decoder)
    */
    void *cluster;

    /** */
    int timedout;

    int (*proc_fn)(int, char *, ssize_t, int, void **, void *);

    void (*timedout_fn)();

    /** */
    struct dc_host *next, *prev;
};

struct dc_hostlist{
    rt_mutex mtx;
    struct dc_host *head, *tail;
    int count;
};

static inline void *cd_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

/** Decoder cluster lock && unlock */
static inline void dc_lock(struct dc_hostlist *list) {
    rt_mutex_lock(&list->mtx);
}

static inline void dc_unlock(struct dc_hostlist *list){
    rt_mutex_unlock(&list->mtx);
}

static inline void disconnected_inc()
{
    rt_mutex_lock(&mtx);
    disconnected_decoders ++;
    rt_mutex_unlock(&mtx);
}

static inline void disconnected_dec()
{
    rt_mutex_lock(&mtx);
    disconnected_decoders --;
    rt_mutex_unlock(&mtx);
}

static inline int disconnected_get()
{
    int disconcnt = 0;

    rt_mutex_lock(&mtx);
    disconcnt = disconnected_decoders;
    rt_mutex_unlock(&mtx);

    return disconcnt;
}

#define hostlist_foreach_host(el, entry)  \
    for(entry = ((struct dc_hostlist*)(el))->head; entry; entry = entry->next)

static inline void *
dc_hostlist_init()
{
    struct dc_hostlist *el;

    el = (struct dc_hostlist *)cd_kmalloc(sizeof(struct dc_hostlist));
    if(likely(el)){
        el->count = 0;
        el->head = el->tail = 0;
        rt_mutex_init(&el->mtx, NULL);
    }

    return el;
}

static inline struct dc_host *
dc_hostlist_find(struct dc_hostlist *hl, int port, struct prefix *prefix)
{
    struct dc_host *h = NULL;
    struct prefix *xprefix;

    hostlist_foreach_host(hl, h){
        xprefix = sockunion2hostprefix(&h->su);
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

static inline void
dc_hostlist_add(struct dc_hostlist *hl,  struct dc_host *h)
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

static inline void
dc_hostlist_add_locked(struct dc_hostlist *hl,  struct dc_host *h)
{
    dc_lock(hl);
    h->next = NULL;
    h->prev = hl->tail;
    if (hl->tail)
        hl->tail->next = h;
    else
        hl->head = h;
    hl->tail = h;
    hl->count++;
    dc_unlock(hl);
}

static inline struct dc_host *
dc_hostlist_del(struct dc_hostlist *hl,  struct dc_host *h)
{
    if (h->next)
        h->next->prev = h->prev;
    else
        hl->tail = h->prev;

    if (h->prev)
        h->prev->next = h->next;
    else
        hl->head = h->next;

    h->next = h->prev = NULL;
    hl->count--;

    return h;
}

static inline struct dc_host *
dc_hostlist_del_locked(struct dc_hostlist *hl,  struct dc_host *h)
{
    dc_lock(hl);
    if (h->next)
        h->next->prev = h->prev;
    else
        hl->tail = h->prev;
    if (h->prev)
        h->prev->next = h->next;
    else
        hl->head = h->next;
    h->next = h->prev = NULL;
    hl->count--;
    dc_unlock(hl);
    return h;
}
static inline struct dc_host *
dc_hostlist_trim_head(struct dc_hostlist *hl)
{
    if (hl->head)
        return dc_hostlist_del(hl, hl->head);

    return NULL;
}

static inline struct dc_host *
dc_hostlist_trim_tail(struct dc_hostlist *hl)
{
    if (hl->head)
        return dc_hostlist_del(hl, hl->tail);

    return NULL;
}

static inline int
dc_del_host(struct decoder *dc, const char *host, const char *port)
{
    int xp = 0;
    struct prefix p;
    struct dc_host *h = NULL;
    struct dc_hostlist *hl = dc->hostlist;
    int er = -1;

    xp = tcp_udp_port_parse(port);
    if(xp < 0){
        rt_log_error(ERRNO_DECODER_YAML_ARGU, "port \"%s\" parse error\n", port);
        goto finish;
    }

    if (!str2prefix(host, &p)){
        rt_log_error(ERRNO_DECODER_YAML_ARGU, "ipaddress \"%s\" parse error\n", host);
        goto finish;
    }

    dc_lock(hl);
    h = dc_hostlist_find(hl, xp, &p);
    if (h){
        if (h->sock > 0){
            disconnected_inc();
        }
        h->valid = INVALID;
    }
    dc_unlock(hl);

finish:
    return er;
}

static inline int
dc_add_host(struct decoder *dc, const char *host, const char *port)
{
    int xp = 0;
    struct dc_host *xh;
    struct dc_host *h = NULL;
    struct dc_hostlist *hl = dc->hostlist;
    struct prefix p;

    xp = tcp_udp_port_parse(port);
    if(xp < 0){
        rt_log_error(ERRNO_DECODER_YAML_ARGU, "port \"%s\" parse error\n", port);
        goto finish;
    }

    memset(&p, 0, sizeof(struct prefix));
    if (!str2prefix(host, &p)){
        rt_log_error(ERRNO_DECODER_YAML_ARGU, "ipaddress \"%s\" parse error\n", host);
        goto finish;
    }

    rt_kmalloc((void **)&xh, sizeof(struct dc_host));
    if(likely(xh)){
        xh->port = xp;
        xh->sock = -1;
        xh->valid = VALID;
        xh->su.sa.sa_family = AF_INET;
        memcpy(&xh->su.sin.sin_addr, &p.u.prefix4, sizeof (xh->su.sin.sin_addr));
        rt_mutex_init(&xh->mtx, NULL);

        dc_lock(hl);
        h = dc_hostlist_find(hl, xp, &p);
        if (h != NULL){
            rt_log_error(ERRNO_DECODER_YAML_ARGU, "exsit host(%s:%s)\n", host, port);
            dc_unlock(hl);
            goto finish;
        }
        dc_hostlist_add(hl, xh);
        disconnected_inc();

        dc_unlock(hl);
    }
    
    return 0;
finish:
    free(xh);
    return -1;
}

static ssize_t dc_talkto(void *host,
    void *data, size_t s)
{
    struct dc_host *h = host;
    assert(h);
    assert(data);
    return rt_sock_send(h->sock, data, s);
}

static ssize_t dc_hearfrom(void *host,
    void *buffer, size_t s)
{
    enum rt_sock_errno rn = SOCK_CONTINUE;
    struct dc_host *h;
    int xret = 0;

    h = host;
    rn = rt_sock_recv_timedout(__func__,
            h->sock, buffer, s, h->timedout, h->proc_fn, h->timedout_fn, 0, NULL, NULL);
    if (rn == SOCK_CLOSE){
        rt_log_info("peer closed\n");
        xret = -1;
        goto finish;
    }

finish:
    return xret;
}

enum dc_type decoder_type ()
{
    enum dc_type type = DT_INVALID;

    ConfNode *base = ConfGetNode("local-decoder-type");
    if (NULL == base){
        rt_log_error(ERRNO_FATAL, "Can not get local decoder type configuration\n");
        goto finish;
    }

    if (!STRCMP(base->val, "master")){
        type  = DT_MASTER;
        goto finish;
    }

    if(!STRCMP(base->val, "slave")){
        type  = DT_SLAVE;
        goto finish;
    }
    type = DT_INVALID;

finish:
    return type;
}


static int
decoder_config_master_cluster(ConfNode *base, struct decoder *dc)
{
    char *host, *port;
    ConfNode *master_node = NULL;

    host = port = NULL;
    TAILQ_FOREACH(master_node, &base->head, next){
        if(!STRCMP(master_node->name, "host")){
            host = master_node->val;
        }
        if(!STRCMP(master_node->name, "port")){
            port = master_node->val;
        }
    }
    return dc_add_host(dc, host, port);
}

static void decoder_dump_cluster(struct decoder *dc)
{
    struct dc_host *h;
    printf ("%60s\n", "HOST-LIST");
    printf ("%60s\n", "=========");
    hostlist_foreach_host(dc->hostlist, h){
        printf("%60s:%d\n", sockunion_su2str(&h->su), h->port);
    }
}

static void decoder_dump_configuration(struct decoder *dc)
{
    printf("\r\nDecoder Cluster Preview\n");
    if(dc->type == DT_MASTER){
        printf("%10s%20s%20s%10s\n", "TYPE", "KALIVE(s)", "LINKDECT(s)", "HOST(S)");
        printf("%10s%20s%20s%10s\n", "====", "=========", "===========", "=======");
        printf("%10s%20d%20d%10d\n", "master", dc->kalive_interval, dc->link_detection_interval,
            ((struct dc_hostlist *)dc->hostlist)->count);
        decoder_dump_cluster(dc);
    }

    if (dc->type == DT_SLAVE){

    }

    printf("\n\n\n");
}

static void
do_kalive(uint32_t __attribute__((__unused__)) uid,
          int __attribute__((__unused__))argc,
          char **argv)
{
    //const char *kalive = "{ \"head\": { \"version\": \"1.0.102\", \"router\": 0,  \"port\": 0, \"direction\": 0, \"command\":200, \"sub_command\": 0, \"sequence\": 100000, \"errno\": 0, \"start_ts\": 1440569083,  \"end_ts\": 1440569000, \"interval\": 3600 }}";
    int l = 0;
    struct decoder *dc = (struct decoder *)argv;
    struct dc_host *h = NULL;
    struct dc_hostlist *hl;

    #if 1
    declare_array(char, kalive, 128);
    #endif
    int xret = -1;

    hl = dc->hostlist;
    dc_lock(hl);
    hostlist_foreach_host(hl, h){
#if 1
        l = 0;
        l = snprintf (kalive, 128 - 1, "%s, %s:%d\n", "hello", sockunion_su2str(&h->su), h->port);
#endif

        if (h->valid &&
            h->sock > 0 &&
            l > 0){
            xret = dc->talkto(h, (void *)kalive, l);
            if (xret < 0){
                rt_log_error(ERRNO_SOCK_SEND, "%s", strerror(errno));
                dc->shutdown(h);
                disconnected_inc();
            }
        }
    }
    dc_unlock(hl);
    return;
}

static void
do_link_detection(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char **argv)
{
    struct decoder *dc = (struct decoder *)argv;
    struct dc_host *h = NULL;
    struct dc_hostlist *hl;
    int xret = -1;

    hl = dc->hostlist;

    /** No probe should be connected to */
    if (disconnected_get() == 0){
        //rt_log_debug ("No target decoder is required to establish a connection\n");
        return;
    }

    rt_log_debug ("  %d unestablished host(s) detected\n", disconnected_get());
    dc_lock(hl);
    hostlist_foreach_host(hl, h){
        if (h->valid &&
            h->sock < 0){
            xret = dc->sock_routine(h);
            if (xret < 0) {
                rt_log_info ("Unestablished host(%s, %d) detected, trying to connect ...  failed(%d, %s)\n",
                    sockunion_su2str(&h->su), h->port, xret, strerror(errno));
                continue;
            }
            disconnected_dec();
            rt_log_info ("Unestablished host(%s, %d) detected, trying to connect ...  success(%d)\n",
                sockunion_su2str(&h->su), h->port, xret);
        }
        if (!h->valid){
            //dc->hostlist_del(hl, h);
            dc_hostlist_del(hl, h);
            if (h->sock > 0){
                dc->shutdown(h);
            }
            disconnected_dec();
            free(h);
        }
    }
    dc_unlock(hl);
    return;
}

static int
decoder_treated_as_master(ConfNode *base, struct decoder *dc)
{
    int xret = -1;
    ConfNode *master_node = NULL;

    TAILQ_FOREACH(master_node, &base->head, next){
        if(!STRCMP(master_node->name, "keepalive-interval")){
            dc->kalive_interval = integer_parser(master_node->val, 0, 86400);
            if (dc->kalive_interval < 0){
                rt_log_error(ERRNO_INVALID_VAL, "keepalive interval invalid\n");
                goto finish;
            }
        }

        if(!STRCMP(master_node->name, "link-detection-interval")){
            dc->link_detection_interval = integer_parser(master_node->val, 0, 86400);
            if (dc->link_detection_interval < 0){
                rt_log_error(ERRNO_INVALID_VAL, "link-detection-interval de interval invalid\n");
                goto finish;
            }
        }

        if(!STRCMP(master_node->name, "cluster")){
            ConfNode *cluster_node = NULL;
            TAILQ_FOREACH(cluster_node, &master_node->head, next){
                //decoder_config_master_cluster(cluster_node, dc->hostlist);
                decoder_config_master_cluster(cluster_node, dc);
            }
        }
    }

#ifdef RT_TMR_ADVANCED
    dc->tm_link_detection = tmr_create(LocalDecoderCluster, "decoder cluster link detection", TMR_PERIODIC,
                                       do_link_detection, 1, (char **)dc, dc->link_detection_interval);
    if (likely((int32_t)dc->tm_link_detection == TMR_INVALID)){
        rt_log_error(ERRNO_TM_INVALID, "Can not create link-detection timer for decoder cluster\n");
        goto finish;
    }
#else
    dc->tm_link_detection = rt_timer_create(LocalDecoderCluster, TYPE_LOOP);
    if (dc->tm_link_detection < 0){
        rt_log_error(ERRNO_TM_INVALID, "Can not create link-detection timer for decoder cluster\n");
        goto finish;
    }
#endif

#ifdef RT_TMR_ADVANCED
    dc->tm_kalive = tmr_create(LocalDecoderCluster, "decoder cluster keeplive", TMR_PERIODIC,
                               do_kalive, 1, (char **)dc, dc->kalive_interval);
    if (likely((int32_t)dc->tm_kalive == TMR_INVALID)){
        rt_log_error(ERRNO_TM_INVALID, "Can not create keepalive timer for decoder cluster\n");
        goto finish;
    }
#else
    dc->tm_kalive = rt_timer_create(LocalDecoderCluster, TYPE_LOOP);
    if (dc->tm_kalive < 0){
        rt_log_error(ERRNO_TM_INVALID, "Can not create keepalive timer for decoder cluster\n");
        goto finish;
    }
#endif
    decoder_dump_configuration(dc);

    xret = 0;
finish:
    return xret;
}


static int
decoder_treated_as_slave(ConfNode *base, struct decoder *dc)
{
    int xret = -1;
    ConfNode *slave_node = NULL;

    TAILQ_FOREACH(slave_node, &base->head, next){
        rt_log_debug ("%s:%s\n", slave_node->name, slave_node->val);
        if(!STRCMP(slave_node->name, "keep-alive")){
            //dc->timeout = integer_parser(slave_node->val, 0, 86400);
        }

        if(!STRCMP(slave_node->name, "host")){
            /** host is unused while decoder in slave mode */
        }

        if(!STRCMP(slave_node->name, "local-port")){
            dc->listen = tcp_udp_port_parse(slave_node->val);
            if(dc->listen < 0){
                rt_log_error(ERRNO_DECODER_YAML_ARGU, "Port invalid\n");
                goto finish;
            }
        }
    }

    xret = 0;

 finish:
    return xret;
}

static int
decoder_cluster_config(struct decoder *dc)
{
    int xret = -1;
    ConfNode *local = NULL;

    dc->type = decoder_type();
    if(dc->type == DT_MASTER){
         local = ConfGetNode("local-decoder-cluster");
        xret = decoder_treated_as_master(local, dc);
        goto finish;
    }

    if(dc->type == DT_SLAVE){
        local = ConfGetNode("local-decoder");
        xret = decoder_treated_as_slave(local, dc);
        goto finish;
    }
    xret = 0;
    decoder_dump_configuration(dc);
finish:
    return xret;
}

static int
do_connect_to(void *xh)
{
    struct dc_host *h = xh;
    union sockunion *su = &h->su;
    //su->sa.sa_family == AF_INET;
    h->sock = 0;
    h->sock = rt_clnt_sock(0, sockunion_su2str(su), h->port, su->sa.sa_family);
    return h->sock;
}

static int
do_connect_from(void *xdc)
{
    struct decoder *dc = xdc;
    dc->sock = 0;
    dc->sock = rt_serv_sock(0, dc->listen, AF_INET);
    return dc->sock;
}

static void
dc_host_shutdown(void *xh)
{
    struct dc_host *h = xh;
    rt_sock_shdown(h->sock, NULL);
    h->sock = -1;
}

static void do_timedout()
{
    printf ("timedout\n");
    return;
}

static int
broadcast_to_probes(char *packet, ssize_t pkt_len)
{
    return probe_broadcast(packet, pkt_len);
}

static int dc_transmit_rulemsg(char *packet, ssize_t pkt_len)
{
    int xret = -1;

    xret = broadcast_to_probes(packet, pkt_len);
    return xret;
}

static int dc_proc_recvmsg(char *packet, ssize_t pkt_len)
{
    int xret = -1;

    RPC_HEAD_SERV_ST *pHead = (RPC_HEAD_SERV_ST *)packet;
    printf("dc_proc_recvmsg = %d\n", NTOHS(pHead->rpc_id));
    switch(NTOHS(pHead->rpc_id))
    {
        case CpssSetRULE_EMAIL_RPC_ID:
        case CpssSetRULE_IPPORT_RPC_ID:
            xret = dc_transmit_rulemsg(packet, pkt_len);
            break;
    }

    return xret;
}

static int
do_proc(IN int sock,
    IN char *buffer,
    IN ssize_t size,
    IN int __attribute__((__unused__)) argc ,
    IN void __attribute__((__unused__)) **argv,
    OUT void __attribute__((__unused__)) *output_params)
{
    struct sockaddr_in sock_addr;
    int xret = -1;

    if (rt_sock_getpeername(sock, &sock_addr) < 0){
        goto finish;
    }

    if (size > 0)
    {

    #if 1
        if (!STRNCMP(buffer, "hello", 5)){
            xret = rt_sock_send(sock, buffer, size);
            goto finish;
        }else{
            xret = dc_proc_recvmsg(buffer, size);
        }
    #else
        printf ("%s, %s\n", __func__, buffer);
    #endif

    }

    if (size == 0){
        rt_log_warning(ERRNO_DC_PEER_CLOSE, "Disconnected with (%s:%d, sock=%d), local shutdown\n",
                inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);
        goto finish;
    }

    if (size < 0){
        rt_log_error(ERRNO_DC_PEER_ERROR, "Peer (%s:%d, sock=%d), local shutdown\n",
                inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);
    }

finish:
    return xret;
}

static void *
agent_spawn(void *args)
{
#define SIZE 102400
    struct dc_host *host = (struct dc_host *)args;
    struct decoder *dc = NULL;
    int sock;
    struct sockaddr_in sock_addr;
    char *buffer;
    int xret = 0;

    if (!host){
        rt_log_warning(ERRNO_DC_AGENT_SPAWN, "No param for agent\n");
        return NULL;
    }

    sock = host->sock;
    if (sock <= 0 ||
        rt_sock_getpeername(sock, &sock_addr) < 0){
        return NULL;
    }

    rt_log_info("Peer (%s:%d, sock=%d) connected with local decoder which running in slave mode\n",
        inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);

    dc = (struct decoder *)host->cluster;
    if (!dc){
        rt_log_warning(ERRNO_DC_AGENT_SPAWN, "This decoder is a devil\n");
        return NULL;
    }

    rt_kmalloc((void **)&buffer, SIZE);
    assert(buffer);

    FOREVER{
        xret = dc->hearfrom(host, buffer, SIZE);
        if (xret < 0) goto finish;
    }

finish:
    free(buffer);
    dc->shutdown(host);
    sock = -1;
    task_deregistry_named(host->desc);
    rt_mutex_destroy(&host->mtx);
    free(host->desc);
    free(host);
    return NULL;
}

static void *master_fn(void *param)
{
    struct decoder *dc = (struct decoder *)param;
    struct dc_host *h = NULL;

#ifdef RT_TMR_ADVANCED
    tmr_start(dc->tm_kalive);
    tmr_start(dc->tm_link_detection);
#else
    rt_timer_start(dc->tm_kalive, dc->kalive_interval * 1000,
        &do_kalive, 1, (char **)dc);
    rt_timer_start(dc->tm_link_detection, dc->link_detection_interval * 1000,
        &do_link_detection, 1, (char **)dc);
#endif

    FOREVER{
        h = NULL;
        dc_lock(dc->hostlist);
        hostlist_foreach_host(dc->hostlist, h){
            if (h->valid == VALID &&
                h->sock > 0){
                ;
            }
        }
        dc_unlock(dc->hostlist);
        sleep(1);
    }

    return NULL;
}


static inline struct dc_host *
dc_host_alloc(struct decoder *dc,
                    int sock)
{
    struct dc_host *host;
    declare_array(char, desc, TASK_NAME_SIZE);

    snprintf (desc, TASK_NAME_SIZE - 1, "do_a_transaction%d", sock);
    
    host = (struct dc_host *)cd_kmalloc(sizeof(struct dc_host));
    if (likely(host)){
        rt_mutex_init(&host->mtx, NULL);
        host->desc = strdup(desc);
        host->sock = sock;
        host->cluster = dc;
        host->proc_fn = do_proc;
        host->timedout_fn = do_timedout;
        host->timedout = 3000000;
    }
    
    return host;
}


static void *accept_task(void *param)
{
    int accept_sock = -1;
    int sock;
    struct decoder *dc = (struct decoder *)param;
    int xret = 0, tmo = 0;

    if(!dc ||
        !dc->sock_routine){
        rt_log_warning(ERRNO_FATAL, "Can not startup in slave mode\n");
        goto finish;
    }

    if ((accept_sock = dc->sock_routine(dc)) < 0){
        rt_log_error(ERRNO_FATAL, "Can not do a sock for slave decoder\n");
        goto finish;
    }

    FOREVER {
        xret = is_sock_ready(accept_sock, 30000000, &tmo);
        if (xret == 0 && tmo == 1)/** xret == 0 && tmo=1: accept timedout */
            continue;
        if (xret < 0)
            goto finish;

        sock = rt_serv_accept(0, accept_sock);
        if (sock <= 0){
            rt_log_error(ERRNO_SOCK_ACCEPT, "%s\n", strerror(errno));
            continue;
        }
        
        struct dc_host *host = dc_host_alloc(dc, sock);
        if(likely(host)){
            task_spawn(host->desc, 0, NULL, agent_spawn, host);
            dc_hostlist_add_locked(dc->hostlist, host);
        }
    }
finish:
    return NULL;
}

extern int broadcast_to_decoders(char *p, ssize_t s);
static struct decoder clst4decoder = {
    .type = DT_INVALID,
    .listen = 0,
    .max_slaves = 0,
    .tm_kalive = -1,
    .tm_link_detection = -1,
    .kalive_interval = 0,
    .link_detection_interval = 0,
    .sock_routine = do_connect_to,
    .broadcast_to_probes = broadcast_to_probes,
    .broadcast_to_decoders = broadcast_to_decoders,
    .hostlist = NULL,
    .decoder_add = decoder_cluster_add,
    .decoder_del = decoder_cluster_del,

    .talkto = dc_talkto,
    .hearfrom = dc_hearfrom,
    .shutdown = dc_host_shutdown,
};

static struct rt_task_t dc_slave_task =
{
    .module = THIS,
    .name = "Slave Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &clst4decoder,
    .routine = accept_task,  /** Front end system request task */
};

static struct rt_task_t dc_master_task =
{
    .module = THIS,
    .name = "Master Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &clst4decoder,
    .routine = master_fn,  /** Front end system request task */
};

struct decoder *decoder_get_local()
{
    return &clst4decoder;
}

int broadcast_to_decoders(char *p, ssize_t s)
{
    struct dc_hostlist *dc = NULL;
    struct dc_host *h = NULL;
    int xret = 0;

    dc = clst4decoder.hostlist;
    if (!dc){
        rt_log_info("Decoder cluster initialization failure");
        return -1;
    }

    dc_lock(dc);
    hostlist_foreach_host(dc, h){
        if (h->valid &&
            h->sock > 0){
            xret = clst4decoder.talkto(h, p, s);
            if (xret < 0){
                rt_log_error(ERRNO_SOCK_SEND, "%s", strerror(errno));
            }
        }
    }
    dc_unlock(dc);

    return xret;
}

int decoder_cluster_del(const char *host, const char *port)
{
    return dc_del_host(&clst4decoder, host, port);
}

int decoder_cluster_add(const char *host, const char *port)
{
    return dc_add_host(&clst4decoder, host, port);
}

static void
decoder_startup(struct decoder *dc)
{
    if (dc->type == DT_MASTER){
        dc->sock_routine = do_connect_to;
        task_spawn_quickly(&dc_master_task);
        goto finish;
    }

    if (dc->type == DT_SLAVE){
        dc->sock_routine = do_connect_from;
        task_spawn_quickly(&dc_slave_task);
        goto finish;
    }

finish:
    return;
}

void decoder_cluster_init()
{
    probe_cluster_init();

    clst4decoder.hostlist = dc_hostlist_init();
    decoder_cluster_config(&clst4decoder);
    decoder_startup(&clst4decoder);
}

