#include <stdint.h>
#include <string.h>

#include "rt_stdlib.h"
#include "rt_logging.h"
#include "rt_named_memory.h"

#define MAX_CONFLICT_CHAIN_SIZ  1023
static uint64_t __mm_node_conflict_chain[MAX_CONFLICT_CHAIN_SIZ + 1] = {0};

static inline uint32_t 
rt_hash_str(const char * str, 
    int count) 
{ 
#define c_mul(a, b) (a * b & 0xFFFFFFFF) 
	unsigned long value = str[0] << 7; 
	unsigned long len = 0; 
	
	while(*str != 0) 
	{ 
		value = c_mul(1000003, value) ^ *str++; 
		len++; 
	} 

	value = value ^ len; 
	if (value == (uint32_t)-1) 
		value = (uint32_t)-2; 
	
	return value % count; 
} 

static inline struct rt_named_memory_t *
rt_named_mem_lookup(const char *symbol)
{
    uint8_t search_depth = 0;
    struct rt_named_memory_t *entry = NULL, *find = NULL;
    uint32_t hash = rt_hash_str(symbol, MAX_CONFLICT_CHAIN_SIZ);
  
    /** Get the subscription entry  */
    entry = (struct rt_named_memory_t *)(* (__mm_node_conflict_chain + hash));
    
    while (entry != NULL){
        if (strncmp(symbol, entry->symbol, NAMED_MEM_DESC_SIZE)){
            entry = entry->next;
            if (search_depth ++ > 8) 
                rt_log_warning(ERRNO_MEMBLK_LOOKUP, "The search depth (%d) is too high ... \n", search_depth);
            continue;
        }
        
        find = entry;
        goto finish;
    }

finish:
    return find;
}

static inline void *
rt_named_mem_hash_add(struct rt_named_memory_t  *rt_named_mem_entry)
{
    uint32_t hash = (rt_named_mem_entry->hash % MAX_CONFLICT_CHAIN_SIZ);
    struct rt_named_memory_t *entry, *first_node;

    /** Get the subscription entry */
    first_node = entry = (struct rt_named_memory_t *)(*(__mm_node_conflict_chain + hash));
    while (entry) {
        if (!strncmp(rt_named_mem_entry->symbol, entry->symbol, NAMED_MEM_DESC_SIZE)) {
            rt_log_warning(ERRNO_MEMBLK_LOOKUP, "Can not add, %s exsit\n", rt_named_mem_entry->symbol);
            free ((void *)rt_named_mem_entry->saddr);
            free (rt_named_mem_entry);
            return (void *)entry->saddr;
        }
        entry = entry->next;
    }

    *(__mm_node_conflict_chain + hash) = (uint64_t)rt_named_mem_entry;
    rt_named_mem_entry->next = first_node;

    return (void *)rt_named_mem_entry->saddr;
}

static inline struct rt_named_memory_t *
rt_named_mem_create ()
{
    struct rt_named_memory_t *rt_named_mem = NULL;
 
    rt_kmalloc((void **)&rt_named_mem, sizeof (struct rt_named_memory_t));
    if (!rt_named_mem){
        rt_log_warning(ERRNO_MEM_ALLOC, "Can not alloc, %s\n", strerror(errno));
        goto finish;
    }

    /** Initialize the memory block */
    rt_named_mem->avail_size   =   0;
    rt_named_mem->saddr        =   0;
    rt_named_mem->eaddr        =   0;
    rt_named_mem->next         =   NULL;

finish:    
    return rt_named_mem;
}

void * rt_named_memblock_getalloc(char *desc, 
    uint64_t size, 
    uint64_t __attribute__((__unused__))alignment)
{
    struct rt_named_memory_t *rt_named_mem = NULL;
    void *memory = NULL, *chk = NULL;

    rt_named_mem = rt_named_mem_lookup (desc);
    if (rt_named_mem) {
        rt_log_warning(ERRNO_MEMBLK_ALLOC, "%s exsit\n", desc);
        memory = (void *)rt_named_mem->saddr;
        goto finish;
    }
   
    rt_named_mem = rt_named_mem_create();
    if (!rt_named_mem) {
        goto finish;
    }

    rt_kmalloc((void **)&chk, size);
    if (!chk){
        rt_log_warning(ERRNO_MEM_ALLOC, "Can not alloc, %s\n", strerror(errno));
        goto finish;
    }
    
    rt_named_mem->symbol = strdup(desc);
    rt_named_mem->saddr = (uint64_t) chk;
    rt_named_mem->eaddr = rt_named_mem->saddr + rt_named_mem->avail_size;
    rt_named_mem->avail_size   =   size;
    rt_named_mem->hash = rt_hash_str(rt_named_mem->symbol, MAX_CONFLICT_CHAIN_SIZ);
    
    memory = rt_named_mem_hash_add(rt_named_mem);

finish:
    return memory;
}

void rt_named_memblock_free(char *desc)
{
    struct rt_named_memory_t *mnode;
    
    mnode = rt_named_mem_lookup(desc);
    if (mnode) {
        free((void *) mnode->symbol);
        free((void *) mnode->saddr);
        free((void *) mnode);
    }
}

void *rt_named_memblock_get(char *desc)
{
    struct rt_named_memory_t *mnode = NULL;
    void *memory = NULL;
    
    mnode = rt_named_mem_lookup(desc);
    if (mnode)
        memory = (void *) mnode->saddr;

    return memory;
}

void __attribute__((__unused__))
rt_named_memblock_test()
{
    struct rt_named_memory_t *mnode;
    mnode = (struct rt_named_memory_t *) rt_named_memblock_getalloc("xxx", sizeof(struct rt_named_memory_t), 128);
    if (!mnode) return;
    mnode->saddr = 1000;
    mnode->eaddr = 1002;
    mnode = NULL;
    mnode = (struct rt_named_memory_t *) rt_named_memblock_get("xxx");
    printf("%ld %ld\n", mnode->saddr, mnode->eaddr);
}