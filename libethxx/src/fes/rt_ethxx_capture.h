
#ifndef __RT_ETHXX_CAPTURE_H__
#define __RT_ETHXX_CAPTURE_H__

extern void start_capture();
extern void stop_capture();
extern void rt_ethernet_init();
extern void rt_ethernet_uninit();

extern int rt_ethxx_proc(void __attribute__((__unused__))*param0,
               void __attribute__((__unused__))*wqex,
               void(*routine)(void *_p, void *resv), void *argument);

extern void rte_netdev_config (struct rt_ethxx_trapper *rte, const char *netdev);
extern int rte_netdev_open (struct rt_ethxx_trapper *rte);
extern void rte_filter_config (struct rt_ethxx_trapper *rte, const char *filter);
extern void rte_netdev_perf_config (struct rt_ethxx_trapper *rte, const char *perf_view_domain);
extern void rte_pktopts_config (struct rt_ethxx_trapper *rte, const char *flags);
extern struct rt_ethxx_trapper *rte_default_trapper ();
extern int rte_open (struct rt_ethxx_trapper *rte);
extern void rte_preview (struct rt_ethxx_trapper *rte);

#endif

