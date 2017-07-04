#include "sysdefs.h"
#include "probe.h"
#include "conf.h"
#include "capture.h"
#include "cluster_probe.h"

typedef struct
{
    uint8_t ver;
    uint8_t dir;
    uint16_t cmd;
    uint16_t seq;
    uint16_t siglen;
} ui_cmd_hdr_st;

typedef struct
{
    ui_cmd_hdr_st hdr;
    uint8_t *pkt;
} ui_pkt_info_st;

typedef struct
{
    int fd;
    char *ip;
    int port;
    void (*routine)(int, char*, int);             /* ui command process function */
}ui_mgmt_st;


#define UI_REQUEST  0
#define UI_RESPONSE 1

#define EXEC_SUCCESS    1
#define EXEC_FAILURE    0

#define RECEIVER_PROCESS_NAME   "Receiver"

#define UI_CMD_START            0x50
#define UI_CMD_STOP             0x51
#define UI_CMD_RESTART          0x52
#define UI_CMD_STATISTICS_GET   0X70

#define UI_CMDTYPE_PROCESS_NAME         0x0001
#define UI_CDMTYPE_EXEC_RESULT          0x1001
#define UI_CMDTYPE_FAIL_REASON          0x1003
#define UI_CMDTYPE_RECV_PKTS            0x2001
#define UI_CMDTYPE_SEND_PKTS            0x2002
#define UI_CMDTYPE_LOST_PKTS            0x2003
#define UI_CMDTYPE_DROP_PKTS_TOTAL      0x2004
#define UI_CMDTYPE_DROP_PKTS_ICMP       0x2005
#define UI_CMDTYPE_DROP_PKTS_TTLZERO    0x2006
#define UI_CMDTYPE_DROP_PKTS_LAYER2     0x2007
#define UI_CMDTYPE_DROP_PKTS_ERROR      0x2008

#define __ui_pkt_size__     1024

probe_stat_st probe_stat;

#define PROTOCOL_VERSION 1

static inline int fillUiRspPktHeader(uint8_t *header, uint16_t cmd, uint16_t seq, uint16_t siglen)
{
    ui_cmd_hdr_st *hdr = (ui_cmd_hdr_st *)header;
    hdr->ver = PROTOCOL_VERSION;
    hdr->dir = UI_RESPONSE;
    hdr->cmd = HTONS(cmd);
    hdr->seq = HTONS(seq);
    hdr->siglen = HTONS(siglen);
    return sizeof(ui_cmd_hdr_st);
}

static inline int fillPktString(uint8_t *ptr, uint16_t type, int len, uint8_t *data)
{
    *(uint16_t *)ptr = HTONS(type);
    *(uint16_t *)(ptr + 2) = HTONS(len + 1);
    STRNCPY(ptr + 4, data, len);
    *(ptr + 4 + len) = '\0';
    return len + 1 + 4;
}
static inline int fillPktInteger(uint8_t *ptr, uint16_t type, uint32_t data)
{
    *(uint16_t *)ptr = HTONS(type);
    *(uint16_t *)(ptr + 2) = HTONS(4);
    *(uint32_t *)(ptr + 4) = HTONL(data);
    return 8;
}
static inline int fillPktLLInteger(uint8_t *ptr, uint16_t type, uint64_t data)
{
    *(uint16_t *)ptr = HTONS(type);
    *(uint16_t *)(ptr + 2) = HTONS(8);
    *(uint64_t *)(ptr + 4) = HTONLL (data);
    return 12;
}
static inline int fillUiRspPktProcessName(uint8_t *ptr)
{
    return fillPktString(ptr, UI_CMDTYPE_PROCESS_NAME, STRLEN(RECEIVER_PROCESS_NAME) + 1, (uint8_t *)RECEIVER_PROCESS_NAME);
}
static inline int fillUiRspPktExecResult(uint8_t *ptr, uint16_t result)
{
    *(uint16_t *)ptr = HTONS(UI_CDMTYPE_EXEC_RESULT);
    *(uint16_t *)(ptr + 2) = HTONS(2);
    *(uint16_t *)(ptr + 4) = HTONS(result);
    return 6;
}
static inline int fillUiRspPktFailReason(uint8_t *ptr, uint8_t *reason, uint16_t len)
{
    return fillPktString(ptr, UI_CMDTYPE_FAIL_REASON, len, reason);
}

static inline int uiCmdStartProcess(int fd, ui_pkt_info_st *pkt_info)
{
    int siglen = 0;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};

#ifdef RT_ETHXX_CAPTURE_ADVANCED
    start_capture();
#else
    rt_captor_status_set (ENABLE);
#endif

    siglen += fillUiRspPktProcessName((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st));
    siglen += fillUiRspPktExecResult((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, EXEC_SUCCESS);
    siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info->hdr.cmd, pkt_info->hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

    if (rt_sock_send(fd, ua_rsp_pkt, siglen) <= 0){
        rt_log_error(ERRNO_UI_A29, "Send response to ui failure: cmd(%d), seq(%d) failure (%s)!",
                    pkt_info->hdr.cmd, pkt_info->hdr.seq, strerror(errno));
        return -1;
    }
    
    return 0;
}
static inline int uiCmdStopProcess(int fd, ui_pkt_info_st *pkt_info)
{
    int siglen = 0;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};

#ifdef RT_ETHXX_CAPTURE_ADVANCED
    stop_capture();
#else
    rt_captor_status_set (DISABLE);
#endif

    siglen += fillUiRspPktProcessName((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st));
    siglen += fillUiRspPktExecResult((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, EXEC_SUCCESS);
    siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info->hdr.cmd, pkt_info->hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

    if (rt_sock_send(fd, (uint8_t *)ua_rsp_pkt, siglen) <= 0){
        rt_log_error(ERRNO_UI_A29, "Send response to ui failure: cmd(%d), seq(%d) failure (%s)!",
            pkt_info->hdr.cmd, pkt_info->hdr.seq, strerror(errno));
        return -1;
    }
    
    return 0;
}
static inline int uiCmdRestartProcess(int fd, ui_pkt_info_st *pkt_info)
{
    int siglen = 0;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};

#ifdef RT_ETHXX_CAPTURE_ADVANCED
    start_capture();
#else
    rt_captor_status_set (ENABLE);
#endif

    siglen += fillUiRspPktProcessName((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st));
    siglen += fillUiRspPktExecResult((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, EXEC_SUCCESS);
    siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info->hdr.cmd, pkt_info->hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

    if (rt_sock_send(fd, (uint8_t *)ua_rsp_pkt, siglen) <= 0){
        rt_log_error(ERRNO_UI_A29, "Send response to ui failure: cmd(%d), seq(%d) failure (%s)!",
                pkt_info->hdr.cmd, pkt_info->hdr.seq, strerror(errno));
        return -1;
    }

    return 0;
}

static inline void
ui_statistics_update(probe_stat_st* probe_stat)
{
    struct pc_host *prob = NULL;
    struct pc_hostlist *probe_list = NULL;

    probe_stat->recv_pkts = 0;
    probe_stat->send_pkts = 0;
    probe_stat->exception.lost_pkts = 0;
    probe_stat->exception.drop_pkts = 0;
    probe_stat->exception.drop_icmp_pkts    = 0;
    probe_stat->exception.drop_ttlzero_pkts = 0;
    probe_stat->exception.drop_layer2_pkts  = 0;
    probe_stat->exception.drop_error_pkts   = 0;

    probe_list = local_probe_cluster();
    if (!probe_list){
        return;
    }

    probelist_lock(probe_list);
    probe_cluster_foreach_host(probe_list, prob) {
        probe_stat->recv_pkts += prob->stat_bak.recv_pkts;
        probe_stat->send_pkts += prob->stat_bak.send_pkts;
        probe_stat->exception.lost_pkts += prob->stat_bak.exception.lost_pkts;
        probe_stat->exception.drop_pkts += prob->stat_bak.exception.drop_pkts;
        probe_stat->exception.drop_icmp_pkts    += prob->stat_bak.exception.drop_icmp_pkts;
        probe_stat->exception.drop_ttlzero_pkts += prob->stat_bak.exception.drop_ttlzero_pkts;
        probe_stat->exception.drop_layer2_pkts  += prob->stat_bak.exception.drop_layer2_pkts;
        probe_stat->exception.drop_error_pkts   += prob->stat_bak.exception.drop_error_pkts;
    }
    probelist_unlock(probe_list);
}

static inline int uiCmdStatisticsGetProcess(int fd, ui_pkt_info_st *pkt_info)
{
    int siglen = 0;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};

    ui_statistics_update(&probe_stat);

    siglen += fillUiRspPktProcessName(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st));
    siglen += fillUiRspPktExecResult(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, EXEC_SUCCESS);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_RECV_PKTS, probe_stat.recv_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_SEND_PKTS, probe_stat.send_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_LOST_PKTS, probe_stat.exception.lost_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_DROP_PKTS_TOTAL, probe_stat.exception.drop_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_DROP_PKTS_ICMP, probe_stat.exception.drop_icmp_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_DROP_PKTS_TTLZERO, probe_stat.exception.drop_ttlzero_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_DROP_PKTS_LAYER2, probe_stat.exception.drop_layer2_pkts);
    siglen += fillPktLLInteger(&ua_rsp_pkt[0] + sizeof(ui_cmd_hdr_st) + siglen, UI_CMDTYPE_DROP_PKTS_ERROR, probe_stat.exception.drop_error_pkts);
    siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info->hdr.cmd, pkt_info->hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

    if (rt_sock_send(fd, &ua_rsp_pkt[0], siglen) <= 0){
        rt_log_error(ERRNO_UI_A29, "Send response to ui failure: cmd(%#x), seq(%d) failure(%s)!",
            pkt_info->hdr.cmd, pkt_info->hdr.seq, strerror(errno));
        return -1;
    }

    return 0;
}

/* On success, return 0; On error, return a nonzero value */
static inline int checkProcessName(uint8_t *ptr)
{
    uint8_t *p = ptr;
    int type = 0;
    int len = 0;
    uint8_t *process = NULL;

    while (p)
    {
        type = NTOHS(*(uint16_t *)p);
        len = NTOHS(*(uint16_t *)(p + 2));

        if (type == UI_CMDTYPE_PROCESS_NAME)
        {
            process = p + 4;
            return STRNCMP(RECEIVER_PROCESS_NAME, process, len);
        }

        p += (len + 4);
    }

    return -1;
}

static inline int uiCmdPktHdrParse(ui_pkt_info_st *pkt_info, uint8_t *packet)
{
    ui_cmd_hdr_st *hdr = (ui_cmd_hdr_st *)packet;

    if ((PROTOCOL_VERSION != hdr->ver) ||
        ((UI_REQUEST != hdr->dir) && (UI_RESPONSE != hdr->dir))) {
        rt_log_error(ERRNO_UI_A29, "Receiver an incorrect packet from UI");
        return -1;
    }
    
    memset (pkt_info, 0, sizeof (ui_cmd_hdr_st));
    pkt_info->hdr.ver = hdr->ver;
    pkt_info->hdr.dir = hdr->dir;
    pkt_info->hdr.cmd = NTOHS(hdr->cmd);
    pkt_info->hdr.seq = NTOHS(hdr->seq);
    pkt_info->hdr.siglen = NTOHS(hdr->siglen);
    
    return 0;
}

static inline int uiErrPktRsp(int fd, ui_pkt_info_st *pkt_info, char *reason)
{
    int siglen = 0;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};
    siglen += fillUiRspPktProcessName((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st));
    siglen += fillUiRspPktExecResult((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, EXEC_FAILURE);
    siglen += fillUiRspPktFailReason((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, (uint8_t *)reason, STRLEN(reason) + 1);
    siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info->hdr.cmd, pkt_info->hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

    if (rt_sock_send(fd, &ua_rsp_pkt[0], siglen) <= 0) {
        rt_log_error(ERRNO_UI_A29, "Send response to ui failure (cmd: %d)!", pkt_info->hdr.cmd);
        return -1;
    }
    
    return 0;
}
static inline int uiPacketProcess(int fd, uint8_t *packet)
{
    int siglen = 0;
    int ret = -1;
    ui_pkt_info_st pkt_info;
    uint8_t ua_rsp_pkt[__ui_pkt_size__] = {0};

    
    if (uiCmdPktHdrParse(&pkt_info, packet)) {
        return uiErrPktRsp (fd, &pkt_info, "Incorrect packet");
    }

    rt_log_debug("Message from UI (cmd: %#x, seq: %#x)!", pkt_info.hdr.cmd, pkt_info.hdr.seq);

    /* check process name */
    if (checkProcessName(packet + sizeof(ui_cmd_hdr_st))) {

        /* Unmatched process name */
        char *reason = "Unrecognized process name";
        siglen += fillUiRspPktProcessName((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st));
        siglen += fillUiRspPktExecResult((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, EXEC_FAILURE);
        siglen += fillUiRspPktFailReason((uint8_t *)ua_rsp_pkt + sizeof(ui_cmd_hdr_st) + siglen, (uint8_t *)reason, STRLEN(reason) + 1);
        siglen += fillUiRspPktHeader((uint8_t *)ua_rsp_pkt, pkt_info.hdr.cmd, pkt_info.hdr.seq, siglen + sizeof(ui_cmd_hdr_st));

        if (rt_sock_send(fd, &ua_rsp_pkt[0], siglen) <= 0) {
            rt_log_error(ERRNO_UI_A29, "Send response to ui failure (cmd: %d)!", pkt_info.hdr.cmd);
        }
        
        return 0;
    }

    /* process by command type */
    switch (pkt_info.hdr.cmd)
    {
        case UI_CMD_START:
            ret = uiCmdStartProcess(fd, &pkt_info);
            break;

        case UI_CMD_STOP :
            ret = uiCmdStopProcess(fd, &pkt_info);
            break;

        case UI_CMD_RESTART :
            ret = uiCmdRestartProcess(fd, &pkt_info);
            break;

        case UI_CMD_STATISTICS_GET :
            ret = uiCmdStatisticsGetProcess(fd, &pkt_info);
            break;

        default :
            ret = uiErrPktRsp (fd, &pkt_info, "Unrecognized command");
            break;
    }

    return ret;
}

void uiCmdcallback(int fd, char *req, int size)
{
    if (size > (int)sizeof(ui_cmd_hdr_st)){
        uiPacketProcess(fd, (uint8_t *)req);
    }
    else{
        rt_log_error(ERRNO_UI_A29, "Receiver an incorrect packet from UI");
        ui_pkt_info_st pkt_info;
        uiErrPktRsp (fd, &pkt_info, "Packet is too short");
    }
    return ;
}

static ui_mgmt_st pst_ui_mgmt = {
    .fd = -1,
    .ip = NULL,
    .port = 65535,
    .routine = &uiCmdcallback
};


void *ui_do_proc(void *args)
{
    args = args;
    int ret, done = 0;
    struct timeval timeout;
    fd_set f_set;
    char request[__ui_pkt_size__] = {0};
    ui_mgmt_st *ui = (ui_mgmt_st *)args;
    
    FOREVER {
        
        do {
            ui->fd = (rt_clnt_sock(0, ui->ip, ui->port, AF_INET));
            sleep (3);
        }while(ui->fd < 0);

        done = 0;
        do {
            FD_ZERO(&f_set);
            FD_SET(ui->fd, &f_set);
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            ret = select(ui->fd + 1, &f_set, NULL, NULL, &timeout);
            if (ret < 0 && !ERRNO_EQUAL(EINTR)){
                rt_log_error(ERRNO_UI_A29, "select: %s", strerror(errno));
                break;
            }
            else{
                if (ret > 0 &&
                    FD_ISSET(ui->fd, &f_set)){
                    int rsize = rt_sock_recv(ui->fd, request, __ui_pkt_size__);
                    if (rsize <= 0) {
                        rt_log_warning(ERRNO_UI_A29, "rsize=%d, sock=%d, %s.", rsize, ui->fd, 
                            (rsize == 0) ? "Connection closed" : strerror (errno));
                        break;
                    }
                    ui->routine(ui->fd, request, rsize);
                }
            }
        } while (!done);

        close(ui->fd);
        ui->fd = (-1);
    }

    task_deregistry_id(pthread_self());
    
    return NULL;
}


static struct rt_task_t serve_ui_task =
{
    .module = THIS,
    .name = "User Interface Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = &pst_ui_mgmt,
    .routine = ui_do_proc,
};

static inline int
ui_config_from_yaml(ui_mgmt_st *m)
{
    int xret = -1;
    assert (m);
    ConfNode *interface = ConfGetNode("interface");
    ConfNode *iobj = NULL;

    TAILQ_FOREACH(iobj, &interface->head, next){
        if(!STRCMP(iobj->name, "ui")){
            ConfNode *obj = NULL;
            char *listen_port = NULL;
            char *ipaddress = NULL;
            TAILQ_FOREACH(obj, &iobj->head, next){
                if(ConfGetChildValue(obj, "ipaddress", &ipaddress)){
                    m->ip = strdup(ipaddress);
                    if(!m->ip){
                        rt_log_error(ERRNO_UI_YAML_ARGU, "Invalid ipaddress");
                        goto finish;
                    }
                    xret = 0;
                }
                if(ConfGetChildValue(obj, "port", &listen_port)){
                    m->port = tcp_udp_port_parse(listen_port);
                    if(m->port < 0){
                        rt_log_error(ERRNO_UI_YAML_ARGU, "Invalid port");
                        goto finish;
                    }
                    xret = 0;
                }
            }
        }
    }
finish:
    return xret;
}

static inline void
ui_config()
{
    ui_config_from_yaml(&pst_ui_mgmt);
    
    printf("\r\nInterface for UI Preview\n");
    printf("%30s:%60s\n", "The Ipaddress", pst_ui_mgmt.ip);
    printf("%30s:%60d\n", "The Port", pst_ui_mgmt.port);    
}

void ui_init_a29()
{
    ui_config();
     
    task_registry(&serve_ui_task);

}
