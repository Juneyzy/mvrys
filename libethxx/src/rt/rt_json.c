#include "sysdefs.h"
#include "rt_json.h"

#define DATE_BUFE_LEN  32

struct json_object* rt_entry_to_object(struct rt_throughput_entry *entry, 
    uint64_t router, uint64_t port)
{
    char date[DATE_BUFE_LEN] = {0};
    json_object *outline = NULL;

    if (!entry){
        goto finish;
    }
    
    outline = json_object_new_object();
    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);
    
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_source_data_record(&entry->throughput));
    
finish:
    return outline;
}

size_t rt_entry_to_string(char *result, int len,
    uint64_t router, uint64_t port,
    void *xentry)
{
	size_t rsize = 0;
	char date[DATE_BUFE_LEN] = {0};
	struct rt_throughput_entry *entry = (struct rt_throughput_entry *)xentry;
	struct json_object *outline;
	
	if(!result || !entry)
		goto finish;

	rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);

	outline = json_object_new_object();
	
	json_object_object_add(outline, "timestamp", json_object_new_string(date));
	json_object_object_add(outline, "router", json_object_new_int64(router));
	json_object_object_add(outline, "port", json_object_new_int64(port));
	json_object_object_add(outline, "throughput", web_json_source_data_record(&entry->throughput));
	rsize = snprintf (result, len - 1, "%s\n", json_object_to_json_string(outline));

	json_object_put(outline);
finish:
	return rsize;
}

struct json_object* rt_proto_entry_to_object(struct rt_throughput_proto_entry *entry, 
    uint64_t cmd_type, uint64_t router, uint64_t port)
{
    char date[DATE_BUFE_LEN] = {0};
    json_object *outline = NULL;

    if (!entry){
        goto finish;
    }
    
    outline = json_object_new_object();
    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);
    
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", 
            web_json_protocol_throughput_record(entry, cmd_type));
    
finish:
    return outline;
}

size_t rt_proto_entry_to_string(char *result, int len,
    uint64_t layer, uint64_t router, uint64_t port,
    void *xentry)
{
    size_t rsize = 0;
    char date[DATE_BUFE_LEN] = {0};
    struct rt_throughput_proto_entry *entry = (struct rt_throughput_proto_entry *)xentry;
    struct json_object *outline;
    
    if(!result || !entry)
        goto finish;

    layer++;
    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);

    outline = json_object_new_object();
	
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_protocol_throughput_record(entry, layer));
    rsize = snprintf (result, len - 1, "%s\n", json_object_to_json_string(outline));

    json_object_put(outline);
finish:
    return rsize;
}

struct json_object* rt_msg_entry_to_object(struct rt_throughput_message_entry *entry, 
    uint64_t router, uint64_t port)
{
    char date[DATE_BUFE_LEN] = {0};
    json_object *outline = NULL;

    if (!entry){
        goto finish;
    }
    
    outline = json_object_new_object();
    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);
    
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_message_throughput_record(entry));
    
finish:
    return outline;
}

size_t rt_msg_entry_to_string(char *result, int len,
    uint64_t router, uint64_t port,
    void *xentry)
{
    size_t rsize = 0;
    char date[DATE_BUFE_LEN] = {0};
    struct rt_throughput_message_entry *entry = (struct rt_throughput_message_entry *)xentry;
    struct json_object *outline;
    
    if(!result || !entry)
        goto finish;

    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);

    outline = json_object_new_object();
    
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_message_throughput_record(entry));
    rsize = snprintf (result, len - 1, "%s\n", json_object_to_json_string(outline));

    json_object_put(outline);
finish:
    return rsize;
}

size_t rt_json_throughput_sum_entry_format(char *result, int len,
    uint64_t router, uint64_t port,
    void *xentry)
{
    size_t rsize = 0;
    char date[DATE_BUFE_LEN] = {0};
    struct rt_throughput_summary_entry *entry = (struct rt_throughput_summary_entry *)xentry;
    struct json_object *outline;
    
    if(!result || !entry)
        goto finish;

    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);

    outline = json_object_new_object();
	
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_summary_throughput_record(entry));
    rsize = snprintf (result, len - 1, "%s\n", json_object_to_json_string(outline));

    json_object_put(outline);
finish:
    return rsize;
}

struct json_object* rt_exce_entry_to_object(struct rt_throughput_exce_entry *entry, 
    uint64_t router, uint64_t port)
{
    char date[DATE_BUFE_LEN] = {0};
    json_object *outline = NULL;

    if (!entry){
        goto finish;
    }
    
    outline = json_object_new_object();
    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);
    
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_exception_throughput_record(entry));
    
finish:
    return outline;
}

size_t rt_exce_entry_to_string(char *result, int len,
    uint64_t router, uint64_t port,
    void *xentry)
{
    size_t rsize = 0;
    char date[DATE_BUFE_LEN] = {0};
    struct rt_throughput_exce_entry *entry = (struct rt_throughput_exce_entry *)xentry;
    struct json_object *outline;
    
    if(!result || !entry)
        goto finish;

    rt_tms2str (entry->tm, EVAL_TM_STYLE_FULL, date, DATE_BUFE_LEN);

    outline = json_object_new_object();
	
    json_object_object_add(outline, "timestamp", json_object_new_string(date));
    json_object_object_add(outline, "router", json_object_new_int64(router));
    json_object_object_add(outline, "port", json_object_new_int64(port));
    json_object_object_add(outline, "throughput", web_json_exception_throughput_record(entry));

    rsize = snprintf (result, len - 1, "%s\n", json_object_to_json_string(outline));

    json_object_put(outline);
finish:
    return rsize;
}


