/*
*   sync.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: System startup synchronous operation interface of SPASR
*   Personal.Q
*/

#include <semaphore.h>
#include "rt_sync.h"

/** For thread sync */
sem_t spasr_sysSync_sem;
/** Shared in all proccess, include threads of proccess */
int pshared = 0;

pthread_cond_t spasr_sysSync_cond;

/** All system semphore is initialized in this api */
void rt_sync_init()
{
    sem_init(&spasr_sysSync_sem, pshared, 1);
    /** Add other semphore here */
}

void rt_unsync()
{
    sem_post(&spasr_sysSync_sem);
}

void rt_sync()
{
    sem_wait(&spasr_sysSync_sem);
}
