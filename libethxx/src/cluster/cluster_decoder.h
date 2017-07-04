#ifndef __DECODER_CLUSTER_H__
#define __DECODER_CLUSTER_H__

enum dc_type{
    DT_INVALID,
    DT_MASTER,
    DT_SLAVE
};



struct decoder{
    /** master or slave mode */
    enum dc_type type;

    /** valid when dc_type=MASTER */
#ifdef RT_TMR_ADVANCED
    uint32_t tm_kalive;
#else
    int tm_kalive;
#endif
    /** valid when dc_type=MASTER */
#ifdef RT_TMR_ADVANCED
    uint32_t tm_link_detection;
#else
    int tm_link_detection;
#endif
    /** valid when dc_type=MASTER */
    int kalive_interval;

    /** valid when dc_type=MASTER */
    int link_detection_interval;

    /** valid when dc_type=MASTER */
    int max_slaves;

    /** valid when dc_type=MASTER */
    #if 0
    struct dc_hostlist *hostlist;
    void (*hostlist_add)(struct dc_hostlist *hl, struct dc_host *h);
    void (*hostlist_add_locked)(struct dc_hostlist *hl, struct dc_host *h);
    struct dc_host *(*hostlist_del)(struct dc_hostlist *hl, struct dc_host *h);
    struct dc_host *(*hostlist_del_locked)(struct dc_hostlist *hl, struct dc_host *h);
    #else
    void *hostlist;
    #if 0
    void (*hostlist_add)(void *hl, void *h);
    void (*hostlist_add_locked)(void *hl, void *h);
    void *(*hostlist_del)(void *hl, void *h);
    void *(*hostlist_del_locked)(void *hl, void *h);
    #else
    int (*decoder_add)(const char *ip, const char *port);
    int (*decoder_del)(const char *ip, const char *port);
    #endif
    #endif
    ssize_t (*talkto)(void *host, void *data, size_t s);
    ssize_t (*hearfrom)(void *host, void *buffer, size_t s);
    int (*broadcast_to_decoders)(char *buffer, ssize_t s);
    int (*broadcast_to_probes)(char *buffer, ssize_t s);
    
    /** valid when dc_type=MASTER && SLAVE,
    slave: rt_serv_sock;
    master: rt_clnt_sock;
    */
    int (*sock_routine)(void *host);

    /** listen port when dc_type=SLAVE */
    int listen;

    /** sock when dc_type=SLAVE */
    int sock;

     /** used both in SLAVE && MASTER mode*/
    void (*shutdown)(void *host);

};


extern void decoder_cluster_init();

extern struct decoder *decoder_get_local();

extern int decoder_cluster_del(const char *host, const char *port);

extern int decoder_cluster_add(const char *host, const char *port);


#endif
