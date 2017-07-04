
#ifndef __ETHXX_PROTOCOL_H__
#define __ETHXX_PROTOCOL_H__

#include <stdint.h>

#define IPV4    4
#define IPV6    6

#define ETH_IPV4        0x0800
#define ETH_ARP         0x0806
#define ETH_VLAN        0x8100
#define ETH_IPV6        0x86DD
#define ETH_MPLS_UT     0x8847      /** MPLS Unicast traffic     */
#define ETH_MPLS_MT     0x8848      /** MPLS Multicast traffic   */
#define ETH_PPPOE_DIS   0x8863      /** PPPoE discovery messages */
#define ETH_PPPOE_SES   0x8864      /** PPPoE session messages   */
#define ETH_QINQ        0x9100

/** PPPOE encapsulation */
#define ETH_PPPOE_IP          0x0021    /** Internet Protocol */
#define ETH_PPPOE_VJ_COMP     0x002d
#define ETH_PPPOE_VJ_UNCOMP   0x002f
#define ETH_PPPOE_CHAP        0xc223    /** Challenge Handshake Authentication Protocol */
#define ETH_PPPOE_LCP         0xc021    /** Link Control Protocol */
#define ETH_PPPOE_PAP         0xc023    /** Password Authentication Protocol */
#define ETH_PPPOE_IPCP        0x8021    /** Internet Protocol Control Protocol */

/** Reserved protocol of L3 and L4 */
#define PROTOCOL_ICMP       1           /* control message protocol */
#define PROTOCOL_IGMP       2           /* group mgmt protocol */
#define PROTOCOL_GGP        3           /* gateway^2 (deprecated) */
#define PROTOCOL_IP         4           /* IP inside IP */
#define PROTOCOL_IPIP       4           /* IP inside IP */
#define PROTOCOL_TCP        6
#define PROTOCOL_EGP        8           /* exterior gateway protocol */
#define PROTOCOL_PUP        12          /* pup */
#define PROTOCOL_UDP        17
#define PROTOCOL_IDP        22          /* xns idp */
#define PROTOCOL_TP         29          /* tp-4 w/ class negotiation */
#define PROTOCOL_IPV6       41          /* IPv6 in IPv6 */
#define PROTOCOL_ROUTING    43          /* Routing header */
#define PROTOCOL_FRAGMENT   44          /* Fragmentation/reassembly header */
#define PROTOCOL_IPV6_FRAG  0x2c        /* Fragmentation/reassembly header */
#define PROTOCOL_GRE        47          /* GRE encap, RFCs 1701/1702 */
#define PROTOCOL_ESP        50          /* Encap. Security Payload */
#define PROTOCOL_AH         51          /* Authentication header */
#define PROTOCOL_MOBILE     55          /* IP Mobility, RFC 2004 */
#define PROTOCOL_ICMPV6     58          /* ICMP for IPv6 */
#define PROTOCOL_NONE       59          /* No next header */
#define PROTOCOL_DSTOPTS    60          /* Destination options header */
#define PROTOCOL_EON        80          /* ISO cnlp */
#define PROTOCOL_ETHERIP    97          /* Ethernet in IPv4 */
#define PROTOCOL_ENCAP      98          /* encapsulation header */
#define PROTOCOL_PIM        103         /* Protocol indep. multicast */
#define PROTOCOL_IPCOMP     108         /* IP Payload Comp. Protocol */
#define PROTOCOL_CARP       112         /* CARP */
#define PROTOCOL_L2TP       115         /* Layer 2 Tunneling */
#define PROTOCOL_SCTP       132         /* SCTP */
#define PROTOCOL_PFSYNC     240         /* PFSYNC */
#define PROTOCOL_RAW        255         /* raw IP packet */
#define PROTOCOL_OSPF       0x59
#define PROTOCOL_RSVP       0x2e

typedef union
{
    uint64_t        data64[2];
    struct
    {
        uint32_t        src_ip;
        uint32_t        dst_ip;
        uint16_t        src_port;
        uint16_t        dst_port;
        uint8_t         protocol;
        uint8_t         resv1;
        uint16_t        resv2;
    };
} tuple_v4;

typedef union
{
    uint64_t        data64[5];
    struct
    {
        /** Word 0 */
        uint64_t        sip_upper;
        /** Word 1 */
        uint64_t        sip_lower;
        /** Word 2 */
        uint64_t        dip_upper;
        /** Word 3 */
        uint64_t        dip_lower;
        /** Word 4 */
        uint16_t        src_port;
        uint16_t        dst_port;
        uint8_t         protocol;
        uint8_t         resv1;
        uint16_t        resv2;
    };
} tuple_v6;

typedef union
{
    uint64_t    value[2];

    struct
    {
        uint32_t    reserved;
        uint32_t    addr_v4;
    };
    struct
    {
        uint64_t    addr_v6_upper;
        uint64_t    addr_v6_lower;
    };

} ip_addr_t;

typedef union
{
    uint64_t        data64[5];
    tuple_v4        v4;
    tuple_v6        v6;
} ip5tuple_st;

typedef union
{
    uint8_t  data[20];
    uint32_t data32[5];
    struct
    {
#if IS_BIG_ENDIAN
        /* uint32_t word1; */
        uint32_t version: 4;
        uint32_t ipHeaderLen: 4;
        uint32_t ip_tos: 8;
        uint32_t totalLen: 16;
        /* uint32_t word2; */
        uint32_t id: 16;
        uint32_t flags: 3;
        uint32_t offset: 13;
        /* uint32_t word3; */
        uint32_t TTL: 8;
        uint32_t protocol: 8;
        uint32_t checksum: 16;
#else
        /* uint32_t word1; */
        uint32_t totalLen: 16;
        uint32_t ip_tos: 8;
        uint32_t ipHeaderLen: 4;
        uint32_t version: 4;
        /* uint32_t word2; */
        uint32_t offset: 13;
        uint32_t flags: 3;
        uint32_t id: 16;
        /* uint32_t word3; */
        uint32_t checksum: 16;
        uint32_t protocol: 8;
        uint32_t TTL: 8;
#endif        
        /* uint32_t word4; */
        uint32_t src_ip;
        /* uint32_t word5; */
        uint32_t dst_ip;
    };
} ipv4_header_st;

typedef union
{
    uint8_t data[40];
    uint32_t data32[10];
    struct
    {
#if IS_BIG_ENDIAN
        /* uint32_t word1; */
        uint32_t version: 4;
        uint32_t traffic_class: 8;
        uint32_t flow_label: 20;

        /* uint32_t word2; */
        uint32_t payload_len: 16;
        uint32_t next_header: 8;
        uint32_t hop_limit: 8;
#else
        /* uint32_t word1; */
        uint32_t flow_label: 20;
        uint32_t traffic_class: 8;
        uint32_t version: 4;
        /* uint32_t word2; */
        uint32_t hop_limit: 8;
        uint32_t next_header: 8;
        uint32_t payload_len: 16;
#endif
        ip_addr_t src_ip;
        ip_addr_t dst_ip;
    };
} ipv6_header_st;

typedef union
{
    uint8_t value[20];
    uint32_t value32[5];
    struct
    {
        /* uint32_t word1; */
        uint16_t src_port;
        uint16_t dst_port;
        /* uint32_t word2; */
        uint32_t seq;
        /* uint32_t word3; */
        uint32_t ack;
#if IS_BIG_ENDIAN
        /* uint32_t word4; */
        uint32_t data_offset: 4;
        uint32_t reserved: 6;
        uint32_t flags: 6;
        uint32_t window: 16;
        /* uint32_t word5; */
        uint32_t checksum: 16;
        uint32_t urgent: 16;
#else
        /* uint32_t word4; */
        uint32_t window: 16;
        uint32_t flags: 6;
        uint32_t reserved: 6;
        uint32_t data_offset: 4;
        /* uint32_t word5; */
        uint32_t urgent: 16;
        uint32_t checksum: 16;
#endif
    };
} tcp_header_st;

typedef struct
{
    uint16_t flags;         /** Flags and version */
    uint16_t protocol;      /** Protocol type */
    uint32_t key;
    uint32_t sqnum;
} gre_header_st;

typedef struct
{
#if IS_BIG_ENDIAN
    uint32_t lable: 20;
    uint32_t exp  : 3 ;
    uint32_t s    : 1 ;
    uint32_t ttl  : 8 ;
#else
    uint32_t ttl  : 8 ;
    uint32_t s    : 1 ;
    uint32_t exp  : 3 ;
    uint32_t lable: 20;
#endif
} __attribute__((packed)) mpls_header_st;

typedef struct
{
#if IS_BIG_ENDIAN
    uint8_t     version: 4;
    uint8_t     type: 4;
#else
    uint8_t     type: 4;
    uint8_t     version: 4;
#endif
    uint8_t     code;
    uint16_t    session_id;
    uint16_t    payload_len;
    uint16_t    p2p_proto;
} __attribute__((packed)) pppoe_header_st;

typedef union
{
    uint8_t value[8];
    struct
    {
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t length;
        uint16_t checksum;
    };

} udp_header_st;

typedef struct
{
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t vtag;
    uint32_t checksum;
} sctp_header_st;

typedef struct
{
    uint8_t ether_dhost[6]; /* dest mac */
    uint8_t ether_shost[6]; /* src mac */
    uint16_t ether_type; /* IP ARP RARP etc */
}sniff_ethernet_st;

enum {
    PARSE_ARP,
    PARSE_TCP,
    PARSE_UDP,
    PARSE_SCTP,
    PARSE_IPV4,
    PARSE_IPV6,
    PARSE_NOT_IP,
    PARSE_FREE
};

#define PCKT_ARP    0x0001
#define PCKT_TCP    0x0002
#define PCKT_UDP    0x0004
#define PCKT_IPV4   0x0010
#define PCKT_IPV6   0x0020
#define PCKT_NOT_IP 0x0040
#define PCKT_FRAG   0x0080

static __rt_always_inline__ void ipv4hdr_hton(ipv4_header_st *src, ipv4_header_st *dest, int s)
{
    int i = 0;

    i = (int)(s/4);
    while(i --)
        dest->data32[i] = HTONL(src->data32[i]);
}

static __rt_always_inline__ void ipv6hdr_hton(ipv6_header_st *src, ipv6_header_st *dest, int s)
{
    int i = 0;
    
    i = (int)(s/4);
    while(i --)
        dest->data32[i] = HTONL(src->data32[i]);
}

static __rt_always_inline__ void tcphdr_hton(tcp_header_st *src, tcp_header_st *dest, 
                        int __attribute__((__unused__))s)
{
    dest->src_port = HTONS(src->src_port);
    dest->dst_port = HTONS(src->dst_port);
    dest->seq = HTONL(src->seq);
    dest->ack = HTONL(src->ack);
    dest->value32[3] = HTONL(src->value32[3]);
    dest->value32[4] = HTONL(src->value32[4]);
}

static __rt_always_inline__ void udphdr_hton(udp_header_st *src, udp_header_st *dest,
                        int __attribute__((__unused__))s)
{
    dest->src_port    =   HTONS(src->src_port);
    dest->dst_port    =   HTONS(src->dst_port);
    dest->length      =   HTONS(src->length);
    dest->checksum    =   HTONS(src->checksum);
}

static __rt_always_inline__ void sctphdr_hton(sctp_header_st *src, sctp_header_st *dest,
                        int __attribute__((__unused__))s)
{
    dest->src_port   = HTONS(src->src_port);
    dest->dst_port   = HTONS(src->dst_port);
    dest->vtag       = HTONL(src->vtag);
    dest->checksum   = HTONL(src->checksum);
}

static __rt_always_inline__ void mplshdr_hton(mpls_header_st *src, mpls_header_st *dest,
                        int __attribute__((__unused__))s)
{
    * (uint32_t *) dest = HTONL(* (uint32_t *) src);
}

#define xhdr_hton(hdr, src, dest, s) hdr##hdr_hton(src, dest, s)

#endif  /* __RV_PROTO_H__ */

