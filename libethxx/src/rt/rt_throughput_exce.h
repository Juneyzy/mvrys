#ifndef __RT_THROUGHPUT_EXCE_H__
#define __RT_THROUGHPUT_EXCE_H__

#include <stdint.h>
#include "rt_sync.h"
#include "rt.h"
#include "json.h"

struct rt_throughput_exce_list{
#define PATH_SIZE   128
    char path[PATH_SIZE];
    uint64_t router;
    uint64_t port;
    rt_mutex  entries_mtx;
    struct rt_throughput_exce_entry *head, *tail;
    int count;

    /** for other routers */
    struct rt_throughput_exce_list * next, *prev;
};

struct rt_throughput_exce_mgmt{
#define PATH_SIZE   128
    char default_path[PATH_SIZE];
    struct rt_throughput_exce_list *cache;
    uint64_t flush_threshold;
    uint64_t sample_threshold;
};

extern void
rt_throughput_exce_init();

extern int rt_exce_throughput_query(uint64_t ,
    uint64_t ,int ,uint64_t ,
	uint64_t ,uint64_t , json_object *record_array_iobound);


#endif
