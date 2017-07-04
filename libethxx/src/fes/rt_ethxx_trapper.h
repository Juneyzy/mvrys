#ifndef __RT_ETHXX_TRAPPER_H__
#define __RT_ETHXX_TRAPPER_H__

#include "rt_pool.h"
#include "rt_list.h"

#define	MAX_WQES	4

struct rt_ethxx_trapper{
#define NETDEV_SIZE 16

	void                *bucket;

	int                 (*packet_parser)(void *snapshot, uint8_t *pkt, size_t pkt_len, void *argument, size_t as);
	pcap_t              *p;
	char                netdev[NETDEV_SIZE];
	char			filter;

	uint8_t             mac[6];
	uint32_t            ipv4;
	uint32_t            mask4;
	int                     recycle;
	int                     perf_interval;

	int                     p_memp_prealloc_size;
	int                     r_memp_prealloc_size;
	int                     dispatch_size;
	int			snap_len;
	atomic64_t         rank;

	char 		warehouse[128];

	/** always the same with logging dirent */
	char                    perf_view_domain [128];

	struct rt_pool_t        *bucketpool;

	uint32_t    perform_tmr;

	uint32_t    ethxx_detect;

	int32_t    link;

	int (*flush)(const char *fname, void *pkthdr,
	                    void *val, size_t __attribute__((__unused__))s);

	void (*display_ops)(const void *pkthdr,
	            void *intro,
	            const u_char *packet,
	            int64_t rank);

	/** threads in thread pool */
	int     thrds;
	/** work queue */
	int     wqe_size;

	struct thpool_ *thrdpool;

	struct session_t{
#define MAX_HSIZE   1024
	    struct {
	        struct hlist_head hash_bucket[MAX_HSIZE];
	    }_hash;
	    struct list_head    *head;
	    uint32_t    (*hash_routine)(void *tuple);
	}session;

	atomic64_t	circulating_factor;		/** used for round robin */
	struct list_head	*wqe;				/** hlist */
	rt_mutex			*wlock;				//
	uint64_t			*wcnt;

	int flags;

};

extern void  *rt_ethxx_open(const char *interface);

extern int rt_ethxx_init(struct rt_ethxx_trapper *rte,
                        const char *dev,
                        int up_check);

extern void rt_ethxx_deinit(struct rt_ethxx_trapper *rte);

#endif

