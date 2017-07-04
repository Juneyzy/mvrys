/*
*   rt_pool.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Pool for inter-thread communication
*   Personal.Q
*/


#ifndef __RT_POOL_H__
#define __RT_POOL_H__

#include "rt_list.h"
#include "rt_sync.h"
#include <stdint.h>

struct rt_pool_bucket_t{

    /** private data area, allocated and freed by USER */
    void *priv_data;
    
    /** sizeof priv_data, unused */
    int size;
    
    /** which pool does this bucket belong to */
    void *pool;
    
    struct list_head	list;

    struct rt_pool_bucket_t *next;
    struct rt_pool_bucket_t *prev;
};

struct rt_pool_t{
    
    struct rt_pool_bucket_t *top;

    struct rt_pool_bucket_t *bot;

    volatile uint32_t len;

    uint32_t    prealloc_size;

    rt_mutex m;

    /** alloc for the bucket which defined in struct rt_ethxx_pool_bucket_t, we call it priv data domain */
    void *(*priv_alloc)();

    /** free for the bucket which defined in struct rt_ethxx_pool_bucket_t, we call it priv data domain */
    void (*priv_free)(void *);
      
    /**
    * Returns -1 if val1 < val2, 0 if equal?, 1 if val1 > val2.
    * Used as definition of sorted for xxx_add_sort
    */
    int (*cmp)(void *val1, void *val2);

    /**
    * callback to free user-owned data when listnode is deleted. supplying
    * this callback is very much encouraged!
    */
    void (*del)(void *val);
    
};


extern struct rt_pool_t *
rt_pool_initialize(int buckets,
                        void *(*priv_alloc)(),
                        void (*priv_free)(void *),
                        int flags);

extern void rt_pool_destroy(struct rt_pool_t *pool);

extern struct rt_pool_bucket_t *
rt_pool_bucket_get_new(struct rt_pool_t *pool,
                        struct rt_pool_bucket_t __attribute__((__unused__))*x);

extern struct rt_pool_bucket_t *
rt_pool_bucket_get (struct rt_pool_t *pool);

extern void rt_pool_bucket_push (struct rt_pool_t *pool,
                        struct rt_pool_bucket_t *bucket);

extern int rt_pool_bucket_number (struct rt_pool_t *pool);

#endif

