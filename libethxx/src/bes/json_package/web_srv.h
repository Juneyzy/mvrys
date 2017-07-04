#ifndef __WEB_H__
#define __WEB_H__

int web_serv_init();

int json_captor_ctrl(void *, void*);
int json_rt_throughput_query(void *, void*);
int json_protocol_throughput_query(void *, void*);
int json_message_throughput_query(void *, void*);
int json_summary_throughput_query(void *, void*);
int json_exception_throughput_query(void *, void*);
int json_clue_proc(const char *, ssize_t, void *,void *,void *);

#endif
