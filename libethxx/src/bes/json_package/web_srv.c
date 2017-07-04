/*
*   web.c
*   Created by Tsihang <qihang@semptian.com>
*   27 Aug, 2015
*   Func: Data Switch Interface between WEB & DECODER
*   Personal.Q
*/

#include "sysdefs.h"
#include "bes-command.h"
#include "web_json.h"
#include "rt.h"
#include "package.h"
#include "rt_throughput.h"
#include "rt_throughput_proto.h"
#include "rt_throughput_message.h"
#include "rt_throughput_sum.h"
#include "rt_throughput_exce.h"
#include "rt_clue.h"
#include "cluster_decoder.h"

#define default_errno() ERRNO_INVALID_ARGU

int
json_exception_throughput_query(json_hdr *jhdr,
    json_object *body)
{
    json_object *record_array_iobound = json_object_new_array();	

    if (jhdr->cmd != UI_CMD_STATISTICS_EXCEPTION_GET){
        jhdr->error = (-default_errno());
        goto finish;
    }

    jhdr->error = rt_exce_throughput_query(jhdr->router, jhdr->port, jhdr->cmd,
            jhdr->start_ts,
            jhdr->end_ts,
            jhdr->interval, record_array_iobound);


    web_json_body_add(body, NULL, "outline", 
                record_array_iobound, NULL, NULL);

    goto finish;

finish:

    return 0;
}

int
json_summary_throughput_query(json_hdr *jhdr,
    json_object *body)
{
    json_object *record_array_iobound = json_object_new_array();	

    if (jhdr->cmd != UI_CMD_STATISTICS_SUMMARY_GET){
        jhdr->error = (-default_errno());
        goto finish;
    }

    jhdr->error = rt_sum_throughput_query(jhdr->router, jhdr->port, jhdr->cmd,
            jhdr->start_ts,
            jhdr->end_ts,
            jhdr->interval, record_array_iobound);


    web_json_body_add(body, NULL, "outline", 
                record_array_iobound, NULL, NULL);

    goto finish;

finish:

    return 0;
}

int
json_message_throughput_query(json_hdr *jhdr,
    json_object *body)
{
    json_object *record_array_iobound = json_object_new_array();	

    if (jhdr->cmd != UI_CMD_STATISTICS_MESSAGE_GET){
        jhdr->error = (-default_errno());
        goto finish;
    }

    jhdr->error = rt_msg_throughput_query(jhdr->router, jhdr->port, jhdr->cmd,
            jhdr->start_ts,
            jhdr->end_ts,
            jhdr->interval, record_array_iobound);

    web_json_body_add(body, NULL, "outline", 
            record_array_iobound, NULL, NULL);

    goto finish;

finish:

    return 0;
}

int
json_protocol_throughput_query(json_hdr *jhdr,
    json_object *body)
{
    json_object *record_array_iobound = json_object_new_array();	

    if (jhdr->cmd != UI_CMD_STATISTICS_PROTOCOL_GET){
        jhdr->error = (-default_errno());
        goto finish;
    }
	
    jhdr->error = rt_proto_throughput_query(jhdr->router, jhdr->port, jhdr->sub_cmd,
            jhdr->start_ts,
            jhdr->end_ts,
            jhdr->interval, record_array_iobound);

    web_json_body_add(body, NULL, "outline", 
        record_array_iobound, NULL, NULL);

    goto finish;

finish:

    return 0;
}

int
json_rt_throughput_query(json_hdr *jhdr,
    json_object *body)
{
    json_object *record_array_iobound = json_object_new_array();	

    if (jhdr->cmd != UI_CMD_STATISTICS_RT_GET){
        jhdr->error = (-default_errno());
        goto finish;
    }
    
    jhdr->error = rt_throughput_query(jhdr->router, jhdr->port, jhdr->cmd,
            jhdr->start_ts,
            jhdr->end_ts,
            jhdr->interval, record_array_iobound);

    web_json_body_add(body, NULL, "outline", 
        record_array_iobound, NULL, NULL);

    goto finish;

finish:

    return 0;
}

int
json_captor_ctrl(json_hdr *jhdr,
    json_object *body)
{
    switch(jhdr->cmd){
        case UI_CMD_START:
            break;
        case UI_CMD_RESTART:
            break;
        case UI_CMD_STOP:
            break;
        default:
            jhdr->error = (-default_errno());
            goto finish;
    }

finish:
    //snprintf(des, 63, "Captor %s ", ctrl == 1 ? "Started": (ctrl == 0 ? "Stopped" : "Restarted" ));
//web_json_body_add(body, des, NULL, NULL, NULL, NULL);

    return 0;
}


static void
json_clue_email_further_proc(struct json_object *items, int action)
{
    printf("step into %s, items=%d, action=%d\n", __func__, json_object_array_length(items), action);
    int count = 0;
#define ALLOWED_STR_SIZ 63
    uint64_t id = 0, lifttime = 0;
    char subtype[ALLOWED_STR_SIZ + 1] =  {0};
    char protocol[ALLOWED_STR_SIZ + 1] =  {0};
    char mailaddr[ALLOWED_STR_SIZ + 1] =  {0};
    char keyword[ALLOWED_STR_SIZ + 1] =  {0};
    char match_method[ALLOWED_STR_SIZ + 1] =  {0};
    char kwpl[ALLOWED_STR_SIZ + 1] =  {0};
    
    if (action == MD_ADD){
         for(count = 0; count < json_object_array_length(items); count ++){
            struct json_object *item = NULL;
            item = json_object_array_get_idx(items, count);
            json_field(item, "id", (void*)&id, sizeof(typeof(id)));
            json_field(item, "lifttime", (void*)&lifttime, sizeof(typeof(lifttime)));
            json_field(item, "protocol", (void*)&protocol, ALLOWED_STR_SIZ);
            json_field(item, "subtype", (void*)&subtype, ALLOWED_STR_SIZ);
            json_field(item, "mailaddr", (void*)&mailaddr, ALLOWED_STR_SIZ);
            json_field(item, "keyword", (void*)&keyword, ALLOWED_STR_SIZ);
            json_field(item, "match_method", (void*)&match_method, ALLOWED_STR_SIZ);
            json_field(item, "kwpl", (void*)&kwpl, ALLOWED_STR_SIZ);
            
            printf ("%ld, %ld, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n", id, lifttime, protocol, subtype, mailaddr, match_method, keyword, kwpl);
            rt_clue_mail_add(id, subtype, protocol, mailaddr, keyword, match_method, lifttime, kwpl);
            /** translit to a rpc string and send to decoders if needed */
            /** */
            //if (decoder_get_local()->type == DT_MASTER){
                //decoder_get_local()->broadcast_to_decoders(qstr, s);
                //;
            //}
         }
    }

    if (action == MD_DEL){

    }

    if (action == MD_CTRL){


    }

    if (action == MD_NCTRL){

    }
    
}

static void
json_clue_ipport_further_proc(struct json_object *items, int action)
{
    printf("step into %s\n", __func__);
}

static void (*clue_further_proc[CLUE_UNUSED])(struct json_object *items, int method);

int json_clue_proc(const char *qstr, ssize_t s, 
    json_hdr *jhdr,
    json_object *obody,
    json_object *injson)
{
    json_object *inbody = NULL;
    json_object *array_list = NULL, *clueset = NULL, *clue = NULL, *items = NULL;
    int array_cnt = 0;
    int xret = -1;
    #define CLUE_STR_SIZ    63
    char type_str[CLUE_STR_SIZ + 1] = {0};
    char action_str[CLUE_STR_SIZ + 1] = {0};

    if (jhdr->cmd != UI_CMD_RULE){
        jhdr->error = (-default_errno());
        goto finish;
    }

    if (!(inbody = web_json_to_field(injson, "body")))
        goto finish;
    
    if (!(clueset = web_json_to_field(inbody, "clueset")))
        goto finish;
    
    for(array_cnt = 0; array_cnt < json_object_array_length(clueset); array_cnt ++){
        
        if (!(array_list = json_object_array_get_idx(clueset, array_cnt)))
            goto finish;
        
        if (!(clue = web_json_to_field(array_list, "clue")))
            goto finish;
        
        json_field(clue, "type", (void*)type_str, CLUE_STR_SIZ);
        json_field(clue, "action", (void*)action_str, CLUE_STR_SIZ);
        printf("type_str=%s, action_str=%s\n", type_str, action_str);
        
        if (!(items = web_json_to_field(array_list, "item")))
            goto finish;

        int clue_type = rt_clue_str2type(type_str, strlen(type_str));
        if (clue_type != CLUE_UNUSED)
            clue_further_proc[clue_type%CLUE_UNUSED](items, 
                rt_clue_str2action(action_str, (ssize_t)strlen(action_str)));
    }
    
#if 0
    xret = rt_rule_body_parser((void *)web_req);
    if (!xret){
        xret = rt_clue_process();
    }else{
        rt_log_debug("Parsing json format issued rule fails\n");
    }
#endif
    jhdr->error = xret;

finish:
    return 0;
}

int
web_serv_init()
{
    clue_further_proc[CLUE_EMAIL]=&json_clue_email_further_proc;
    clue_further_proc[CLUE_IPPORT]=&json_clue_ipport_further_proc;
    
    MC_SET_DEBUG(1);
    return 0;
}

