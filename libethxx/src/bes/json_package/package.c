/*
*   json_package.c
*   Created by Tsihang <qihang@semptian.com>
*   27 Aug, 2015
*   Func: Data Switch Interface between WEB & DECODER
*   Personal.Q
*/

#include "sysdefs.h"
#include "web_json.h"
#include "rt.h"

static inline struct json_object *
web_json_data_record(union rt_io io)
{
    json_object *eval_obj = json_object_new_object();

    io = io;
    json_object_object_add(eval_obj, "inb", json_object_new_int64(io.in.iv));
    json_object_object_add(eval_obj, "outb", json_object_new_int64(io.out.iv));
    json_object_object_add(eval_obj, "total", json_object_new_int64(io.total.iv));

    return eval_obj;
}

struct json_object *
web_json_source_data_record(struct rt_throughput *throughput)
{
    json_object *eval_obj = json_object_new_object();

    web_json_body_add(eval_obj, NULL,
    	"bs", web_json_data_record(throughput->bs),
    	"ps", web_json_data_record(throughput->ps));
    web_json_body_add(eval_obj, NULL, 
    	"bps", web_json_data_record(throughput->bps),
    	"pps", web_json_data_record(throughput->pps));
    web_json_body_add(eval_obj, NULL,
    	"bw_usage", web_json_data_record(throughput->bw_usage), NULL, NULL);

    return eval_obj;
}

static inline struct json_object *
web_json_sum_protocol_data_record(union rt_io *io_session)
{
    json_object *eval_obj = json_object_new_object();
    char *protocol[SUMMARY_PROTOCOL] = {"total", "data_link_layer", "network_layer", \
    		"transport_layer", "application_layer"};
    int i = 0;

    for(i=0; i < SUMMARY_PROTOCOL; i++){
        web_json_body_add(eval_obj, NULL, 
        	protocol[i], web_json_data_record(io_session[i]), NULL, NULL);
    }

    return eval_obj;
}

static inline struct json_object *
web_json_sum_session_data_record(union rt_io *io_session)
{
    json_object *eval_obj = json_object_new_object();
    char *session[SUMMARY_SESSION] = {"physical_layer", "network_layer", "tcp", "udp"};
    int i = 0;

    for (i=0; i < SUMMARY_SESSION; i++){
    	web_json_body_add(eval_obj, NULL,
    		session[i], web_json_data_record(io_session[i]), NULL, NULL);
    }

    return eval_obj;
}

static inline struct json_object *
web_json_protocol_data_link_layer(struct rt_throughput *throughput_link)
{
    json_object *eval_obj = json_object_new_object();
    char *data_link[DATA_LINK_LAYER] = {"arp", "rarp", "aarp", "pppoe", "vlan", "stp", "mpls", 
    	"ethernet_ii", "ethernet_8022", "ethernet_8023", "ethernet_snap"};
    int i = 0;

    for (i=0; i < DATA_LINK_LAYER; i++){
    	web_json_body_add(eval_obj, NULL,
    		data_link[i], web_json_source_data_record(&throughput_link[i]), NULL, NULL);
    }

    return eval_obj;
}

static inline struct json_object *
web_json_protocol_network_layer(struct rt_throughput *throughput_net)
{
    json_object *eval_obj = json_object_new_object();
    char *network[NETWORK_LAYER] = {"cgmp", "eigrp", "egp", "gre", "icmp", "icmpv6", "igmp", \
    	"ip", "ip_fragment", "ospf", "rip", "rip1", "rip2", "rip3", "rip4", "gdp"};
    int i = 0;

    for (i=0; i < NETWORK_LAYER; i++){
    	web_json_body_add(eval_obj, NULL, 
    		network[i], web_json_source_data_record(&throughput_net[i]), NULL, NULL);
    }

    return eval_obj;
}

static inline struct json_object *
web_json_protocol_transport_layer(struct rt_throughput *throughput_trans)
{
    json_object *eval_obj = json_object_new_object();
    char *transport[TRANSPORT_LAYER] = {"h225", "rtcp", "ssh", "tcp", "udp", "netbeui", "sctp"};
    int i = 0;

    for (i=0; i < TRANSPORT_LAYER; i++){
    	web_json_body_add(eval_obj, NULL, 
    		transport[i], web_json_source_data_record(&throughput_trans[i]), NULL, NULL);
    }

    return eval_obj;
}

static inline struct json_object *
web_json_protocol_application_layer(struct rt_throughput *throughput_app)
{
    json_object *eval_obj = json_object_new_object();
    char *application[APPLICATION_LAYER] = {"ftp_ctrl", "ftp_data", "ftp", "dns", "http", "https", \
    	"ntp", "rtp", "snmp", "telnet", "pop2", "pop3", "pop3s"};
    int i = 0;

    for (i=0; i < APPLICATION_LAYER; i++){
    	web_json_body_add(eval_obj, NULL, 
    		application[i], web_json_source_data_record(&throughput_app[i]), NULL, NULL);
    }

    return eval_obj;
}

struct json_object *
web_json_protocol_throughput_record(struct rt_throughput_proto_entry *rt_proto_entry,
        uint64_t layer)
{
    json_object *eval_obj = json_object_new_object();
    
    if((layer == RT_PROTO_DATA_LINK) ||
        (layer == RT_PROTO_ALL_LAYER)){
        web_json_body_add(eval_obj, NULL, 
            "data_link_layer", web_json_protocol_data_link_layer(rt_proto_entry->throughput_link), 
            NULL, NULL);
    }

    if((layer == RT_PROTO_NETWORK) ||
        (layer == RT_PROTO_ALL_LAYER)){
       web_json_body_add(eval_obj, NULL, 
    	    "network_layer", web_json_protocol_network_layer(rt_proto_entry->throughput_net), 
    	      NULL, NULL);	
    }

    if((layer == RT_PROTO_TRANSPORT) ||
        (layer == RT_PROTO_ALL_LAYER)){        
        web_json_body_add(eval_obj, NULL,
            "transport_layer", web_json_protocol_transport_layer(rt_proto_entry->throughput_trans),
             NULL, NULL);
    }

    if((layer == RT_PROTO_APPLICATION) ||
        (layer == RT_PROTO_ALL_LAYER)){        
        web_json_body_add(eval_obj, NULL,
            "application_layer", web_json_protocol_application_layer(rt_proto_entry->throughput_app),
              NULL, NULL);
    }
    
    return eval_obj;
}

struct json_object *
web_json_message_throughput_record(struct rt_throughput_message_entry *rt_message_entry)
{
    int i = 0;
    json_object *eval_obj = json_object_new_object();
    char *pakets[MESSAGE_LEN_THROUGHPUT] = {"packet64", "packet65_127", "packet128_255",
        "packet256_511", "packet512_1023", "packet1024_1517", "packet1518"};

    for (i = 0; i< MESSAGE_LEN_THROUGHPUT; i++){
        web_json_body_add(eval_obj, NULL, pakets[i],            
            web_json_source_data_record(&rt_message_entry->throughput[i]),
            NULL, NULL);
    }
    
    return eval_obj;
}

struct json_object *
web_json_summary_throughput_record(struct rt_throughput_summary_entry *rt_sum_entry)
{
    json_object *eval_obj = json_object_new_object();

    web_json_body_add(eval_obj, NULL, 
    	"session", 
    	web_json_sum_session_data_record(rt_sum_entry->rt_io_session), 
    	"protocol", 
    	web_json_sum_protocol_data_record(rt_sum_entry->rt_io_protocol));

    return eval_obj;
}

struct json_object *
web_json_exception_throughput_record(struct rt_throughput_exce_entry *rt_exce_entry)
{
    int i = 0;
    json_object *eval_obj = json_object_new_object();

    char *exce[EXCE_MSG_THROUGHPUT] = {"local_flow", "life_cycle", "arp_broadcast", "icmp",
                "snmp", "telnet", "ssh", "ftp", "tftp", "http"};

    for (i = 0; i< EXCE_MSG_THROUGHPUT; i++){
        web_json_body_add(eval_obj, NULL, exce[i],            
            web_json_source_data_record(&rt_exce_entry->throughput[i]),
            NULL, NULL);
    }
    
    return eval_obj;
}


