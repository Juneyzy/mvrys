#ifndef __RT_NAMED_MEMORY_H__
#define __RT_NAMED_MEMORY_H__

struct rt_named_hash_t {
	 struct rt_named_hash *next;
	 int  id;
};

struct rt_named_memory_t {
#define NAMED_MEM_DESC_SIZE  64
    char *symbol;
    uint64_t    saddr;
    uint64_t    eaddr;
    uint64_t    avail_size; /** Must <= (eaddr - saddr) */
    struct rt_named_memory_t *next;

    uint32_t    hash;
} ;

extern void *
rt_named_mem_alloc (const char *symbol, const int size);

#endif
