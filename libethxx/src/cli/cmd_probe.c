#include "sysdefs.h"
#include "command.h"
#include "cluster_probe.h"
#include "rpc_probe.h"

#define PROBE_STAT_SHOW_DATA_FORMAT "  %-22lu"
#define PROBE_STAT_SHOW_DATA_PROMPT_FORMAT "  %-22s"

#define PROBE_STAT_SHOW_PROMPT_FORMAT "  %-30s"

DEFUN(get_statistics_from_probe, get_statistics_from_probe_cmd,
      "show probe statistics",
      SHOW_STR
      SHOW_CSTR
      "probe\n"
      "probe\n"
      STATISTICS_STR
      STATISTICS_CSTR)
{
    self = self;
    argc = argc;
    argv = argv;

    probe_stat_st stat;
    memset (&stat, 0, sizeof(probe_stat_st));
    uint64_t uiPromptbuf[12][37];
    int probeId = 0;
    int probeCnt = 0;
    struct pc_host *prob = NULL;
    struct pc_hostlist *prob_list = NULL;

    CpssReqEthernetGetStatistics ();

    prob_list = local_probe_cluster();
    if (NULL == prob_list){
        return CMD_ERR_NO_MATCH;
    }
    probelist_lock(prob_list);
    probe_cluster_foreach_host(prob_list, prob)
    {
        if (prob->valid &&
            prob->sock < 0)
        {
            continue;
        }
        uiPromptbuf[probeId][0] = prob->sock;
        uiPromptbuf[probeId][1] = prob->stat_bak.recv_pkts;
        uiPromptbuf[probeId][2] = prob->stat_bak.send_pkts;
        uiPromptbuf[probeId][3] = prob->stat_bak.exception.lost_pkts;
        uiPromptbuf[probeId][4] = prob->stat_bak.exception.drop_pkts;
        uiPromptbuf[probeId][5] = prob->stat_bak.exception.drop_icmp_pkts;
        uiPromptbuf[probeId][6] = prob->stat_bak.exception.drop_ttlzero_pkts;
        uiPromptbuf[probeId][7] = prob->stat_bak.exception.drop_layer2_pkts;
        uiPromptbuf[probeId][8] = prob->stat_bak.exception.drop_error_pkts;
        uiPromptbuf[probeId][9] = prob->stat_bak.exception.ingress_lr_Pkts;
        uiPromptbuf[probeId][10] = prob->stat_bak.exception.ingress_nln_pkts;
        uiPromptbuf[probeId][11] = prob->stat_bak.exception.ingress_lr_req_pkts;
        uiPromptbuf[probeId][12] = prob->stat_bak.exception.ingress_lr_ctrl_pkts;
        uiPromptbuf[probeId][13] = prob->stat_bak.ethernet_II.ingress_layer2_pkts;
        uiPromptbuf[probeId][14] = prob->stat_bak.ethernet_II.ingress_arp_pkts;
        uiPromptbuf[probeId][15] = prob->stat_bak.ethernet_II.osi_layer3.ingress_icmp_pkts;
        uiPromptbuf[probeId][16] = prob->stat_bak.ethernet_II.osi_layer3.ingress_igmp_pkts;
        uiPromptbuf[probeId][17] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts;
        uiPromptbuf[probeId][18] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts;
        uiPromptbuf[probeId][19] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts;
        uiPromptbuf[probeId][20] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts;
        uiPromptbuf[probeId][21] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts;
        uiPromptbuf[probeId][22] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts;
        uiPromptbuf[probeId][23] = prob->stat_bak.exception.egress_lr_Pkts;
        uiPromptbuf[probeId][24] = prob->stat_bak.exception.egress_nln_pkts;
        uiPromptbuf[probeId][25] = prob->stat_bak.exception.egress_lr_req_pkts;
        uiPromptbuf[probeId][26] = prob->stat_bak.exception.egress_lr_ctrl_pkts;
        uiPromptbuf[probeId][27] = prob->stat_bak.ethernet_II.egress_layer2_pkts;
        uiPromptbuf[probeId][28] = prob->stat_bak.ethernet_II.egress_arp_pkts;
        uiPromptbuf[probeId][29] = prob->stat_bak.ethernet_II.osi_layer3.egress_icmp_pkts;
        uiPromptbuf[probeId][30] = prob->stat_bak.ethernet_II.osi_layer3.egress_igmp_pkts;
        uiPromptbuf[probeId][31] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts;
        uiPromptbuf[probeId][32] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts;
        uiPromptbuf[probeId][33] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts;
        uiPromptbuf[probeId][34] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts;
        uiPromptbuf[probeId][35] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts;
        uiPromptbuf[probeId][36] = prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts;
        stat.recv_pkts += prob->stat_bak.recv_pkts;
        stat.send_pkts += prob->stat_bak.send_pkts;
        stat.exception.lost_pkts += prob->stat_bak.exception.lost_pkts;
        stat.exception.drop_pkts += prob->stat_bak.exception.drop_pkts;
        stat.exception.drop_icmp_pkts        += prob->stat_bak.exception.drop_icmp_pkts;
        stat.exception.drop_ttlzero_pkts     += prob->stat_bak.exception.drop_ttlzero_pkts;
        stat.exception.drop_layer2_pkts      += prob->stat_bak.exception.drop_layer2_pkts;
        stat.exception.drop_error_pkts       += prob->stat_bak.exception.drop_error_pkts;
        stat.exception.ingress_lr_Pkts       += prob->stat_bak.exception.ingress_lr_Pkts;
        stat.exception.ingress_nln_pkts      += prob->stat_bak.exception.ingress_nln_pkts;
        stat.exception.ingress_lr_req_pkts   += prob->stat_bak.exception.ingress_lr_req_pkts;
        stat.exception.ingress_lr_ctrl_pkts  += prob->stat_bak.exception.ingress_lr_ctrl_pkts;
        stat.ethernet_II.ingress_layer2_pkts   += prob->stat_bak.ethernet_II.ingress_layer2_pkts;
        stat.ethernet_II.ingress_arp_pkts      += prob->stat_bak.ethernet_II.ingress_arp_pkts;
        stat.ethernet_II.osi_layer3.ingress_icmp_pkts     += prob->stat_bak.ethernet_II.osi_layer3.ingress_icmp_pkts;
        stat.ethernet_II.osi_layer3.ingress_igmp_pkts     += prob->stat_bak.ethernet_II.osi_layer3.ingress_igmp_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts   += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts      += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts      += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts     += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts     += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts     += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts;
        stat.exception.egress_lr_Pkts        += prob->stat_bak.exception.egress_lr_Pkts;
        stat.exception.egress_nln_pkts       += prob->stat_bak.exception.egress_nln_pkts;
        stat.exception.egress_lr_req_pkts    += prob->stat_bak.exception.egress_lr_req_pkts;
        stat.exception.egress_lr_ctrl_pkts   += prob->stat_bak.exception.egress_lr_ctrl_pkts;
        stat.ethernet_II.egress_layer2_pkts    += prob->stat_bak.ethernet_II.egress_layer2_pkts;
        stat.ethernet_II.egress_arp_pkts       += prob->stat_bak.ethernet_II.egress_arp_pkts;
        stat.ethernet_II.osi_layer3.egress_icmp_pkts      += prob->stat_bak.ethernet_II.osi_layer3.egress_icmp_pkts;
        stat.ethernet_II.osi_layer3.egress_igmp_pkts      += prob->stat_bak.ethernet_II.osi_layer3.egress_igmp_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts    += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts       += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts       += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts      += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts      += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts;
        stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts      += prob->stat_bak.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts;
        probeId++;
    }
    probelist_unlock(prob_list);

    char buf[256] = {0};
    int len = 0;

    vty_out (vty, "\r\n");
    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "PROBE:");
    len = snprintf (buf + len, 256, PROBE_STAT_SHOW_PROMPT_FORMAT, "------");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][0]);

       len += snprintf (buf + len, 256 - len, PROBE_STAT_SHOW_DATA_PROMPT_FORMAT, "--");

    }
    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "total");

    len += snprintf (buf + len, 256 - len, PROBE_STAT_SHOW_PROMPT_FORMAT, "-----");

    //vty_out (vty, "\r\n");
    vty_out (vty, "\r\n%s\r\n", buf);

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "RECV-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][1]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.recv_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "SEND-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][2]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.send_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
              "LOST-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
     vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                  uiPromptbuf[probeCnt][3]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
              stat.exception.lost_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "DROP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][4]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.drop_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "DROP-ICMP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][5]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.drop_icmp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "DROP-TTLZERO-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][6]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.drop_ttlzero_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "DROP-LAYER2-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][7]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.drop_layer2_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "DROP-ERROR-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][8]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.drop_error_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-LR-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][9]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.ingress_lr_Pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-NLR-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][10]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.ingress_nln_pkts);
    vty_out (vty, "\r\n");


    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-REQ-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][11]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.ingress_lr_req_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-CTRL-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][12]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.ingress_lr_ctrl_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-lAYER2-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][13]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.ingress_layer2_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-ARP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][14]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.ingress_arp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-ICMP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][15]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.ingress_icmp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-IGMP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][16]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.ingress_igmp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-TELNET-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][17]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_telnet_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-SSH-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][18]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ssh_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-FTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][19]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_ftp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-SMPT-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][20]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_smpt_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-HTTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][21]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.ingress_http_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "INGRESS-TFTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][22]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.ingress_tftp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-LR-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][23]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.egress_lr_Pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-NLR-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][24]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.egress_nln_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-REQ-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][25]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.egress_lr_req_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-CTRL-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][26]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.exception.egress_lr_ctrl_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-lAYER2-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][27]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.egress_layer2_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-ARP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][28]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.egress_arp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-ICMP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][29]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.egress_icmp_pkts);
    vty_out (vty, "\r\n");


    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-IGMP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][30]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.egress_igmp_pkts);
    vty_out (vty, "\r\n");


    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-TELNET-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][31]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_telnet_pkts);
    vty_out (vty, "\r\n");


    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-SSH-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][32]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ssh_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-FTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][33]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_ftp_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-SMPT-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][34]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_smpt_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-HTTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][35]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoltcp.egress_http_pkts);
    vty_out (vty, "\r\n");

    vty_out(vty,PROBE_STAT_SHOW_PROMPT_FORMAT,
                "EGRESS-TFTP-PKTS");
    for(probeCnt = 0; probeCnt < probeId; probeCnt++)
    {
       vty_out(vty, PROBE_STAT_SHOW_DATA_FORMAT,
                    uiPromptbuf[probeCnt][36]);

    }
    vty_out(vty,PROBE_STAT_SHOW_DATA_FORMAT,
                stat.ethernet_II.osi_layer3.osi_layer4.protocoludp.egress_tftp_pkts);
    vty_out (vty, "\r\n");
    vty_out (vty, "\r\n");

    return CMD_SUCCESS;
}

#define PROBE_HOST_SHOW_PROMPT_FORMAT     "  %5s%14s%14s%14s\r\n"
#define PROBE_HOST_SHOW_DATA_FORMAT       "  %5d%13.2f%%%12luKB%13.2f%%\r\n"

#define PROBE_HOST_SHOW_PROMPT()\
do{\
    vty_out (vty, "\r\n");\
    vty_out (vty, PROBE_HOST_SHOW_PROMPT_FORMAT, \
                        "PROBE", "CPU-USAGE", "MEM-TOTAL", "MEM-USAGE");\
    vty_out (vty, PROBE_HOST_SHOW_PROMPT_FORMAT, \
                        "-----", "---------", "---------", "---------");\
}while(0)

DEFUN(get_host_from_probe, get_host_from_probe_cmd,
      "show probe host",
      SHOW_STR
      SHOW_CSTR
      "Probe\n"
      "Probe\n"
      "Probe host\n"
      "Probe本地信息\n")
{
    self = self;
    argc = argc;
    argv = argv;

    probe_stat_st stat;
    memset (&stat, 0, sizeof(probe_stat_st));
    struct pc_host *prob = NULL;
    struct pc_hostlist *prob_list = NULL;

    prob_list = local_probe_cluster();
    if (NULL == prob_list){
        return CMD_ERR_NO_MATCH;
    }

    CpssReqGetHostInfo ();

    PROBE_HOST_SHOW_PROMPT ();
    probelist_lock(prob_list);
    probe_cluster_foreach_host(prob_list, prob)
    {
        if (prob->valid &&
        prob->sock < 0)
        {
            continue;
        }
        vty_out (vty, PROBE_HOST_SHOW_DATA_FORMAT,
                prob->sock,
                prob->host.cpuUsage * 100,
                prob->host.memTotal,
                prob->host.memUsage * 100);
    }
    probelist_unlock(prob_list);
    vty_out (vty, "%s", VTY_NEWLINE);

    return CMD_SUCCESS;
}


void probe_cmdline_initialize(void)
{
    install_element (CONFIG_NODE, &get_statistics_from_probe_cmd);
    install_element (CONFIG_NODE, &get_host_from_probe_cmd);
}

