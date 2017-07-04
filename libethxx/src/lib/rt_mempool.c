/*
*   rt_mempool.c
*   Created by Tsihang <qihang@semptian.com>
*   26 Mar, 2016
*   Func: Memory Pool for fast allocation
*   Personal.Q
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rt_common.h"
#include "rt_stdlib.h"
#include "rt_mempool.h"
#include "rt_bug.h"

static void add_element(struct mempool_t *pool, void *element)
{
	BUG_ON(pool->curr_nr >= pool->min_nr);
	pool->elements[pool->curr_nr++] = element;
}

static void *remove_element(struct mempool_t *pool)
{
	BUG_ON(pool->curr_nr <= 0);
	return pool->elements[--pool->curr_nr];
}

static void free_pool(struct mempool_t *pool)
{
	while (pool->curr_nr) {
		void *element = remove_element(pool);
		pool->free(element, pool->pool_data);
	}
	kfree(pool->elements);
	kfree(pool);
}

struct mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
				mempool_free_t *free_fn, void *pool_data)
{
	return  mempool_create_node(min_nr,alloc_fn,free_fn, pool_data,-1);
}

struct mempool_t *mempool_create_node(int min_nr, mempool_alloc_t *alloc_fn,
			mempool_free_t *free_fn, void *pool_data, int node_id)
{
	struct mempool_t *pool;
    
       pool = (struct mempool_t *)kmalloc(sizeof(struct mempool_t), MPF_CLR, node_id);
       if(unlikely(!pool)){
            goto finish;
       }
       
       pool->elements = kmalloc(min_nr * sizeof(void *),
		MPF_CLR, node_id);
       if (!pool->elements) {
	    kfree(pool);
	    goto finish;
	}
       
	rt_mutex_init(&pool->lock, NULL);
	pool->min_nr = min_nr;
	pool->pool_data = pool_data;
	pool->alloc = alloc_fn;
	pool->free = free_fn;

	/*
	 * First pre-allocate the guaranteed number of buffers.
	 */
	while (pool->curr_nr < pool->min_nr) {
		void *element;

		element = pool->alloc(0, pool->pool_data);
		if (unlikely(!element)) {
			free_pool(pool);
			return NULL;
		}
		add_element(pool, element);
	}
    
finish:
	return pool;
}

void mempool_destroy(struct mempool_t *pool)
{
	/* Check for outstanding elements */
	BUG_ON(pool->curr_nr != pool->min_nr);
	free_pool(pool);
}

void * mempool_alloc(struct mempool_t *pool, 
                            gfp_t __attribute__((__unused__))gfp_mask)
{
	void *element;
        gfp_t gfp_temp = 0;
    
repeat_alloc:

	rt_mutex_lock(&pool->lock);
	if (likely(pool->curr_nr)) {
		element = remove_element(pool);
		rt_mutex_unlock(&pool->lock);
		return element;
	}
	rt_mutex_unlock(&pool->lock);

	element = pool->alloc(gfp_temp, pool->pool_data);
	if (likely(element != NULL))
		return element;

	goto repeat_alloc;
}

/**
 * mempool_free - return an element to the pool.
 * @element:   pool element pointer.
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 *
 * this function only sleeps if the free_fn() function sleeps.
 */
void mempool_free(void *element, struct mempool_t *pool)
{
	if (unlikely(element == NULL))
		return;
    
	if (pool->curr_nr < pool->min_nr) {
		rt_mutex_lock(&pool->lock);
		if (pool->curr_nr < pool->min_nr) {
			add_element(pool, element);
			rt_mutex_unlock(&pool->lock);
			return;
		}
		rt_mutex_lock(&pool->lock);
	}
	pool->free(element, pool->pool_data);
}

#define RT_MEMPOOL_TEST
#ifdef RT_MEMPOOL_TEST

struct ceph_msgpool {
    const char *name;
    struct mempool_t *pool;
    int front_len;          /* preallocated payload size */
};
struct ceph_msg {
    char msg[128];
    int front_max;
    void *which_pool_belong_to;
};

static struct ceph_msg *ceph_msg_new(int __attribute__((__unused__))type, 
                        int front_len, gfp_t flags)
{
    struct ceph_msg *m;

    m = kmalloc(sizeof(*m), flags, -1);
    if (m == NULL)
    	goto out;

    m->front_max = front_len;
    m->which_pool_belong_to = NULL;
    return m;
out:
    return NULL;
}

static inline void ceph_msg_put(struct ceph_msg __attribute__((__unused__))*msg)
{
    return;
}

static void *alloc_fn(gfp_t gfp_mask, void *arg)
{
    struct ceph_msgpool *pool = arg;
    return ceph_msg_new(0, pool->front_len, gfp_mask);
}

static void free_fn(void *element, 
                    void __attribute__((__unused__))*arg)
{
    kfree(element);
}

int ceph_msgpool_init(struct ceph_msgpool *pool,
		      int __attribute__((__unused__))front_len, int size, 
		      int __attribute__((__unused__))blocking, const char *name)
{
    pool->front_len = front_len;
    pool->pool = mempool_create(size, alloc_fn, free_fn, pool);
    if (!pool->pool)
    	return -1;
    pool->name = name;
    return 0;
}

struct ceph_msg *ceph_msgpool_get(struct ceph_msgpool *pool,
				  int front_len)
{
    if (front_len > pool->front_len) {
        printf("msgpool_get pool %s need front %d, pool size is %d\n",
               pool->name, front_len, pool->front_len);

        /* try to alloc a fresh message */
        return ceph_msg_new(0, front_len, 0);
    }

    return mempool_alloc(pool->pool, 0);
}

struct ceph_msgpool msgpool_op;
void ceph_test()
{
    ceph_msgpool_init(&msgpool_op, 4096, 10, 1,
				"osd_op");

    struct ceph_msg *msg = NULL;
    while((msg = ceph_msgpool_get(&msgpool_op, 0)) != NULL){
            usleep(500000);
            mempool_free(msg, msgpool_op.pool);
    }
}
#endif

