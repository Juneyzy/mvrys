#include "sysdefs.h"
#include "cluster_probe.h"
#include "service.h"
#include "lwrpc.h"
#include "rpc_probe.h"

/*back stat*/
static void cpb_cluster_backupCntmsg(probe_stat_st *stat_bak, probe_stat_st stat)
{
    stat_bak->recv_pkts  = stat.recv_pkts;
    stat_bak->send_pkts  = stat.send_pkts;
    stat_bak->exception.lost_pkts = stat.exception.lost_pkts;
    stat_bak->exception.drop_pkts = stat.exception.drop_pkts;
    stat_bak->exception.drop_icmp_pkts     = stat.exception.drop_icmp_pkts;
    stat_bak->exception.drop_ttlzero_pkts  = stat.exception.drop_ttlzero_pkts;
    stat_bak->exception.drop_layer2_pkts   = stat.exception.drop_layer2_pkts;
    stat_bak->exception.drop_error_pkts    = stat.exception.drop_error_pkts;
    stat_bak->exception.ingress_lr_Pkts      = stat.exception.ingress_lr_Pkts;
    stat_bak->exception.ingress_nln_pkts     = stat.exception.ingress_nln_pkts;
    stat_bak->exception.ingress_lr_req_pkts  = stat.exception.ingress_lr_req_pkts;
    stat_bak->exception.ingress_lr_ctrl_pkts = stat.exception.ingress_lr_ctrl_pkts;
    stat_bak->ethernet_II.ingress_layer2_pkts  = stat.ethernet_II.ingress_layer2_pkts;
    stat_bak->ethernet_II.ingress_arp_pkts     = stat.ethernet_II.ingress_arp_pkts;
    stat_bak->ethernet_II.osi_layer3.ingress_icmp_pkts    = stat.ethernet_II.osi_layer3.ingress_icmp_pkts;
    stat_bak->ethernet_II.osi_layer3.ingress_igmp_pkts    = stat.ethernet_II.osi_layer3.ingress_igmp_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts  = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts     = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts     = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts    = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts    = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts    = stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts;
    stat_bak->exception.egress_lr_Pkts       = stat.exception.egress_lr_Pkts;
    stat_bak->exception.egress_nln_pkts      = stat.exception.egress_nln_pkts;
    stat_bak->exception.egress_lr_req_pkts   = stat.exception.egress_lr_req_pkts;
    stat_bak->exception.egress_lr_ctrl_pkts  = stat.exception.egress_lr_ctrl_pkts;
    stat_bak->ethernet_II.egress_layer2_pkts   = stat.ethernet_II.egress_layer2_pkts;
    stat_bak->ethernet_II.egress_arp_pkts      = stat.ethernet_II.egress_arp_pkts;
    stat_bak->ethernet_II.osi_layer3.egress_icmp_pkts     = stat.ethernet_II.osi_layer3.egress_icmp_pkts;
    stat_bak->ethernet_II.osi_layer3.egress_igmp_pkts     = stat.ethernet_II.osi_layer3.egress_igmp_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts   = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts      = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts      = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts     = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts     = stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts;
    stat_bak->ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts     = stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts;

    return;
}

/*parse and store stat msg*/
static inline void cpb_cluster_parseCntmsg(unsigned char *p, uint64_t *ret, struct pc_host *pc_host, int offset2)
{
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[1]);
    pc_host->stat.recv_pkts = ret[1];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[2]);
    pc_host->stat.send_pkts = ret[2];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[3]);
    pc_host->stat.exception.lost_pkts = ret[3];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[4]);
    pc_host->stat.exception.drop_pkts = ret[4];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[5]);
    pc_host->stat.exception.drop_icmp_pkts = ret[5];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[6]);
    pc_host->stat.exception.drop_ttlzero_pkts = ret[6];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[7]);
    pc_host->stat.exception.drop_layer2_pkts = ret[7];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[8]);
    pc_host->stat.exception.drop_error_pkts = ret[8];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[9]);
    pc_host->stat.exception.ingress_lr_Pkts = ret[9];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[10]);
    pc_host->stat.exception.ingress_nln_pkts = ret[10];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[11]);
    pc_host->stat.exception.ingress_lr_req_pkts = ret[11];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[12]);
    pc_host->stat.exception.ingress_lr_ctrl_pkts = ret[12];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[13]);
    pc_host->stat.ethernet_II.ingress_layer2_pkts = ret[13];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[14]);
    pc_host->stat.ethernet_II.ingress_arp_pkts = ret[14];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[15]);
    pc_host->stat.ethernet_II.osi_layer3.ingress_icmp_pkts = ret[15];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[16]);
    pc_host->stat.ethernet_II.osi_layer3.ingress_igmp_pkts = ret[16];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[17]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts = ret[17];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[18]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts = ret[18];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[19]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts = ret[19];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[20]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts = ret[20];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[21]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts = ret[21];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[22]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts = ret[22];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[23]);
    pc_host->stat.exception.egress_lr_Pkts = ret[23];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[24]);
    pc_host->stat.exception.egress_nln_pkts= ret[24];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[25]);
    pc_host->stat.exception.egress_lr_req_pkts = ret[25];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[26]);
    pc_host->stat.exception.egress_lr_ctrl_pkts = ret[26];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[27]);
    pc_host->stat.ethernet_II.egress_layer2_pkts = ret[27];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[28]);
    pc_host->stat.ethernet_II.egress_arp_pkts = ret[28];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[29]);
    pc_host->stat.ethernet_II.osi_layer3.egress_icmp_pkts = ret[29];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[30]);
    pc_host->stat.ethernet_II.osi_layer3.egress_igmp_pkts = ret[30];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[31]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts = ret[31];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[32]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts = ret[32];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[33]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts = ret[33];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[34]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts = ret[34];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[35]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts = ret[35];
    rpc_parse_param(p, &offset2, (unsigned char *) &ret[36]);
    pc_host->stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts = ret[36];

    return;
}

/** process stat msg*/
static int cpb_cluster_ProcCntmsg(unsigned char *response, int offset2, struct pc_host *pc_host)
{
    uint64_t ret[37] = {0};
    unsigned char *p = NULL;

    p = response;
    rpc_parse_param(response, &offset2, (unsigned char *) &ret[0]);

    if ((int)ret[0] == 0)
    {
        cpb_cluster_parseCntmsg(p, &ret[0], pc_host, offset2);
        cpb_cluster_backupCntmsg(&pc_host->stat_bak, pc_host->stat);
    }

    return 0;
}


/** process HostInfo msg */
static inline int cpb_cluster_ProcHostInfo(unsigned char *response, int offset2, struct pc_host *pc_host)
{
    unsigned char *p = NULL;
    uint64_t ret[6] = {0};
    uint64_t cpuUsageInt, cpuUsageFlt;
    uint64_t memTotal, memUsageInt,memUsageFlt;

    p = response;
    rpc_parse_param(response, &offset2, (unsigned char *) &ret[0]);

    if ((int)ret[0] == 0)
    {
        rpc_parse_param(p, &offset2, (unsigned char *) &cpuUsageInt);
        rpc_parse_param(p, &offset2, (unsigned char *) &cpuUsageFlt);
        rpc_parse_param(p, &offset2, (unsigned char *) &memTotal);
        rpc_parse_param(p, &offset2, (unsigned char *) &memUsageInt);
        rpc_parse_param(p, &offset2, (unsigned char *) &memUsageFlt);

        pc_host->host.memTotal = memTotal;
        pc_host->host.memUsage = (float)memUsageInt + (float)memUsageFlt / 1000000;
        pc_host->host.cpuUsage = (float)cpuUsageInt + (float)cpuUsageFlt / 1000000;
    }

    return 0;
}


static int
cpb_cluster_ProcRspMsg(IN int sock,
                       IN char *buffer,
                       IN ssize_t size,
                       IN int __attribute__((__unused__)) argc ,
                       IN void **argv,
                       OUT void __attribute__((__unused__)) *output_params)
{
    int offset2 = sizeof(RPC_HEAD_CLNT_ST);
    unsigned char *response = NULL;
    int rspLen = 0;
    struct sockaddr_in sock_addr;
    int rpc_id = 0;
    struct pc_host *pc_host;

    rpc_id = *((int *)argv[0]);
    pc_host = (struct pc_host *)argv[1];
    response = (unsigned char *)buffer;
    rspLen = size;

    if (pc_host == NULL || response == NULL)
    {
        goto msg_end;
    }

    if (size == 0){
        rt_log_warning(ERRNO_PROBE_PEER_CLOSE, "Disconnected with (%s:%d, sock=%d), local shutdown\n",
                      inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);
        goto msg_end;
    }
    if (size < 0){
        rt_log_warning(ERRNO_PROBE_PEER_ERROR, "Peer (%s:%d, sock=%d), local shutdown\n",
                      inet_ntoa(sock_addr.sin_addr), ntohs(sock_addr.sin_port), sock);
        goto msg_end;
    }

    if  (((uint32_t)size) <= sizeof(offset2)){
        rt_log_error(ERRNO_RPC_SIZE,"Invalid stat response(porbe: %d).\n", sock);
        goto msg_end;
    }
    RPCClntResponseDebug(response, rpc_id, rspLen);

    RPC_HEAD_ST *pHead = (RPC_HEAD_ST *)response;
    if (HTONS(pHead->rpc_id) == rpc_id && rpc_id == CpssGetStatistics_RPC_ID){
        cpb_cluster_ProcCntmsg(response, offset2, pc_host);
    }
    if (HTONS(pHead->rpc_id) == rpc_id && rpc_id == CpssGetHostInfo_RPC_ID){
        cpb_cluster_ProcHostInfo(response, offset2, pc_host);
    }

msg_end:

    return 0;
}

int cpb_cluster_RequestCntmsg(struct pc_host *pc_host)
{
    unsigned char request[RPC_READ_BUF] = {0};
    unsigned int pkt_len = sizeof(RPC_HEAD_CLNT_ST);
    unsigned int rpc_id = CpssGetStatistics_RPC_ID;
    int iResult = 0;
    unsigned int param_num = 0;
    int send_len = 0;
    uint8_t uiRouteID = 0, uiRport = 0;
    int offset = sizeof(RPC_HEAD_CLNT_ST);
    char msg[RPC_READ_BUF] = {0};
    int probe_sock = 0;

    /* Build params 1*/
    RPC_GENERATE_PARAM(TypeServU8, 5, uiRouteID);
    /* Build params 2*/
    RPC_GENERATE_PARAM(TypeServU8, 5, uiRport);
     /* Build head*/
    rpc_generate_request_head(request, pkt_len, rpc_id, param_num);

    probe_sock = pc_host->sock;
    if (probe_sock < 0)
    {
        iResult = -1;
        return iResult;
    }

    /*write request*/
    send_len = rt_sock_send(probe_sock, request, pkt_len);
    if (send_len < 0){
        iResult = -1;
        return iResult;
    }
    RPCClntRequestDebug(request, rpc_id, pkt_len);\

    /*recv cntmsg response*/
    char* argv[2] = {0};
    argv[0] = (char *)(int *)&rpc_id;
    argv[1] = (char *)pc_host;
    iResult = rt_sock_recv_timedout(__func__,
              probe_sock, msg, 2048, 1000000, cpb_cluster_ProcRspMsg, NULL, 2, (void **)argv, NULL);
    if (iResult == SOCK_CLOSE){
        iResult = -1;
    }

    return iResult;
}

/*   E T H E R N E T   P O R T   G E T   S T A T I S T I C S   */
/*-------------------------------------------------------------------------
    获取以太网端口的统计信息
-------------------------------------------------------------------------*/
int CpssReqEthernetGetStatistics(/*int RecvID, int PhyPortID, uint64_t *InBytes, uint64_t *OutBytes, uint64_t *InPkts, uint64_t *OutPkts,
                                 uint64_t *InBytesSpeed, uint64_t *OutBytesSpeed, uint64_t *InPktsSpeed, uint64_t *OutPktsSpeed*/)
{
    /* send request variables */
    int iResult = -1;
    struct pc_host *pc_host = NULL;
    struct pc_hostlist *host_list = NULL;

    host_list = local_probe_cluster();
    if (!host_list){
        return -1;
    }
    probelist_lock(host_list);
    probe_cluster_foreach_host(host_list, pc_host)
    {
        if (pc_host->valid &&
            pc_host->sock > 0){
            iResult = cpb_cluster_RequestCntmsg(pc_host);
            if (iResult < 0){
                rt_log_info("Disconnected with (%s:%d, sock=%d)\n",sockunion_su2str(&pc_host->su), pc_host->port, pc_host->sock);
            }
        }
    }
    probelist_unlock(host_list);
    if (iResult != 0){
        return -1;
    }

    return 0;
}
static int cpb_cluster_RequestHostInfo(unsigned int rpc_id, unsigned char *request,
                                       unsigned int pkt_len, struct pc_host * pc_host)
{
    int probe_sock = 0;
    int send_len = 0;
    char msg[RPC_READ_BUF] = {0};
    int iResult = 0;

    probe_sock = pc_host->sock;
    if (probe_sock < 0){
        iResult = -1;
        return iResult;
    }
     /*write request*/
    send_len = rt_sock_send(probe_sock, request, pkt_len);
    if (send_len < 0){
        iResult = -1;
        return iResult;
    }
    RPCClntRequestDebug(request, rpc_id, pkt_len);

    /*recv response*/
    char* argv[2] = {0};
    argv[0] = (char *)(int *)&rpc_id;
    argv[1] = (char *)pc_host;
    iResult = rt_sock_recv_timedout(__func__,
                  probe_sock, msg, 2048, 1000000, cpb_cluster_ProcRspMsg, NULL, 1, (void **)argv, NULL);
     if (iResult == SOCK_CLOSE){
        iResult = -1;
    }
    return iResult;
}

/* get probe host info */
int CpssReqGetHostInfo(void)
{
    /* send request variables */
    unsigned char request[RPC_READ_BUF] = {0};
    unsigned int pkt_len = sizeof(RPC_HEAD_CLNT_ST);
    unsigned int rpc_id = CpssGetHostInfo_RPC_ID;
    unsigned int param_num = 0;
    /* recv response variables */
    int offset = sizeof(RPC_HEAD_CLNT_ST);
    int RecvID = 1;
    int PhyPortID = 2;
    struct pc_host *pc_host = NULL;
    struct pc_hostlist *host_list = NULL;
    int iResult = -1;

    /* Build params 1*/
    RPC_GENERATE_PARAM(TypeServI32, 8, RecvID);
    /* Build params 2*/
    RPC_GENERATE_PARAM(TypeServI32, 8, PhyPortID);
    /* Build head*/
    rpc_generate_request_head(request, pkt_len, rpc_id, param_num);

    host_list = local_probe_cluster();
    if (!host_list){
        return -1;
    }

    probelist_lock(host_list);
    probe_cluster_foreach_host(host_list, pc_host)
    {
        if (pc_host->valid &&
            pc_host->sock > 0){
            iResult = cpb_cluster_RequestHostInfo(rpc_id, request, pkt_len, pc_host);
            if (iResult < 0){
                rt_log_info("Disconnected with (%s:%d, sock=%d)\n",sockunion_su2str(&pc_host->su), pc_host->port, pc_host->sock);
            }
        }
    }
    probelist_unlock(host_list);
    if (iResult != 0){
        return -1;
    }

    return 0;
}
