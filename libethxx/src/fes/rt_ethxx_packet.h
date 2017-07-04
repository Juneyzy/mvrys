#ifndef __RT_ETHXX_PACKET_H__
#define __RT_ETHXX_PACKET_H__

#include "rt_config.h"
#include "rt_ethxx_pcap.h"
#include "rt_ethxx_proto.h"

union call_uri_t {
	struct {
		uint32_t	sr155_id: 6;
		uint32_t	e1_id: 6;
		uint32_t	ts_id: 5;
		uint32_t	resv0_0: 15;
	};

	struct {
		uint32_t	source_sys_id;
	};

	uint32_t	data;
};

union call_link_t {
	
	struct {

		/** WORD 0 --> CALLID */
		uint32_t	tm;

		/** timestamp */
		union call_uri_t	uri;

	};

	uint64_t    data;
};


union vrs_fsnapshot_t {
	struct {

		/** WORD 0 --> CALLID */
		union call_link_t call;
		
		/** WORD 1 */
		uint64_t	dir: 4;	/** 1, 2 */
		uint64_t	case_id: 4;	/** 0, 1 */
		uint64_t	code_method: 4;
		uint64_t	resv1_0:	8;
		uint64_t	mark:4;		/** FIN: 0x02, DATA: 0x03 */
		uint64_t	dsn:	8;
		uint64_t	payload_size:16; /** real voice data length for this packet */
		uint64_t   resv1_1:12;     
	};
	uint64_t    data[2];
};

struct rt_packet_snapshot_t{
    u_char      	*packet;
    size_t        	size;
    uint8_t      	type; 		/* packet type: IPV4(4) | IPV6(6) */
    ip5tuple_st     ft;      	/* fivetuple */
    uint64_t        timestamp; 	/* timestamp (ns) */

    uint8_t rid;
    uint8_t pid;
    uint8_t dir;

    int source_sys;
	
    union vrs_fsnapshot_t call_snapshot;

    int flags;

    int parse_result;
};

struct rt_packet_t{
/** Packet size */
#define SNAP_LEN    4096
    struct pcap_pkthdr pkthdr;
    struct rt_packet_snapshot_t intro;
    void *buffer;
    /** The length of packet buffer, not the valid length */
    int buffer_size;

    int ready;
    int flags;
};

#endif
