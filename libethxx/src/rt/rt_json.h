#ifndef __RT_JSON_H__
#define __RT_JSON_H__

#include "json.h"
#include "package.h"

static __rt_always_inline__ void
rt_json_throughput_add (struct json_object * jo, const char *section, union rt_io *io)
{
    if (!strncmp("bs", section, 2) || !strncmp("ps", section, 2)){
        json_object_object_add(jo, "in", json_object_new_int64(io->in.iv));
        json_object_object_add(jo, "out", json_object_new_int64(io->out.iv));
        json_object_object_add(jo, "total", json_object_new_int64(io->total.iv));
    }
    else
    {
        json_object_object_add(jo, "in", json_object_new_double(io->in.fv));
        json_object_object_add(jo, "out", json_object_new_int64(io->out.fv));
        json_object_object_add(jo, "total", json_object_new_int64(io->total.fv));
    }
    
    return;
}

extern struct json_object* rt_entry_to_object(struct rt_throughput_entry *entry, 
    uint64_t, uint64_t);
extern size_t rt_entry_to_string(char *, int,
    uint64_t, uint64_t,
    void *xentry);

struct json_object* rt_proto_entry_to_object(struct rt_throughput_proto_entry *entry, 
    uint64_t , uint64_t, uint64_t);
extern size_t rt_proto_entry_to_string(char *, int, uint64_t, 
    uint64_t, uint64_t,
    void *);

extern struct json_object* rt_msg_entry_to_object(struct rt_throughput_message_entry *entry, 
    uint64_t router, uint64_t port);
extern size_t rt_msg_entry_to_string(char *, int ,
    uint64_t , uint64_t ,
    void *);

extern size_t rt_json_throughput_sum_entry_format(char *, int ,
    uint64_t , uint64_t ,
    void *);

extern size_t rt_exce_entry_to_string(char *, int ,
    uint64_t , uint64_t ,
    void *);
extern struct json_object* rt_exce_entry_to_object(struct rt_throughput_exce_entry *entry, 
    uint64_t router, uint64_t port);

#endif
