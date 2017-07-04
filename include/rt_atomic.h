/*
*   rt_atomic.h
*   Created by Tsihang <qihang@semptian.com>
*   22 Feb, 2016
*   Func: Lockless Chain
*   Personal.Q
*/

#ifndef __RT_ATOMIC_H__
#define __RT_ATOMIC_H__

typedef struct{
#define ATOMIC_SIZE sizeof(int)
    int32_t counter;
}atomic_t;

typedef struct{
#define ATOMIC64_SIZE sizeof(long long)
    int64_t counter;
}atomic64_t;


#define ATOMIC_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v)	(*(volatile int *)&(v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
//#define atomic_set(v, i) (((v)->counter) = (i))
static inline void atomic_set(atomic_t *v, int32_t val)
{
    v->counter = val;
}

#define atomic_add(x, y) (__sync_add_and_fetch((&(((atomic_t *)x)->counter)), (y)))
#define atomic_sub(x, y) (__sync_sub_and_fetch((&(((atomic_t *)x)->counter)), (y)))
#define atomic_inc(x)    (atomic_add((x), 1))
#define atomic_dec(x)   (atomic_sub((x), 1))


#define atomic64_add(x, y) (__sync_add_and_fetch((&(((atomic64_t *)x)->counter)), (y)))
#define atomic64_sub(x, y) (__sync_sub_and_fetch((&(((atomic64_t *)x)->counter)), (y)))
#define atomic64_inc(x)  (atomic64_add((x), 1))
#define atomic64_dec(x) (atomic64_sub((x), 1))


/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic64_read(v)	(*(volatile int64_t *)&(v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
//#define atomic64_set(v, i) (((v)->counter) = (i))
static inline void atomic64_set(atomic64_t *v, int64_t val)
{
    v->counter = val;
}

#endif
