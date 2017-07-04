
#ifndef __RT_ETHXX_PARSER_H__
#define __RT_ETHXX_PARSER_H__

#include <stdint.h>

#define PR_DROPONLY    	(1 << 0)
#define PR_IPV4ONLY  	(1 << 1)
#define PR_IPV6ONLY		(1 << 2)

#define PR_IP  			(1 << 3)
#define PR_NOT_IP 		(1 << 4)

#define PR_TCP 			(1 << 5)
#define PR_UDP   		(1 << 6)
#define PR_SCTP    		(1 << 7)

typedef enum{
	DIR_INGRESS,
	DIR_EGRRASE,
	DIR_VAGRANT,
	DIR_MAX,
}STRAME_DIR;

extern int rt_ethxx_packet_parser(void *snapshot, uint8_t *packet,
                          size_t pkt_len, 
                          void __attribute__((__unused__))*argument,
                          size_t __attribute__((__unused__))as /** Argument size*/);


#endif  /* __RV_PROTO_H__ */

