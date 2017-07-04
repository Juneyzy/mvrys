/*
*   sync.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System startup synchronous operation interface of SPASR
*   Personal.Q
*/


#ifndef __SYSTEM_SYNC_H__
#define __SYSTEM_SYNC_H__

#include <pthread.h>


#define rt_pthread_attr pthread_attr_t
#define rt_pthread  pthread_t
#define rt_mutex pthread_mutex_t
#define rt_mutex_attr pthread_mutexattr_t
#define rt_mutex_init(mut, mutattr ) pthread_mutex_init(mut, mutattr)
#define rt_mutex_lock(mut) pthread_mutex_lock(mut)
#define rt_mutex_trylock(mut) pthread_mutex_trylock(mut)
#define rt_mutex_unlock(mut) pthread_mutex_unlock(mut)
#define rt_mutex_destroy(mut) pthread_mutex_destroy(mut)

#define rt_cond pthread_cond_t
#define rt_cond_init(cond, attr)    pthread_cond_init(cond, attr)
#define rt_cond_signal(cond) pthread_cond_signal(cond)
#define rt_cond_broadcast(cond) pthread_cond_broadcast(cond)
#define rt_cond_wait(cond, mtx) pthread_cond_wait(cond, mtx)
#define rt_cond_timedwait(cond, mtx, tspec) pthread_cond_timedwait(cond, mtx, tspec)
#define rt_cond_destroy(cond) pthread_cond_destroy(cond);

#define rt_spinlock                                                 pthread_spinlock_t
#define rt_spinlock_lock(spin)                              pthread_spin_lock(spin)
#define rt_spinlock_trylock(spin)                     pthread_spin_trylock(spin)
#define rt_spinlock_unlock(spin)                      pthread_spin_unlock(spin)
#define rt_spinlock_init(spin, spin_attr)             pthread_spin_init(spin, spin_attr)
#define rt_spinlock_destroy(spin)                     pthread_spin_destroy(spin)


#define INIT_MUTEX(name)\
    rt_mutex name = PTHREAD_MUTEX_INITIALIZER;

extern void rt_sync_init();
extern void rt_unsync();
extern void rt_sync();


#endif

