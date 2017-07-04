#ifndef __RPC_PROBE_H__
#define __RPC_PROBE_H__

extern int CpssReqEthernetGetStatistics(void);
extern int CpssReqGetHostInfo(void);
extern int cpb_cluster_RequestCntmsg(struct pc_host *pc_host);

#endif