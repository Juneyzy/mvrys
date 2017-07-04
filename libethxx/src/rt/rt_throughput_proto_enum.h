#ifndef __RT_THROUGHPUT_PROTO_ENUM_H__
#define __RT_THROUGHPUT_PROTO_ENUM_H__

enum rt_layer{
    RT_LAYER2,
    RT_LAYER3,
    RT_LAYER4,
    RT_LAYER5,
    RT_LAYERX
};

enum rt_layer2_proto{
    RT_PROTO_ARP,
    RT_PROTO_MPLS,
    RT_PROTO_PPPOE,
    RT_PROTO_VLAN,
    RT_PROTO_IP,
    RT_PROTO_IPv6,

    RT_PROTO_LAYER2_MAX
};

enum rt_layer3_proto{
    RT_PROTO_ICMP,
    RT_PROTO_ICMPv6,
    RT_PROTO_IGMP,
    RT_PROTO_OSPF,
    RT_PROTO_OSPFv6,
    RT_PROTO_RIP
};

enum rt_layer4_proto{
    RT_PROTO_UDP,
    RT_PROTO_TCP,
    RT_PROTO_SCTP,
    RT_PROTO_SSH
};

enum rt_layer5_proto{
    RT_PROTO_FTP,
    RT_PROTO_TFTP,
    RT_PROTO_HTTP,
    RT_PROTO_DNS
};

struct rt_layer_proto{
    char rt_layer2_proto_str_map_table[RT_PROTO_LAYER2_MAX];
};

struct rt_proto_layer{
    void (*layer_format)(void);
}rt_proto_layer_instance[RT_LAYERX];


#endif
