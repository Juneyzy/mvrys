#ifndef __RT_H__
#define __RT_H__

#include <stdint.h>



#define max_routers                     4
#define max_ports_per_router    32

union rt_io{
    uint64_t data[3];
    union {
        uint64_t iv;
        float fv;
    }in;
    union {
        uint64_t iv;
        float fv;
    }out;
    union {
        uint64_t iv;
        float fv;
    }total;
};

struct rt_throughput {
    /** bit statistics */
    union rt_io bs;
    /** packet statistics */
    union rt_io ps;
    /** bit statistics per second */
    union rt_io bps;
    /** packet statistics per second */
    union rt_io pps;
    /** bandwidth usage */
    union rt_io bw_usage;
};


/** real time throughput */
struct rt_throughput_entry{
    /** timestamp of current record */
    uint64_t tm;
    struct rt_throughput throughput;
    struct rt_throughput_entry *next, *prev;
};


/** real time protocol_throughput */
#define DATA_LINK_LAYER    11
#define NETWORK_LAYER      16
#define TRANSPORT_LAYER    7
#define APPLICATION_LAYER  13
struct rt_throughput_proto_entry{
    /** timestamp of current record */
    uint64_t tm;
    struct rt_throughput throughput_link[DATA_LINK_LAYER];
    struct rt_throughput throughput_net[NETWORK_LAYER];
    struct rt_throughput throughput_trans[TRANSPORT_LAYER];
    struct rt_throughput throughput_app[APPLICATION_LAYER];
    struct rt_throughput_proto_entry *next, *prev;
};
enum rt_proto_layer{
    RT_PROTO_ALL_LAYER = 0,
    RT_PROTO_DATA_LINK ,
    RT_PROTO_NETWORK,
    RT_PROTO_TRANSPORT,
    RT_PROTO_APPLICATION,
    RT_PROTO_MAX,
};

/** real time message_throughput */
#define MESSAGE_LEN_THROUGHPUT 7
struct rt_throughput_message_entry{
    /** timestamp of current record */
    uint64_t tm;
    struct rt_throughput throughput[MESSAGE_LEN_THROUGHPUT];
    struct rt_throughput_message_entry *next, *prev;
};


/** real time summary_throughput */
#define SUMMARY_SESSION  4
#define SUMMARY_PROTOCOL 5
struct rt_throughput_summary_entry{
    /** timestamp of current record */
    uint64_t tm;
    union rt_io rt_io_session[SUMMARY_SESSION];
    union rt_io rt_io_protocol[SUMMARY_PROTOCOL];
    struct rt_throughput_summary_entry *next, *prev;
};


/** real time exception_msg_throughput */
#define EXCE_MSG_THROUGHPUT  10
struct rt_throughput_exce_entry{
    /** timestamp of current record */
    uint64_t tm;
    struct rt_throughput throughput[EXCE_MSG_THROUGHPUT];
    struct rt_throughput_exce_entry *next, *prev;
};


/**
 * These are reserved protocol IDs.
 */
enum
{
    RT_PROTO_UNKNOWN = 0,
    RT_PROTO_IP,
    RT_PROTO_IPV6,
    RT_PROTO_ICMP,
    RT_PROTO_ICMPV6,
    RT_PROTO_IGMP,
    RT_PROTO_ARP,
    RT_PROTO_PIM,
    RT_PROTO_TCP,
    RT_PROTO_UDP,
    RT_PROTO_SCTP,
    RT_PROTO_IPSEC,
    RT_PROTO_WAP,
    RT_PROTO_WSP,
    RT_PROTO_WTP,
    RT_PROTO_WEBBINARYXML,
    RT_PROTO_PPP,
    RT_PROTO_PPPIP,    /** by tsihang, start */
    RT_PORTO_PPPIPCP,
    RT_PROTO_PPPLCP,   /** by tsihang, end */
    RT_PROTO_PPPCHAP,
    RT_PROTO_PPPPAP,
    RT_PROTO_GRE,
    RT_PROTO_PPTP,
    RT_PROTO_L2TP,
    RT_PROTO_MMS,
    RT_PROTO_MMSENCAP,
    RT_PROTO_OSPF,
    RT_PROTO_RTP,
    RT_PROTO_AH,
    RT_PROTO_IPCOMP,
    // Insert more protocols here if needed, and add lines in init_rsv_protos().

    /* Total number of reserved protocols. The IDs of customer defined protocols
    * should start with this number. */
    TOTAL_RT_PROTOS,
};

enum template_type{
    TEMP_RTSS,
    TEMP_RTSS_PROTO,
    TEMP_RTSS_MESSAGE,
    TEMP_RTSS_SUMMARY,
    TEMP_RTSS_EXCEPTION,
    TEMP_MAX
};

struct template_priv{
    const char *desc;
    enum template_type type;
    void (*init)();
    void *(*param)();
};

struct eval_template{
    struct template_priv priv;
    void *(*cache_ctx)(void *param);
    void *(*archive_ctx)(void *param);
    int (*cache)(void *list, void *entry);
    int (*retrieve)(void *list);
    int (*cleanup)(void *list);
    int (*query)(uint64_t sts, uint64_t ets, void *list);
}*xtemplate[TEMP_MAX];


#define template_install(type, template) if (type < TEMP_MAX) xtemplate[type] = template



extern void rt_init();

#endif
