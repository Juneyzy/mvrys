#ifndef __PACKAGE__
#define __PACKAGE__

/*
*   package.h
*   Created by Tsihang <qihang@semptian.com>
*   27 Aug, 2015
*   Func: Data Switch Interface between WEB & DECODER
*   Personal.Q
*/

#include "rt.h"

extern struct json_object *
web_json_source_data_record(struct rt_throughput *throughput);

extern struct json_object *
web_json_protocol_throughput_record(
		struct rt_throughput_proto_entry *rt_proto_entry, int);

extern struct json_object *
web_json_message_throughput_record(
		struct rt_throughput_message_entry *rt_message_entry);

extern struct json_object *
web_json_summary_throughput_record(
		struct rt_throughput_summary_entry *rt_summary_entry);

extern struct json_object *
web_json_exception_throughput_record(
		struct rt_throughput_exce_entry *rt_exce_entry);

#endif
