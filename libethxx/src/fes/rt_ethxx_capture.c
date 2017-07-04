#include "sysdefs.h"
#include "conf.h"
#include "rt_ethxx_trapper.h"

#include "rt_ethxx_packet.h"
#include "rt_ethxx_reporter.h"
#include "rt_ethxx_parser.h"

#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
#include "vrs_session.h"
#endif

extern void rt_ethxx_reporter_init(int buckets);

extern void report_lineup(const char __attribute__((__unused__))*path,
                        size_t __attribute__((__unused__))ps,
                        const char *file,
                        size_t fs);

static LIST_HEAD(packet_list);

#if SPASR_BRANCH_EQUAL(BRANCH_A29)
static atomic_t rt_ethxx_capture_signal = ATOMIC_INIT(0);
#else
static atomic_t rt_ethxx_capture_signal = ATOMIC_INIT(1);
#endif

void start_capture()
{
    atomic_set(&rt_ethxx_capture_signal, ENABLE);
}

void stop_capture()
{
    atomic_set(&rt_ethxx_capture_signal, DISABLE);
}

#define capture_kmalloc(s)  kmalloc(s, MPF_CLR, -1)

#define FQLOCK_INIT(q) rt_mutex_init(&(q)->m, NULL)
#define FQLOCK_DESTROY(q) rt_mutex_destroy(&(q)->m)
#define FQLOCK_LOCK(q) rt_mutex_lock(&(q)->m)
#define FQLOCK_TRYLOCK(q) rt_mutex_trylock(&(q)->m)
#define FQLOCK_UNLOCK(q) rt_mutex_unlock(&(q)->m)

#define PACKET_INITIALIZE(p) do { \
            (p)->buffer = NULL;\
            (p)->buffer_size = 0;\
            (p)->ready = 0;\
         } while (0)


#define A_UNDO (0)
#define A_WRONLY    (1 << 0)                            /** write to disk only */
#define A_RDONLY    (1 << 1)                            /** display packet only  */
#define A_RDWR      (A_WRONLY | A_RDONLY)   /** display and write */

static void rt_ethxx_packet_display(const void *phdr,
                        void *intr,
                        const u_char *packet,
                        int64_t rank)
{
    int i = 0;
    struct pcap_pkthdr *pkthdr = (struct pcap_pkthdr *)phdr;
    struct rt_packet_snapshot_t *intro = (struct rt_packet_snapshot_t *)intr;

    printf("id: %ld\n", rank);
    printf("Packet length: %d\n", pkthdr->len);
    printf("Number of bytes: %d\n", pkthdr->caplen);
    printf("Recieved time: %s", ctime((const time_t *)&pkthdr->ts.tv_sec));
    if(intro){
        printf("type: %d\n", intro->type);
        printf("timestamp: %lu, %lu\n", intro->timestamp, pkthdr->ts.tv_sec);
    }

    for(i=0; i < (int)pkthdr->len; ++i){
        printf(" %02x", packet[i]);
        if( (i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");
}

static struct rt_ethxx_trapper ethxx_trapper = {
    .p = NULL,
    .rank = ATOMIC_INIT(0),
    .netdev = "wlp3s0",
    .ipv4 = 0XFFFFFFFF,
    .mac = {0},
    .recycle = FORBIDDEN,
    .warehouse = "./warehouse",
    .perf_view_domain = "./logs",
    .snap_len = 4096,
    .thrds = 1,
    .wqe_size = 40960,
    .filter = FILTER_VRS_LOCAL,
    .perf_interval = 15,

    .p_memp_prealloc_size = 409600,
    .r_memp_prealloc_size = 4096,
    .dispatch_size = 10240,
    .packet_parser = &rt_ethxx_packet_parser,

    .flags = A_UNDO,
    .display_ops = &rt_ethxx_packet_display,
    .flush = &rt_ethxx_pcap_flush,
};

struct rt_ethxx_trapper *rte_default_trapper ()
{
	return &ethxx_trapper;
}

static int
rte_wqe_init (struct rt_ethxx_trapper *rte,
			int max_wqes)
{
	int i;

	rte->wqe = (struct list_head *)kmalloc(sizeof(struct list_head) * max_wqes, MPF_CLR, -1);
	rte->wcnt = (uint64_t *)kmalloc(sizeof(uint64_t) * max_wqes, MPF_CLR, -1);
	rte->wlock = (rt_mutex *)kmalloc(sizeof(rt_mutex) * max_wqes, MPF_CLR, -1);

	for (i = 0; i < max_wqes; i ++) {
		INIT_LIST_HEAD (&rte->wqe[i]);
		rt_mutex_init (&rte->wlock[i], NULL);
		rte->wcnt[i] = 0;
	}

	return 0;
}


void rte_default_trapper_config (struct rt_ethxx_trapper *rte)
{
	if (likely (rte)) {
		memcpy (rte, rte_default_trapper(), sizeof (struct rt_ethxx_trapper));
	}
}


void rte_netdev_config (struct rt_ethxx_trapper *rte, const char *netdev)
{
	if (likely (netdev)) {
		memset (rte->netdev, 0, NETDEV_SIZE);
		memcpy (rte->netdev, netdev, strlen (netdev));
	}
}

int rte_netdev_open (struct rt_ethxx_trapper *rte)
{
	if (unlikely (!rte->netdev))
		return -1;

	kfree (rte->p);
	return rt_ethxx_init (rte, rte->netdev, 1);
}

void rte_netdev_perf_config (struct rt_ethxx_trapper *rte, const char *perf_view_domain)
{
	if (likely (perf_view_domain)) {
		memset (rte->perf_view_domain, 0, strlen (rte->perf_view_domain));
		memcpy (rte->perf_view_domain, perf_view_domain, strlen (perf_view_domain));
	}
}

void rte_filter_config (struct rt_ethxx_trapper *rte, const char *filter)
{
	rte->filter = FILTER_NORMAL;

	if (likely (filter) &&
		strcase_equal (filter, "vrs"))
		rte->filter = FILTER_VRS_LOCAL;
}

void rte_pktopts_config (struct rt_ethxx_trapper *rte, const char *flags)
{
	 rte->flags = A_UNDO;

	 if (unlikely (!flags))
	 	return;

	 if (strcase_equal(flags, "A_WRONLY")){
	        rte->flags |= A_WRONLY;
	    }

	 if (strcase_equal(flags, "A_RDONLY")){
	        rte->flags |= A_RDONLY;
	    }

	 if (strcase_equal(flags, "A_RDWR")){
	        rte->flags |= A_RDWR;
	    }
}


static __rt_always_inline__  int
rte_config(struct rt_ethxx_trapper *rte)
{
    int xret = 0;
    ConfNode *this_node = NULL;

    if (unlikely(!rte)){
        goto finish;
    }

    ConfNode *base = ConfGetNode("capture");
    if (!base){
        rt_log_error(ERRNO_YAML,
            "Can not get logging configuration");
        goto finish;
    }

    TAILQ_FOREACH(this_node, &base->head, next){

        if(!STRCMP(this_node->name, "netdev")){
		rte_netdev_config (rte, this_node->val);
        }

        if(!STRCMP(this_node->name, "filter")){
	    	rte_filter_config (rte, this_node->val);
        }

        if(!STRCMP(this_node->name, "packet-caches")){
	      rte->p_memp_prealloc_size = integer_parser(this_node->val, 0, 65535);
        }

        if(!STRCMP(this_node->name, "packet-cache-size")){
            rte->snap_len = integer_parser(this_node->val, 0, 65535);
        }

        if (!STRCMP(this_node->name, "packet-option")) {
            rte_pktopts_config (rte, this_node->val);

        }

        if(!STRCMP(this_node->name, "warehouse")) {
            if (rte->flags & A_WRONLY){
                if (!this_node->val){
                    rt_log_error(ERRNO_INVALID_ARGU,
                        "no warehouse");
                    xret = -1;
                    goto finish;
                }
		  memcpy (rte->warehouse, this_node->val, strlen (this_node->val));
            }
        }
    }

    base = ConfGetNode("logging");
    if (!base){
        rt_log_error(ERRNO_FATAL, "Can not get logging configuration");
        goto finish;
    }

    TAILQ_FOREACH(this_node, &base->head, next){
        if(!STRCMP(this_node->name, "default-log-dir")){
	     rte_netdev_perf_config (rte, this_node->val);
        }
     }

finish:
    return xret;
}

static __rt_always_inline__ int rte_check_and_mkdir(const char *root)
{
	char cmd[256] = {0};
	if(!rt_dir_exsit(root)) {
		sprintf(cmd, "mkdir -p %s", root);
		do_system(cmd);

		snprintf (cmd, 128,  "chmod 775 %s", root);
        	do_system(cmd);
		return 1;
	}

	return 0;
}

static void rte_link_detected(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char **argv)
{
    struct rt_ethxx_trapper *rte = (struct rt_ethxx_trapper *)argv;
    pcap_t *p = NULL;

    int up = nic_linkdetected(rte->netdev);
    if (up > 0){
            if(unlikely(!rte->p)){
                p = (pcap_t *)rt_ethxx_open(rte->netdev);
                if (likely(p)){
                    rt_log_info(
                                "%s link up",  rte->netdev);
                    rte->p = p;
                }
            }
    }else{
            if (up == 0)
                rt_log_info(
                            "%s link down",  rte->netdev);
            else
                rt_log_info(
                            "%s link error",  rte->netdev);
    }

    return;
}


static __rt_always_inline__ void
copy_to_bucket(struct rt_packet_t *p,
                        const u_char *packet,
                        const struct pcap_pkthdr *pkthdr)
{
    if(p->buffer){
        memset64(p->buffer, 0, p->buffer_size);
        memcpy64(p->buffer, packet, pkthdr->len);
        p->ready = 1;
        p->pkthdr.caplen = pkthdr->caplen;
        p->pkthdr.len = pkthdr->len;
        p->pkthdr.ts.tv_sec = pkthdr->ts.tv_sec;
        p->pkthdr.ts.tv_usec = pkthdr->ts.tv_usec;
    }
}

static __rt_always_inline__ void dispatch_lineup(struct rt_ethxx_trapper *rte,
				struct rt_pool_bucket_t *b, uint64_t callid)
{

	//int64_t	cur_circulating_factor = atomic64_inc (&rte->circulating_factor);
	int	hval	=	callid % MAX_WQES;

    atomic64_inc (&rte->circulating_factor);

	rt_mutex_lock (&rte->wlock[hval]);
	list_add_tail (&b->list, &rte->wqe[hval]);
	rt_mutex_unlock (&rte->wlock[hval]);
	rte->wcnt[hval] ++;


	DISPATCH_EQ_ADD(1);
}

static __rt_always_inline__ void mkfname(const char *warehouse,
                        void *intros,
                        void __attribute__((__unused__))*pf_hdr,
                        const void *phdr, int64_t rank,
                        char *filepath,
                        size_t path_size,
                        size_t *size)
{
    size_t s;
    int year, month, day, hour, min, sec;
    struct rt_packet_snapshot_t *intro = (struct rt_packet_snapshot_t *)intros;
    struct pcap_pkthdr *pkthdr = (struct pcap_pkthdr *)phdr;

    tmscanf_s (pkthdr->ts.tv_sec, &year, &month, &day, &hour, &min, &sec);

    if (intro->type == IPV4){
        s = SNPRINTF(filepath, path_size - 1, "%s/%d_%d_%d_0x%08X_0x%08X_0x%04X_0x%04X_0x%02X_%02d.%02d.%04d-%02d:%02d:%02d_%ld.pcap",
                 warehouse, intro->rid, intro->pid, intro->dir,
                 intro->ft.v4.src_ip, intro->ft.v4.dst_ip,
                 intro->ft.v4.src_port, intro->ft.v4.dst_port, intro->ft.v4.protocol,
                 month, day, year, hour, min, sec, rank);
    }else {
        if (intro->type == IPV6) {
            s = SNPRINTF(filepath, path_size - 1, "%s/%d_%d_%d_0x%016lX%016lX_0x%016lX%016lX_0x%04X_0x%04X_0x%02X_%02d.%02d.%04d-%02d:%02d:%02d_%ld.pcap",
                     warehouse, intro->rid, intro->pid, intro->dir,
                     intro->ft.v6.sip_upper, intro->ft.v6.sip_lower,
                     intro->ft.v6.dip_upper, intro->ft.v6.dip_lower,
                     intro->ft.v6.src_port, intro->ft.v6.dst_port,
                     intro->ft.v6.protocol,
                     month, day, year, hour, min, sec, rank);
        }else{
                s = SNPRINTF(filepath, path_size - 1, "%s/%d_%d_%d_%s_%02d.%02d.%04d-%02d:%02d:%02d_%ld.pcap",
                        warehouse, intro->rid, intro->pid, intro->dir,
                        "not_ip", month, day, year, hour, min, sec, rank);
        }
    }

    *size = s;
}

static __rt_always_inline__ void
packet_further_proc(struct rt_ethxx_trapper *rte,
    struct rt_packet_t *p)
{
    char filepath[512] = {0};
    size_t s = 0;

    if(likely(p)){

        if((rte->flags & A_RDONLY) &&
                    rte->display_ops){
            rte->display_ops(&p->pkthdr, &p->intro, (uint8_t *)p->buffer,
                                    atomic64_add(&rte->rank, 0));
        }

        if((rte->flags & A_WRONLY) &&
                    rte->flush && !(p->intro.flags && PR_DROPONLY)){

            mkfname(rte->warehouse, (void *)&p->intro, NULL,
                                &p->pkthdr,
                                random(), filepath, 512, &s);

            rte->flush(filepath, &p->pkthdr, (uint8_t *)p->buffer, p->pkthdr.len);

            report_lineup(NULL, 0, filepath, (int)s);

        }

    }
}


//#define THRDPOOL

static __rt_always_inline__ void
___thrdpool_feed(void *pv_val)
{

    struct rt_packet_t  *p;

    p = (struct rt_packet_t *)pv_val;
    if(likely(p)){
#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
        SGProc(p);
#else
	struct rt_ethxx_trapper *rte = rte_default_trapper();
	char filepath[512] = {0};
	size_t s = 0;

        mkfname(rte->warehouse, (void *)&p->intro, NULL,
                            &p->pkthdr,
                            random(), filepath, 512, &s);

        if((rte->flags & A_WRONLY) &&
                    rte->flush && !(p->intro.flags && PR_DROPONLY)){
            rte->flush(filepath, &p->pkthdr, (uint8_t *)p->buffer, p->pkthdr.len);
            report_lineup(NULL, 0, filepath, (int)s);
        }
#endif

    }
}

#define MACADDR_CMP(local, dst)   (*(uint32_t *)local == NTOHL(*(uint32_t *)dst) && \
                                                                    *(uint16_t *)(local + 4)== NTOHL(*(uint16_t *)dst + 4))

static __rt_always_inline__ int   local_frame(uint8_t *local, uint8_t *frame_dst)
{
	local = local;
	frame_dst = frame_dst;
 /*   return *(uint32_t *)local == *(uint32_t *)frame_dst; */

	return 1;
}

static __rt_always_inline__ int rte_filter (struct rt_ethxx_trapper *rte, const uint8_t __attribute__((__unused__))*p,
                                const struct pcap_pkthdr *pkthdr, int __attribute__((__unused__))flags)
{
   	int xret = -1;
	uint16_t	eth_type = 0, eth_type_pos = 12;
	uint8_t *current_ip_hdr;
	ipv4_header_st current_ipv4_hdr;

	if ((int)pkthdr->len > (rte->snap_len - 1)){
		rt_log_warning(ERRNO_PCAP,
		            "Jumbo Frame[%d]", pkthdr->caplen);
		goto finish;
	}

	eth_type = HTONS(* (uint16_t *)(p + eth_type_pos));

	if(rte->filter == FILTER_VRS_LOCAL) {
		/** Local VRS */
		if (eth_type == 0x8052) {
			if((int)pkthdr->len == 1069
				&& p[16] == 0x03 && local_frame((uint8_t *)&rte->mac[0], (uint8_t *)&p[0])){
				xret = 0;
				goto finish;
			}
		}
	}

	if (rte->filter == FILTER_VRS_YJ) {
		/** YJ VRS */
		if (eth_type == 0x0800) {
			/** Get IPV4 Header */
    			current_ip_hdr = (uint8_t *)(p + eth_type_pos + 2);
			xhdr_hton(ipv4, (ipv4_header_st *) current_ip_hdr, &current_ipv4_hdr, sizeof(ipv4_header_st));
			if (current_ipv4_hdr.protocol == 61) {
				xret = 0;
				goto finish;
			}
		}
	}

	if (rte->filter == FILTER_NORMAL) {
		xret = 0;
		goto finish;
	}

	/**
	if(rte->display_ops){
		rte->display_ops(pkthdr, NULL, (uint8_t *)p,
	            		atomic64_add(&rte->rank, 0));
	}
	*/

finish:
	return xret;
}
static void
rte_dispatcher(u_char __attribute__((__unused__))*argument,
    const struct pcap_pkthdr *pkthdr,
    const u_char *packet)
{
    struct rt_ethxx_trapper *rte = (struct rt_ethxx_trapper *)argument;
    struct rt_pool_bucket_t *_this = NULL;
    struct rt_packet_t  *p;
    int source_sys = rte->filter;

    if(rte_filter(rte, packet, pkthdr, 0) < 0)
        return;

    _this = rt_pool_bucket_get_new(rte->bucketpool, NULL);
    if (_this) {
        p = (struct rt_packet_t *)_this->priv_data;
        if(likely(p)) {
            copy_to_bucket(p, packet, pkthdr);

		if(rte->packet_parser)
			rte->packet_parser(&p->intro,
			                    (uint8_t *)p->buffer, p->pkthdr.len, (void *)&source_sys, sizeof(source_sys));
		/** */
		if((rte->flags & A_RDONLY) &&
		            rte->display_ops){
		    rte->display_ops(&p->pkthdr, &p->intro, (uint8_t *)p->buffer,
		                            atomic64_add(&rte->rank, 0));
		}
		if(p->intro.size == 60 &&
			rte->display_ops)
			rte->display_ops(&p->pkthdr, &p->intro, (uint8_t *)p->buffer,
		                            atomic64_add(&rte->rank, 0));

#ifndef THRDPOOL
            dispatch_lineup(rte, _this, p->intro.call_snapshot.call.data);
#else
            threadpool_add(rte->thrdpool, (void *)___thrdpool_feed, p, 0);
            rt_pool_bucket_push (rte->bucketpool, _this);
#endif
        }
    }
}

static void keep_silence()
{
    ;
}

static __rt_always_inline__ void
_further_proc(void *p,
    void(*routine)(void *_p, void *resv), void *argument)
{
    struct rt_packet_t *_this = (struct rt_packet_t *)p;
    (_this->ready && routine) ? routine (p, argument) : keep_silence();
}

int rt_ethxx_proc(void __attribute__((__unused__))*param0,
               void __attribute__((__unused__))*wqex /** WQE ID, ALL queues Round-Robin if wqex = NULL */,
               void(*routine)(void *_p, void *resv), void *argument)
{

	struct rt_ethxx_trapper *rte = rte_default_trapper();
	struct rt_pool_bucket_t *_this, *p;
	volatile int counter = 0;
	int	wqe = 0;

	if (unlikely (!wqex)) {
		for (wqe = 0; wqe < MAX_WQES; wqe ++) {

			rt_mutex_lock(&rte->wlock[wqe]);

			list_for_each_entry_safe (_this, p, &rte->wqe[wqe], list) {

				list_del(&_this->list);

				_further_proc(_this->priv_data, routine, argument);
				rt_pool_bucket_push (rte->bucketpool, _this);

				DISPATCH_DQ_ADD(1);
				counter++;
				rte->wcnt[wqe]--;
			}

			rt_mutex_unlock(&rte->wlock[wqe]);
		}
	}

	else {

		wqe = *(int *)wqex % MAX_WQES;
		rt_mutex_lock(&rte->wlock[wqe]);
		list_for_each_entry_safe (_this, p, &rte->wqe[wqe], list) {

			list_del(&_this->list);
			_further_proc(_this->priv_data, routine, argument);
			rt_pool_bucket_push (rte->bucketpool, _this);

			DISPATCH_DQ_ADD(1);
			counter++;
			rte->wcnt[wqe]--;
		}
		rt_mutex_unlock(&rte->wlock[wqe]);
	}

	return counter;
}

static void *
rt_ethxx_capture_routine(void *args)
{
    struct rt_ethxx_trapper *rte = (struct rt_ethxx_trapper *)args;
    static int64_t rank_acc;
    static int signal;

    signal = atomic_read(&rt_ethxx_capture_signal);
    FOREVER {

        if(unlikely(signal != atomic_add(&rt_ethxx_capture_signal, 0))){
            rt_log_warning(ERRNO_PCAP, "Capture %s", atomic_read(&rt_ethxx_capture_signal) ? "Started" : "Stopped");
            signal = atomic_read(&rt_ethxx_capture_signal);
        }

        if(signal &&
                likely(rte->p)){
            rank_acc = pcap_dispatch(rte->p,
                                    rte->dispatch_size,
                                    rte_dispatcher,
                                    (u_char *)rte);
            if(rank_acc >= 0){
                atomic64_add(&rte->rank, rank_acc);
            }else{
                pcap_close(rte->p);
                printf("rte->p=%p\n", rte->p);
                rte->p = NULL;

                if(rank_acc == -1 /** error occurs */){

                }

                if(rank_acc == -2 /** breakloop called */){
                    ;
                }
            }
        }
    }

    task_deregistry_id(pthread_self());

    return NULL;
}


#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)

static void
__rt_ethxx_proc(void *_p,
    void __attribute__((__unused__)) *resv)
{

    struct rt_packet_t  *p;

    p = (struct rt_packet_t *)_p;
    if(likely(p)){
#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
        SGProc(p);
#else
	struct rt_ethxx_trapper *rte = rte_default_trapper ();
	char filepath[512] = {0};
	size_t s = 0;

        mkfname(rte->warehouse, (void *)&p->intro, NULL,
                            &p->pkthdr,
                            random(), filepath, 512, &s);

        if((rte->flags & A_WRONLY) &&
                    rte->flush && !(p->intro.flags && PR_DROPONLY)){
            rte->flush(filepath, &p->pkthdr, (uint8_t *)p->buffer, p->pkthdr.len);
            report_lineup(NULL, 0, filepath, (int)s);
        }
#endif

    }
}

static void *
rt_ethxx_proc_routine(void __attribute__((__unused__))*argvs)
{
   FOREVER{
       rt_ethxx_proc(argvs, NULL, __rt_ethxx_proc, NULL);
   }

   task_deregistry_id(pthread_self());
   return NULL;
}
static struct rt_task_t ethxxProc = {
    .module = THIS,
    .name = "The Ethxx Packet Proc Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ethxx_trapper,
    .routine = rt_ethxx_proc_routine,
    .recycle = FORBIDDEN,
};
#endif

static struct rt_task_t ethxxCaptor = {
    .module = THIS,
    .name = "The Ethxx Dispatch Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &ethxx_trapper,
    .routine = rt_ethxx_capture_routine,
    .recycle = FORBIDDEN,
};

static void *
packet_alloc()
{
    struct rt_packet_t *p;
    struct rt_ethxx_trapper *rte = rte_default_trapper();

    p = (struct rt_packet_t *)capture_kmalloc(sizeof(struct rt_packet_t));
    if (likely(p)) {
        PACKET_INITIALIZE(p);
        p->buffer = capture_kmalloc(rte->snap_len);
        if (unlikely(!p->buffer)) {
            free(p);
            p = NULL;
        }
        p->buffer_size = rte->snap_len;
    }
    return p;
}

static void
packet_free(void *priv_data)
{
    struct rt_packet_t *p = (struct rt_packet_t *)priv_data;

    if(likely(p)){
        if(likely(p->buffer))
            free(p->buffer);
        free(p);
    }
}

static void do_perform_monitor(uint32_t __attribute__((__unused__)) uid,
                  int __attribute__((__unused__))argc,
                  char **argv)
{
#define PERF_DUMP_SIZE  1023
#define PERF_GATHER_SIZE 512

    struct rt_ethxx_trapper *rte = (struct rt_ethxx_trapper *)argv;
    char packet_bucket_gather[PERF_GATHER_SIZE] = {0};
    char list_perf_dump[PERF_DUMP_SIZE] = {0};
    int l = 0;
    char tm[64] = {0}, tm_ymd[64] = {0};
    int64_t dispatcher_eq, dispatcher_dq, dispatcher_wr;
    int64_t reporter_eq, reporter_dq;

    //printf("inc = %d\n", inc);

    rte_check_and_mkdir (rte->warehouse);
    rte_check_and_mkdir (rte->perf_view_domain);

    rt_curr_tms2str(EVAL_TM_STYLE_FULL, tm, 63);
    l = 0;
    /** Dispatch perf */
    l += SNPRINTF(packet_bucket_gather + l, PERF_GATHER_SIZE - l,
                    "%s     Dispatches=%ld\n",
                    tm, atomic64_add(&rte->rank, 0));

    dispatcher_eq = DISPATCH_EQ_ADD(0);
    dispatcher_dq = DISPATCH_DQ_ADD(0);
    dispatcher_wr = DISPATCH_WR_ADD(0);

    reporter_eq = REPORTER_EQ_ADD(0);
    reporter_dq = REPORTER_DQ_ADD(0);

    l += SNPRINTF(packet_bucket_gather + l, PERF_GATHER_SIZE - l,
        "\tDispatcher enqueue=%ld, dequeue=%ld, flush=%ld, remain=%ld\n", dispatcher_eq, dispatcher_dq,
        dispatcher_wr, (dispatcher_eq - dispatcher_dq));

    l += SNPRINTF(packet_bucket_gather + l, PERF_GATHER_SIZE - l,
        "\tReporter   enqueue=%ld, dequeue=%ld, remain=%ld\n", reporter_eq, reporter_dq,
        (reporter_eq - reporter_dq));

    /** %Y-%m-%d, 2016-05-14 */
    memcpy(tm_ymd, tm, 10);
    SNPRINTF(list_perf_dump, PERF_DUMP_SIZE, "echo \"%s\" >> %s/dispatcher-reporter-WQEs-%s", packet_bucket_gather,
                                rte->perf_view_domain, tm_ymd);
    do_system(list_perf_dump);

}

void rte_preview (struct rt_ethxx_trapper *rte)
{
    char form_mac_str[64] = {0};
    char form_ip_str[64] = {0};
    char form_mask_str[64] = {0};
    char form_all[64] = {0};

    nic_address(rte->netdev,  form_ip_str, 63,
                                form_mask_str, 63, NULL, 6);

    snprintf(form_all, 63, "%s/%s", form_ip_str, form_mask_str);

    printf("\r\nCapture Ethxx Preview\n");
    printf("%30s:%60s\n", "The Ethernet Adapter", rte->netdev);
    printf("%30s:%60s\n", "The IpAddress", form_all);

    snprintf (form_mac_str, 63, "%02x:%02x:%02x:%02x:%02x:%02x", rte->mac[0],
                    rte->mac[1], rte->mac[2],  rte->mac[3], rte->mac[4], rte->mac[5]);

    printf("%30s:%60s\n", "The MacAddress", form_mac_str);
    printf("%30s:%60s\n", "The Warehouse", rte->warehouse);
    printf("%30s:%60d\n", "The Filter", rte->filter);
    printf("%30s:%60s\n", "The Performence View", rte->perf_view_domain);
    printf("%30s:%60d\n", "The Performence View Interval", rte->perf_interval);
    printf("%30s:%60d\n", "The Dispatches", rte->dispatch_size);
    printf("%30s:%60d\n", "The Snap Length", rte->snap_len);
    printf("%30s:%60s\n", "The Capture", atomic_read(&rt_ethxx_capture_signal) ? "Started" : "Stopped");
    printf("%30s:%60d\n", "The Capture Prealloc Buckets", rte->p_memp_prealloc_size);
    printf("%30s:%60d\n", "The Reporter Prealloc Buckets", rte->r_memp_prealloc_size);
    printf("%30s:%60d\n", "The Threadpool's Threads", rte->thrds);
    printf("%30s:%60d\n", "The Threadpool's WQE Size", rte->wqe_size);

    printf("\r\n\n");

}

int rte_open (struct rt_ethxx_trapper *rte)
{
	int xret = -1;

	rte_wqe_init (rte, MAX_WQES);
	rte_check_and_mkdir (rte->warehouse);
	rte_check_and_mkdir (rte->perf_view_domain);
	xret = rte_netdev_open (rte);

	if(likely(rte->p)) {

#if !SPASR_BRANCH_EQUAL(BRANCH_VRS)
	    rt_ethxx_reporter_init((rte->r_memp_prealloc_size > 0) ?
	                            rte->r_memp_prealloc_size : 4096);
#endif
	    rte->bucketpool = rt_pool_initialize((rte->p_memp_prealloc_size > 0) ?   \
	                            rte->p_memp_prealloc_size : 4096,   \
	                            packet_alloc, packet_free,    \
	                            0);
#ifdef THRDPOOL
	    assert((rte->thrdpool = thpool_create(rte->thrds, rte->wqe_size, 0)) != NULL);
	    fprintf(stderr, "Pool started with %d threads and "
	            "queue size of %d\n", rte->thrds, rte->wqe_size);
#endif

	    rte->perform_tmr = tmr_create(PcapCapture,
	                            "Performence Monitor for Captor", TMR_PERIODIC,
	                            do_perform_monitor, 1, (char **)rte, rte->perf_interval);

	    rte->ethxx_detect = tmr_create(PcapCapture,
	                            "Eth up status monitor", TMR_PERIODIC,
	                            rte_link_detected, 1, (char **)rte, 3);

#if !SPASR_BRANCH_EQUAL(BRANCH_A29)
	    atomic_set(&rt_ethxx_capture_signal, 1);
#endif
	    task_registry(&ethxxCaptor);

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
	    task_registry(&ethxxProc);
#endif

	    tmr_start(rte->perform_tmr);
	    tmr_start(rte->ethxx_detect);
	}
    return xret;
}

void rt_ethernet_init()
{
    struct rt_ethxx_trapper *rte = rte_default_trapper ();

    atomic_set(&rt_ethxx_capture_signal, 0);

    rte_config (rte);
    rte_open (rte);
    rte_preview(rte);
}


void rt_ethernet_uninit()
{
    struct rt_ethxx_trapper *rte = rte_default_trapper ();

    rt_ethxx_deinit(rte);

    thpool_destroy(rte->thrdpool, 0);
    rt_log_debug("threadpool destroy");
}

