/*
*   rt_mempool.c
*   Created by Tsihang <qihang@semptian.com>
*   26 Mar, 2016
*   Func: Memory Pool for fast allocation
*   Personal.Q
*/

#ifndef __RT_MEMPOOL_H__
#define __RT_MEMPOOL_H__


#include "rt_sync.h"
#include "rt_list.h"

typedef unsigned gfp_t;

typedef void * (mempool_alloc_t)(gfp_t gfp_mask, void *pool_data);
typedef void (mempool_free_t)(void *element, void *pool_data);

struct mempool_t {
    
    rt_mutex lock;

    int min_nr;		/* nr of elements at *elements */

    int curr_nr;		/* Current nr of elements at *elements */

    void **elements;

    void *pool_data;

    mempool_alloc_t *alloc;

    mempool_free_t *free;

    int flags;
    
} ;

extern struct mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data);
extern struct mempool_t *mempool_create_node(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data, int nid);

extern int mempool_resize(struct mempool_t *pool, int new_min_nr, gfp_t gfp_mask);
extern void mempool_destroy(struct mempool_t *pool);
extern void * mempool_alloc(struct mempool_t *pool, gfp_t gfp_mask);
extern void mempool_free(void *element, struct mempool_t *pool);

#endif  /** END OF __RT_MEMPOOL_H__ */
