/*
*   web_json.h
*   Created by Tsihang <qihang@semptian.com>
*   27 Aug, 2015
*   Func: Data Switch Interface between WEB & DECODER
*   Personal.Q
*/
#ifndef __WEB_JSON_H__
#define __WEB_JSON_H__

#include "json.h"

enum rt_throughput_stage{
    /** real time statistics stage 0 */
    STAT_RT_STAGE0,
    /** real time statistics stage 1 */
    STAT_RT_STAGE1,
    /** history statistics */
    STAT_HI_STATG0
};

typedef struct
{
#define JSON_BUF_LEN 32
    char ver[JSON_BUF_LEN];
    uint64_t router;
    uint64_t port;
    uint64_t dir;
    int64_t error;
    uint64_t cmd;
    uint64_t sub_cmd;
    uint64_t seq;
    uint64_t start_ts;
    uint64_t end_ts;
    uint64_t interval;
}json_hdr;

#define JSON_DEBUG
#define json_printf(fmt,args...) printf(fmt,##args)

static inline void
web_json_type_parse(
    json_object *jso,
    void *data, size_t dsize)
{
    json_bool jb;
    int32_t ji;
    const char *str;

    switch(json_object_get_type(jso)){
        case json_type_boolean:
            jb = json_object_get_boolean(jso);
            *(json_bool *)data = jb;
            break;
        case json_type_string:
            str = json_object_get_string(jso);
            memcpy(data, str, dsize > strlen(str)? strlen(str):dsize);
            break;
        case json_type_int:
            ji = json_object_get_int(jso);
            *(int32_t *)data = ji;
            break;
         default:
            break;
    }

    return;
}

static inline json_object *
web_json_to_field(
    json_object *new_obj,
    const char *field)
{
    return json_object_object_get(new_obj, field);
}

#define json_field(cobj, field, variable, variable_len) web_json_type_parse(\
    web_json_to_field(cobj, field),\
        variable, variable_len);

static inline json_object *
web_json_tokener_parse(const char *desc,
	const char *json_str)
{
    enum json_tokener_error errors;

    json_object *jo = json_object_new_object();
    jo = json_tokener_parse_verbose(json_str, &errors);
    json_printf("length=%d, errors=%d\n %s.to_string()=%s\n", 
        (int)strlen(json_str), errors, desc, json_object_to_json_string(jo));
    return jo;
}

static inline void
web_json_element(json_hdr *jhdr)
{
#ifdef JSON_DEBUG
    json_printf("\tversion=\"%s\"\n", jhdr->ver);
    json_printf("\tcommand=%lu\n", jhdr->cmd);
    json_printf("\tsub_command=%lu\n", jhdr->sub_cmd);
    json_printf("\tsequence=%lu\n", jhdr->seq);
    json_printf("\terrno=%lu\n", jhdr->error);
    json_printf("\tstart_ts=%lu\n", jhdr->start_ts);
    json_printf("\tend_ts=%lu\n", jhdr->end_ts);
    json_printf("\tinterval=%lu\n", jhdr->interval);
#endif
}


static inline void
web_json_parser(
    const char *jstr,
    json_hdr *jhdr,
    json_object **injson)
{
    json_object *head;

    *injson = web_json_tokener_parse("JSONSTR", jstr);
    head = web_json_to_field(*injson, "head");

    json_field(head, "version", (void*)&jhdr->ver[0], JSON_BUF_LEN);
    json_field(head, "router", (void*)&jhdr->router, sizeof(typeof(jhdr->router)));
    json_field(head, "port", (void*)&jhdr->port, sizeof(typeof(jhdr->port)));
    json_field(head, "direction", (void*)&jhdr->dir, sizeof(typeof(jhdr->dir)));
    json_field(head, "command", (void*)&jhdr->cmd, sizeof(typeof(jhdr->cmd)));
    json_field(head, "sub_command", (void*)&jhdr->sub_cmd, sizeof(typeof(jhdr->sub_cmd)));
    json_field(head, "sequence", (void*)&jhdr->seq, sizeof(typeof(jhdr->seq)));
    json_field(head, "errno", (void*)&jhdr->error, sizeof(typeof(jhdr->error)));
    json_field(head, "start_ts", (void*)&jhdr->start_ts, sizeof(typeof(jhdr->start_ts)));
    json_field(head, "end_ts", (void*)&jhdr->end_ts, sizeof(typeof(jhdr->end_ts)));
    json_field(head, "interval", (void*)&jhdr->interval, sizeof(typeof(jhdr->interval)));

    web_json_element(jhdr);

}

static inline int
web_json_record_array_add(json_object *array,
    json_object *val)
{
    return json_object_array_add(array, val);
}

static inline void
web_json_body_add(json_object *body,
    const char *body_desc __attribute__((__unused__)),
    const char *set_desc1,
    json_object *set1,
    const char *set_desc2,
    json_object *set2)
{
    //json_object_object_add(body, "description", json_object_new_string(body_desc));

    if(set_desc1 && set1)
        json_object_object_add(body, set_desc1, set1);
    if(set_desc2 && set2)
        json_object_object_add(body, set_desc2, set2);
}


static inline  void
web_json_head_add(json_object *head,
    json_hdr *jhdr,
    json_object *body)
{
    json_object *hdr = json_object_new_object();
    json_object_object_add(hdr,   "version", json_object_new_string(jhdr->ver));
    json_object_object_add(hdr,   "router", json_object_new_int64(jhdr->router));
    json_object_object_add(hdr,   "port", json_object_new_int64(jhdr->port));
    json_object_object_add(hdr,   "direction", json_object_new_int64(jhdr->dir));
    json_object_object_add(hdr,   "command", json_object_new_int64(jhdr->cmd));
    json_object_object_add(hdr,   "sub_command", json_object_new_int64(jhdr->sub_cmd));
    json_object_object_add(hdr,   "sequence", json_object_new_int64(jhdr->seq));
    json_object_object_add(hdr,   "errno", json_object_new_int64(jhdr->error));
    json_object_object_add(hdr,   "start_ts", json_object_new_int64(jhdr->start_ts));
    json_object_object_add(hdr,   "end_ts", json_object_new_int64(jhdr->end_ts));
    json_object_object_add(hdr,   "interval", json_object_new_int64(jhdr->interval));
    
    json_object_object_add(head,    "head",     hdr);
    json_object_object_add(head,    "body",     body);

    return;
}

/** Java will not get data without '\n' */
static inline int
web_json_data_rebuild(const char *data, 
    size_t size, 
    char **odata, 
    size_t *osize)
{
    size_t real_size = size + 2; /** 2, '\n' + '\0' */
    
    if (!data || !size) 
        return -1;
    
    *odata = malloc (real_size);
    assert(*odata);
    memset (*odata, 0, real_size);
    snprintf(*odata, real_size,"%s\n", data);
    *osize = real_size;
    
    return 0;
}

#if 1
static inline enum rt_throughput_stage
json_subcmd_to_stage(uint64_t sub_cmd)
{
    return (enum rt_throughput_stage)sub_cmd;
}
#endif

#endif
