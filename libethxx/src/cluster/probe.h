
#ifndef __PROBE_H__
#define __PROBE_H__


#include "sockunion.h"

struct protocol_tcp_stat
{
    uint64_t ingress_telnet_pkts, egress_telnet_pkts;   /* ingress/egress local router telnet packets */
    uint64_t ingress_ssh_pkts, egress_ssh_pkts;      /* ingress local router ssh packets */
    uint64_t ingress_ftp_pkts, egress_ftp_pkts;    /* ingress/egress local router ftp packets*/
    uint64_t ingress_smpt_pkts, egress_smpt_pkts;    /* ingress/egress local router SMPT packets*/
    uint64_t ingress_http_pkts, egress_http_pkts;    /* ingress/egress local router http packets*/
};

struct protocol_udp_stat
{
    uint64_t ingress_tftp_pkts, egress_tftp_pkts;    /* ingress/egress local router tftp packets*/
};

struct osi_layer4_stat{
    uint64_t ingress_tcp_pkts,  egress_tcp_pkts;
    uint64_t ingress_udp_pkts,  egress_udp_pkts;
    uint64_t ingress_sctp_pkts, egress_sctp_pkts;
    struct protocol_tcp_stat protocoltcp;
    struct protocol_udp_stat protocoludp;
};

struct osi_layer3_stat{
    uint64_t ingress_icmp_pkts, egress_icmp_pkts;
    uint64_t ingress_igmp_pkts, egress_igmp_pkts;
    uint64_t ingress_ipv4_pkts, egress_ipv4_pkts;
    uint64_t ingress_ipv6_pkts, egress_ipv6_pkts;
    uint64_t ingress_ospf_pkts, egress_ospf_pkts;
    uint64_t ingress_bgpd_pkts, egress_bgpd_pkts;
    uint64_t ingress_ripd_pkts, egress_ripd_pkts;
    struct osi_layer4_stat osi_layer4;
};

struct osi_layer2_stat{
    uint64_t ingress_layer2_pkts, egress_layer2_pkts;  /* only-layer2 packets from ingress/egress */
    uint64_t ingress_arp_pkts,    egress_arp_pkts;        /* ingress/egress local router arp packets */
    struct osi_layer3_stat osi_layer3;
};

struct exception_stat{
    uint64_t lost_pkts;                          /* total lost packets */
    uint64_t drop_pkts;                          /* total droped packets */
    uint64_t drop_icmp_pkts;                     /* droped pcakets: icmp */
    uint64_t drop_ttlzero_pkts;                  /* droped packets: ttl is zero */
    uint64_t drop_layer2_pkts;                   /* droped packets: layer two */
    uint64_t drop_error_pkts;                    /* droped packets: error packets */
    uint64_t ingress_lr_Pkts,      egress_lr_Pkts;        /* ingress/egress local router packets */
    uint64_t ingress_nln_pkts,     egress_nln_pkts;       /* ingress/egress not local domain packets */
    uint64_t ingress_lr_req_pkts,  egress_lr_req_pkts;    /* ingress/egress local router request packets */
    uint64_t ingress_lr_ctrl_pkts, egress_lr_ctrl_pkts;   /* ingress/egress local router control packets */
};

typedef struct
{
    uint64_t recv_pkts;            /* total received packets */
    uint64_t send_pkts;            /* total sent packets */
    struct osi_layer2_stat ethernet_II;
    struct osi_layer2_stat ethernet_8023_raw;
    struct exception_stat  exception;
}probe_stat_st;

struct host_usage
{
    uint64_t memTotal;
    float memUsage;
    float cpuUsage;
};

struct pc_host{

    int valid;

    int sock;

    /** connection port of slave */
    uint16_t port;

    union sockunion su;
    union sockunion mask;
    /** */
    struct pc_host *next, *prev;

    /*keepalive time*/
    int keepalive_timer;

    /*probe static*/
    struct host_usage host;

    probe_stat_st stat, stat_bak;

    rt_mutex mtx;
};


static inline void
probe_stat_hton(probe_stat_st* probe_stat)
{
    probe_stat->recv_pkts = HTONLL (probe_stat->recv_pkts);
    probe_stat->send_pkts = HTONLL (probe_stat->send_pkts);
    probe_stat->exception.lost_pkts = HTONLL (probe_stat->exception.lost_pkts);
    probe_stat->exception.drop_pkts = HTONLL (probe_stat->exception.drop_pkts);
    probe_stat->exception.drop_icmp_pkts    = HTONLL (probe_stat->exception.drop_icmp_pkts);
    probe_stat->exception.drop_ttlzero_pkts = HTONLL (probe_stat->exception.drop_ttlzero_pkts);
    probe_stat->exception.drop_layer2_pkts  = HTONLL (probe_stat->exception.drop_layer2_pkts);
    probe_stat->exception.drop_error_pkts   = HTONLL (probe_stat->exception.drop_error_pkts);
}

#endif  /* __PROBE_STAT_H__ */

