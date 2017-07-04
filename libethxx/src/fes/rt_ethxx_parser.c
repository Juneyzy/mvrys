
#include "sysdefs.h"
#include "rt_ethxx_proto.h"
#include "rt_ethxx_parser.h"
#include "rt_ethxx_packet.h"

//#define PROTOCOL_DEBUG

#ifdef PROTOCOL_DEBUG
#define protocol_debug(fmt,args...) printf(fmt,##args)
#define ipv4_5tuple_dump(tuple)\
    printf ("0x%08x, 0x%08x, 0x%04x, 0x%04x, 0x%02x\n",\
            tuple->v4.src_ip,\
            tuple->v4.dst_ip,\
            tuple->v4.src_port,\
            tuple->v4.dst_port,\
            tuple->v4.protocol);

#else
#define protocol_debug(fmt,args...)
#define ipv4_5tuple_dump(tuple)
#endif

static const int ipv4_hdr_size  = sizeof(ipv4_header_st);
static const int ipv6_hdr_size  = sizeof(ipv6_header_st);
static const int mpls_hdr_size  =   sizeof(mpls_header_st);
static const int pppoe_hdr_size =   sizeof(pppoe_header_st);

int rt_ethxx_packet_parser(void *snapshot, uint8_t *packet,
                          size_t pkt_len,
                          void __attribute__((__unused__))*argument,
                          size_t __attribute__((__unused__))as /** Argument size*/)
{
    uint16_t eth_type = 0x0;    /** Parse and Skip MAC Header */
    uint16_t eth_type_pos = 12;
    uint8_t *current_ip_hdr = NULL;
    uint8_t  current_ip_ver = 0;
    uint16_t ipHdrLen = 0;
    uint16_t ipHdrOff = 0;
    pppoe_header_st *current_pppoe_hdr = NULL;
    mpls_header_st  current_mpls_hdr;
    ipv4_header_st  current_ipv4_hdr;
    ipv6_header_st  current_ipv6_hdr;
    tcp_header_st   current_tcp_hdr;
    udp_header_st   current_udp_hdr;
    sctp_header_st  current_sctp_hdr;
    int	source_sys = FILTER_VRS_LOCAL;

#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
    if (argument) source_sys = *(int *)argument;
#endif

    struct rt_packet_snapshot_t*intro = (struct rt_packet_snapshot_t*)snapshot;
    ip5tuple_st *ft = &intro->ft;
    
    intro->packet = packet;
    intro->size = pkt_len;
    ft = &intro->ft;
   
    intro->rid = (*((uint8_t *)intro->packet + 6) & 0xE0) >> 5;
    intro->pid = (*((uint8_t *)intro->packet + 6) & 0x1E) >> 1;
    intro->dir = (*((uint8_t *)intro->packet + 7) & 0xC0) >> 6;

    intro->source_sys = source_sys;
#if SPASR_BRANCH_EQUAL(BRANCH_VRS)
    if (source_sys == FILTER_VRS_LOCAL) {
	    intro->call_snapshot.call.data = *(uint64_t *)(intro->packet + 20);
	    intro->call_snapshot.dir = intro->packet[28];
	    intro->call_snapshot.case_id = intro->packet[29];
	    intro->call_snapshot.mark = intro->packet[33];
	    intro->call_snapshot.payload_size = NTOHS(*(uint16_t *)(intro->packet + 35));
		/**
		   char date[32] = {0};
		   rt_tms2str (intro->call_snapshot.call.tm, EVAL_TM_STYLE_FULL, date, 32);
		   rt_log_debug("%s, callid=%lu[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x], [%d, %02x, %d]",
				date, intro->call_snapshot.call.data,
					   intro->packet[20],  intro->packet[21], intro->packet[22], intro->packet[23], intro->packet[24], intro->packet[25], intro->packet[26], intro->packet[27],
		                        intro->call_snapshot.dir, intro->call_snapshot.mark, intro->call_snapshot.payload_size);
		*/
    }
    else
    {
	;
    }
#endif

    while (eth_type_pos < pkt_len &&
                    eth_type != ETH_IPV4 && eth_type != ETH_IPV6 &&
                    eth_type != ETH_PPPOE_SES && eth_type != ETH_MPLS_MT && eth_type != ETH_MPLS_UT) {
                    eth_type = HTONS(* (uint16_t *)(packet + eth_type_pos));

        switch (eth_type)  {
            case ETH_IPV4:
            case ETH_IPV6:
                break;

            case ETH_MPLS_UT:
            case ETH_MPLS_MT:
                do  {
                    /** multi-mpls header parsing. */
                    xhdr_hton(mpls, (mpls_header_st *)(packet + eth_type_pos + 2), &current_mpls_hdr, 0);
                    eth_type_pos += mpls_hdr_size;
                }
                while (!current_mpls_hdr.s);

                break;

            case ETH_PPPOE_SES:
                current_pppoe_hdr = (pppoe_header_st *)(packet + eth_type_pos + 2);

                if (HTONS(current_pppoe_hdr->p2p_proto) != ETH_PPPOE_IP)
                {
                    return PARSE_NOT_IP;
                }

                eth_type_pos += pppoe_hdr_size;/** escape pppoe hdr */
                break;

            case ETH_ARP:
                return PARSE_ARP;

            case ETH_VLAN:
            case ETH_QINQ:
                eth_type_pos += 4;
                break;

            default:
                return PARSE_NOT_IP;
        }
    }

    ipHdrOff = eth_type_pos + 2;
    /** Get IPV4 Header */
    current_ip_hdr = packet + ipHdrOff;
    current_ip_ver = current_ip_hdr[0] >> 4;

    /**
    *   THE THIRD LAYER PACKET PARSING
    */

    if (current_ip_ver == 0x4)
    {
        intro->type = IPV4;
        xhdr_hton(ipv4, (ipv4_header_st *) current_ip_hdr, &current_ipv4_hdr, ipv4_hdr_size);
        /** Parse the Fields in the IP header */
        ft->v4.src_ip = current_ipv4_hdr.src_ip;
        ft->v4.dst_ip = current_ipv4_hdr.dst_ip;
        ipHdrLen = (current_ipv4_hdr.ipHeaderLen << 2);
        protocol_debug("ipHeaderLen: %d, current_ipv4_hdr->protocol: 0x%04x\n", ipHdrLen, current_ipv4_hdr.protocol);

        switch (current_ipv4_hdr.protocol)
        {
            case PROTOCOL_TCP:
                /** Parse the Fields in the TCP header */
                xhdr_hton(tcp, (tcp_header_st *)(current_ip_hdr + ipHdrLen), &current_tcp_hdr, 0);
                ft->v4.src_port = current_tcp_hdr.src_port;
                ft->v4.dst_port = current_tcp_hdr.dst_port;
                ft->v4.protocol = PROTOCOL_TCP;
                ipv4_5tuple_dump(ft);
                return (PARSE_TCP);

            case PROTOCOL_UDP:
                /** Parse the fields in the UDP header */
                xhdr_hton(udp, (udp_header_st *)(current_ip_hdr + ipHdrLen), &current_udp_hdr, 0);
                ft->v4.src_port = current_udp_hdr.src_port;
                ft->v4.dst_port = current_udp_hdr.dst_port;
                ft->v4.protocol = PROTOCOL_UDP;
                ipv4_5tuple_dump(ft);
                return (PARSE_UDP);

            case PROTOCOL_SCTP:
                /** Parse the fields in the SCTP header */
                xhdr_hton(sctp, (sctp_header_st *)(current_ip_hdr + ipHdrLen), &current_sctp_hdr, 0);
                ft->v4.src_port = current_sctp_hdr.src_port;
                ft->v4.dst_port = current_sctp_hdr.dst_port;
                ft->v4.protocol = PROTOCOL_SCTP;
                ipv4_5tuple_dump(ft);
                return (PARSE_SCTP);

            case PROTOCOL_ICMP:
                ft->v4.protocol = PROTOCOL_ICMP;
                ipv4_5tuple_dump(ft);
                return (PCKT_IPV4);

            case PROTOCOL_L2TP:
                ft->v4.protocol = PROTOCOL_L2TP;
                ipv4_5tuple_dump(ft);
                return (PCKT_IPV4);

            case PROTOCOL_GRE:
                protocol_debug("gre packet ...\n");
                ft->v4.protocol = PROTOCOL_GRE;
                ipv4_5tuple_dump(ft);
                return (PCKT_IPV4);

            default:
                protocol_debug("DEBUG: IPV4 default ...\n");
                ft->v4.protocol  =   current_ipv4_hdr.protocol;

                if (ft->v4.protocol != PROTOCOL_ICMP &&
                        ft->v4.protocol != PROTOCOL_IGMP &&
                        ft->v4.protocol != PROTOCOL_SCTP &&
                        ft->v4.protocol != PROTOCOL_OSPF &&
                        ft->v4.protocol != PROTOCOL_RSVP &&
                        ft->v4.protocol != PROTOCOL_AH &&
                        ft->v4.protocol != PROTOCOL_IPCOMP &&
                        ft->v4.protocol != PROTOCOL_ESP)
                {
                    ft->v4.protocol  =   PROTOCOL_IP;
                }

                ipv4_5tuple_dump(ft);
                return (PCKT_IPV4);
        }
    }
    else if (current_ip_ver == 0x6)
    {
        intro->type = IPV6;
        /* Parse the Fields in the IP header */
        xhdr_hton(ipv6, (ipv6_header_st *) current_ip_hdr, &current_ipv6_hdr, ipv6_hdr_size);
        ft->v6.sip_lower = current_ipv6_hdr.src_ip.value[0];
        ft->v6.sip_upper = current_ipv6_hdr.src_ip.value[1];
        ft->v6.dip_lower = current_ipv6_hdr.dst_ip.value[0];
        ft->v6.dip_upper = current_ipv6_hdr.dst_ip.value[1];
        ft->v6.protocol = current_ipv6_hdr.next_header;

        if (current_ipv6_hdr.next_header == PROTOCOL_TCP)
        {
            /* Parse the Fields in the TCP header */
            xhdr_hton(tcp, (tcp_header_st *)(current_ip_hdr + 40), &current_tcp_hdr, 0);
            ft->v6.protocol = PROTOCOL_TCP;
            ft->v6.src_port = current_tcp_hdr.src_port;
            ft->v6.dst_port = current_tcp_hdr.dst_port;
            return (PARSE_TCP);
        }
        else if (current_ipv6_hdr.next_header == PROTOCOL_UDP)
        {
            /* Parse the Fields in the UDP header */
            xhdr_hton(udp, (udp_header_st *)(current_ip_hdr + 40), &current_udp_hdr, 0);
            ft->v6.src_port = current_udp_hdr.src_port;
            ft->v6.dst_port = current_udp_hdr.dst_port;
            ft->v6.protocol = PROTOCOL_UDP;
            return (PARSE_UDP);
        }
        else
        {
            ft->v6.protocol = current_ipv6_hdr.next_header;

            if (ft->v6.protocol != PROTOCOL_ICMPV6 &&
                    ft->v6.protocol != PROTOCOL_IGMP &&
                    ft->v6.protocol != PROTOCOL_SCTP &&
                    ft->v6.protocol != PROTOCOL_OSPF &&
                    ft->v6.protocol != PROTOCOL_RSVP &&
                    ft->v6.protocol != PROTOCOL_AH &&
                    ft->v6.protocol != PROTOCOL_IPCOMP &&
                    ft->v6.protocol != PROTOCOL_ESP)
            {
                ft->v6.protocol = PROTOCOL_IPV6;
            }

            return (PCKT_IPV6);
        }
    }
    else
    {
        /** Overwrite the work entry to keep the info */
        return PARSE_NOT_IP;
    }
}
