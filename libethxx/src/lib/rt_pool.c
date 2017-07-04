/*
*   rt_pool.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Pool for inter-thread communication
*   Personal.Q
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "rt_logging.h"
#include "rt_stdlib.h"
#include "rt_common.h"

#include "rt_pool.h"


#define BUCKET_INITIALIZE(bucket) do { \
            (bucket)->next = NULL;\
            (bucket)->prev = NULL;\
            (bucket)->priv_data = NULL;\
            INIT_LIST_HEAD(&(bucket)->list);\
         } while (0)

#define QLOCK_INIT(pool) rt_mutex_init(&(pool)->m, NULL)
#define QLOCK_DESTROY(pool) rt_mutex_destroy(&(pool)->m)
#define QLOCK_LOCK(pool) rt_mutex_lock(&(pool)->m)
#define QLOCK_TRYLOCK(pool) rt_mutex_trylock(&(pool)->m)
#define QLOCK_UNLOCK(pool) rt_mutex_unlock(&(pool)->m)

static __rt_always_inline__ void *
pool_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

static __rt_always_inline__ struct rt_pool_t *
rt_pool_new()
{
    struct rt_pool_t *pool = NULL;
    
    pool = (struct rt_pool_t *)pool_kmalloc(sizeof(struct rt_pool_t));
    if(likely(pool)){
        QLOCK_INIT(pool);
        goto finish;
    }

    rt_log_error (ERRNO_MEM_ALLOC,
        "%s\n", strerror(errno));

finish:
    return pool;
}

static __rt_always_inline__ struct rt_pool_bucket_t *
rt_pool_bucket_alloc(struct rt_pool_t *pool)
{
    struct rt_pool_bucket_t *bucket;

    bucket = (struct rt_pool_bucket_t *)pool_kmalloc(sizeof(struct rt_pool_bucket_t));
    if (likely(bucket)) {
        BUCKET_INITIALIZE(bucket);
        if (pool->priv_alloc){
            bucket->priv_data = pool->priv_alloc();
            if (likely(bucket->priv_data) ){
                bucket->pool = pool;
                goto finish;
            }
        }
    }

     rt_log_error(ERRNO_MEM_ALLOC,
        "%s", strerror(errno));

finish:
    return bucket;
}

void rt_pool_bucket_push (struct rt_pool_t *pool,
                        struct rt_pool_bucket_t *bucket)
{
#ifdef DEBUG
    BUG_ON(pool == NULL || bucket == NULL);
#endif

    QLOCK_LOCK(pool);

    if (pool->top != NULL) {
        bucket->next = pool->top;
        pool->top->prev = bucket;
        pool->top = bucket;
    } else {
        pool->top = bucket;
        pool->bot = bucket;
    }
    pool->len++;
#ifdef DBG_PERF
    if (pool->len > pool->dbg_maxlen)
        pool->dbg_maxlen = pool->len;
#endif /* DBG_PERF */
    QLOCK_UNLOCK(pool);
}

struct rt_pool_bucket_t *
rt_pool_bucket_get (struct rt_pool_t *pool)
{
    QLOCK_LOCK(pool);

    struct rt_pool_bucket_t *bucket = pool->bot;
    if (unlikely(!bucket)) {
        QLOCK_UNLOCK(pool);
        return NULL;
    }

    if (pool->bot->prev != NULL) {
        pool->bot = pool->bot->prev;
        pool->bot->next = NULL;
    } else {
        pool->top = NULL;
        pool->bot = NULL;
    }

#ifdef DEBUG
    BUG_ON(pool->len == 0);
#endif
    if (pool->len > 0)
        pool->len--;

    bucket->next = NULL;
    bucket->prev = NULL;

    QLOCK_UNLOCK(pool);
    return bucket;
}

/** Not Recommended */
static __rt_always_inline__ void
rt_pool_bucket_free(struct rt_pool_t __attribute__((__unused__))*pool,
                        struct rt_pool_bucket_t * bucket)
{
    if(likely(bucket)){
        if(likely(bucket->priv_data)){
            if(likely(pool->priv_free))
                pool->priv_free(bucket->priv_data);
        }
        free(bucket);
    }
}

struct rt_pool_bucket_t *
rt_pool_bucket_get_new(struct rt_pool_t *pool,
   struct rt_pool_bucket_t __attribute__((__unused__))*x)
{
    struct rt_pool_bucket_t *bucket = NULL;

    if(likely(pool)){
        bucket = rt_pool_bucket_get(pool);
        if (likely(bucket))
            goto finish;

        bucket = rt_pool_bucket_alloc(pool);
        if (likely(bucket)) {
            goto finish;
        }
    }
    
    rt_log_warning(ERRNO_MEM_ALLOC,
        "Don't have enough space for pool bucket allocation");
finish:
    return bucket;
}


struct rt_pool_t *
rt_pool_initialize(int buckets,
    void *(*priv_alloc)(),
    void (*priv_free)(void *),
    int __attribute__((__unused__))flags)
{
    int i = 0;
    struct rt_pool_t *pool;
    int errors = 0;
    
    pool = rt_pool_new();
    if(unlikely(!pool)){
        rt_log_error(ERRNO_MEMBLK_ALLOC,
                "%s", strerror(errno));
        goto finish;
    }

    pool->priv_alloc = priv_alloc;
    pool->priv_free = priv_free;

    for (i = 0; i < buckets; i++) {
        struct rt_pool_bucket_t *b = rt_pool_bucket_alloc(pool);
        if (likely(b)) {
            pool->prealloc_size ++;
            rt_pool_bucket_push(pool, b);
            continue;
        }
        
        errors ++;
    }

    if(errors) rt_log_error(ERRNO_MEMBLK_ALLOC,
                        "%s, allocated failure %d", strerror(errno), errors);
finish:
    return pool;
}

void rt_pool_destroy(struct rt_pool_t *pool)
{
    struct rt_pool_bucket_t *bucket;

    if(likely(pool)){
        bucket = rt_pool_bucket_get(pool);
        while(likely(bucket)){
            if(pool->priv_free){
                if(bucket->priv_data)
                    pool->priv_free(bucket->priv_data);
            }
            free(bucket);
        }
        free(pool);
    }
}

int
rt_pool_bucket_number (struct rt_pool_t *pool)
{
    if(likely(pool)){
	return (int)pool->len;
    }

    return -1;
}
