/*
*   rt_threadpool.c
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Threadpool implementation file
*   Personal.Q
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h> 
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "rt_sync.h"
#include "rt_atomic.h"
#include "rt_thrdpool.h"
#include "rt_stdlib.h"

#if 0
/*
 * Warning do not increase THREAD and QUEUES too much on 32-bit
 * platforms: because of each thread (and there will be THREAD *
 * QUEUES of them) will allocate its own stack (8 MB is the default on
 * Linux), you'll quickly run out of virtual space.
 */
 
typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown  = 2
} threadpool_shutdown_t;

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

struct threadpool_task_t{
    void (*function)(void *);
    void *argument;
};

/**
 *  @struct threadpool
 *  @brief The threadpool struct
 *
 *  @var notify       Condition variable to notify worker threads.
 *  @var threads      Array containing worker threads ID.
 *  @var thread_count Number of threads
 *  @var queue        Array containing the task queue.
 *  @var queue_size   Size of the task queue.
 *  @var head         Index of the first element.
 *  @var tail         Index of the next element.
 *  @var count        Number of pending tasks
 *  @var shutdown     Flag indicating if the pool is shutting down
 *  @var started      Number of started threads
 */
struct threadpool_t {
  rt_mutex lock;
  rt_cond notify;
  rt_pthread *threads;
  struct threadpool_task_t *queue;
  int thread_count;
  int queue_size;
  int head;
  int tail;
  int count;
  int shutdown;
  int started;
};

/**
 * @function void *threadpool_thread(void *threadpool)
 * @brief the worker thread
 * @param threadpool the pool which own the thread
 */
static void *threadpool_thread(void *threadpool);

int threadpool_free(struct threadpool_t *pool);

static inline void *threadpool_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

struct threadpool_t *
threadpool_create(int thread_count,
    int queue_size, 
    int __attribute__((__unused__)) flags)
{
    if(thread_count <= 0 || thread_count > MAX_THREADS || 
        queue_size <= 0 || queue_size > MAX_QUEUE) {
        return NULL;
    }
    
    struct threadpool_t *pool;
    int i;

    if((pool = (struct threadpool_t *)threadpool_kmalloc(sizeof(struct threadpool_t))) == NULL) {
        goto err;
    }

    /* Initialize */
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    /* Allocate thread and task queue */
    pool->threads = (rt_pthread *)threadpool_kmalloc(sizeof(rt_pthread) * thread_count);
    pool->queue = (struct threadpool_task_t *)threadpool_kmalloc
        (sizeof(struct threadpool_task_t) * queue_size);

    /* Initialize mutex and conditional variable first */
    if((rt_mutex_init(&pool->lock, NULL)) ||
       (rt_cond_init(&pool->notify, NULL)) ||
       (pool->threads == NULL) ||
       (pool->queue == NULL)) {
        goto err;
    }

    /* Start worker threads */
    for(i = 0; i < thread_count; i++) {
        if(pthread_create(&(pool->threads[i]), NULL,
                          threadpool_thread, (void*)pool) != 0) {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }

    return pool;

 err:
    if(pool) {
        threadpool_free(pool);
    }
    return NULL;
}

int 
threadpool_add(struct threadpool_t *pool, 
    void (*function)(void *),
    void *argument, 
    int __attribute__((__unused__)) flags)
{
    int err = 0;
    int next;

    if(pool == NULL || function == NULL) {
        return threadpool_invalid;
    }

    rt_mutex_lock(&pool->lock);

    next = pool->tail + 1;
    next = (next == pool->queue_size) ? 0 : next;

    do {
        /* Are we full ? */
        if(pool->count == pool->queue_size) {
            err = threadpool_queue_full;
            break;
        }

        /* Are we shutting down ? */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        /* Add task to queue */
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;

        /* pthread_cond_broadcast */
        rt_cond_signal(&pool->notify);
    } while(0);

    rt_mutex_unlock(&pool->lock);

    return err;
}

int 
threadpool_destroy(struct threadpool_t *pool, 
    int flags)
{
    int i, err = 0;

    if(pool == NULL) {
        return threadpool_invalid;
    }

    rt_mutex_lock(&pool->lock);

    do {
        /* Already shutting down */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        pool->shutdown = (flags & threadpool_graceful) ?
            graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        if(rt_cond_broadcast(&pool->notify) ||
           rt_mutex_unlock(&pool->lock)) {
            err = threadpool_lock_failure;
            break;
        }

        /* Join all worker thread */
        for(i = 0; i < pool->thread_count; i++) {
            if(pthread_join(pool->threads[i], NULL) != 0) {
                err = threadpool_thread_failure;
            }
        }
    } while(0);

    /* Only if everything went well do we deallocate the pool */
    if(!err) {
        threadpool_free(pool);
    }
    return err;
}

int 
threadpool_free(struct threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0) {
        return -1;
    }

    /* Did we manage to allocate ? */
    if(pool->threads) {
        free(pool->threads);
        free(pool->queue);
 
        /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
        rt_mutex_lock(&pool->lock);
        rt_mutex_destroy(&pool->lock);
        rt_cond_destroy(&pool->notify);
    }
    free(pool);    
    return 0;
}


static void *
threadpool_thread(void *threadpool)
{
    struct threadpool_t *pool = (struct threadpool_t *)threadpool;
    struct threadpool_task_t task;

    for(;;) {
        /* Lock must be taken to wait on conditional variable */
        rt_mutex_lock(&pool->lock);

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        while((pool->count == 0) && (!pool->shutdown)) {
            rt_cond_wait(&pool->notify, &pool->lock);
        }

        if((pool->shutdown == immediate_shutdown) ||
           ((pool->shutdown == graceful_shutdown) &&
            (pool->count == 0 /** wait for all task finished running */))) {
            break;
        }

        /* Grab our task */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head += 1;
        pool->head = (pool->head == pool->queue_size) ? 0 : pool->head;
        pool->count -= 1;

        /* Unlock */
        rt_mutex_unlock(&pool->lock);

        /* Get to work */
        (*(task.function))(task.argument);
    }

    pool->started--;

    rt_mutex_unlock(&pool->lock);
    pthread_exit(NULL);
    return(NULL);
}

#define THRDPOOL_TEST
#ifdef THRDPOOL_TEST
#define THREAD 4
#define SIZE   8192
#define QUEUES SIZE

atomic_t tasks = ATOMIC_INIT(0), done = ATOMIC_INIT(0);

void dummy_task0(void __attribute__((__unused__))*arg) 
{
    usleep(10000);
    atomic_inc(&done);
}

int rt_thrdpool_test0()
{
    int dxx, txx;
    
    struct threadpool_t *pool;

    assert((pool = threadpool_create(THREAD, QUEUES, 0)) != NULL);
    fprintf(stderr, "Pool started with %d threads and "
            "queue size of %d\n", THREAD, QUEUES);

    while(threadpool_add(pool, &dummy_task0, NULL, 0) == 0) {
        atomic_inc(&tasks);
    }

    txx = atomic_add(&tasks, 0);
    
    fprintf(stderr, "Added %d tasks\n", txx);

    dxx = atomic_add(&done, 0);
    while((txx / 2) > dxx) {
        usleep(10000);
        dxx = atomic_add(&done, 0);
    }
    
    assert(threadpool_destroy(pool, 0) == 0);
    fprintf(stderr, "Did %d tasks\n", dxx);

    return 0;
}


#if 0
int tasks[SIZE], left;
rt_mutex lock;

int error;

void dummy_task(void *arg) {
    int *pi = (int *)arg;
    *pi += 1;

    if(*pi < QUEUES) {
        assert(threadpool_add(pool[*pi], &dummy_task, arg, 0) == 0);
    } else {
        pthread_mutex_lock(&lock);
        left--;
        pthread_mutex_unlock(&lock);
    }
}

int main(int argc, char **argv)
{
    /*
     * Warning do not increase THREAD and QUEUES too much on 32-bit
     * platforms: because of each thread (and there will be THREAD *
     * QUEUES of them) will allocate its own stack (8 MB is the default on
     * Linux), you'll quickly run out of virtual space.
     */

    threadpool_t *pool[QUEUES];

    int i, copy = 1;

    left = SIZE;
    pthread_mutex_init(&lock, NULL);

    for(i = 0; i < QUEUES; i++) {
        pool[i] = threadpool_create(THREAD, SIZE, 0);
        assert(pool[i] != NULL);
    }

    usleep(10);

    for(i = 0; i < SIZE; i++) {
        tasks[i] = 0;
        assert(threadpool_add(pool[0], &dummy_task, &(tasks[i]), 0) == 0);
    }

    while(copy > 0) {
        usleep(10);
        pthread_mutex_lock(&lock);
        copy = left;
        pthread_mutex_unlock(&lock);
    }

    for(i = 0; i < QUEUES; i++) {
        assert(threadpool_destroy(pool[i], 0) == 0);
    }

    pthread_mutex_destroy(&lock);

    return 0;
}
#endif

void rt_thrdpool_test()
{
    rt_thrdpool_test0();
}

#endif

#else


#ifdef THPOOL_DEBUG
#define THPOOL_DEBUG 1
#else
#define THPOOL_DEBUG 0
#endif

static volatile int threads_keepalive;
static volatile int threads_on_hold;



/* ========================== STRUCTURES ============================ */


/* Binary semaphore */
typedef struct bsem {
	pthread_mutex_t mutex;
	pthread_cond_t   cond;
	int v;
} bsem;


/* Job */
typedef struct job{
	struct job*  prev;                   /* pointer to previous job   */
	void*  (*function)(void* arg);       /* function pointer          */
	void*  arg;                          /* function's argument       */
} job;


/* Job queue */
typedef struct jobqueue{
	pthread_mutex_t rwmutex;             /* used for queue r/w access */
	job  *front;                         /* pointer to front of queue */
	job  *rear;                          /* pointer to rear  of queue */
	bsem *has_jobs;                      /* flag as binary semaphore  */
	int   len;                           /* number of jobs in queue   */
} jobqueue;


/* Thread */
typedef struct thread{
	int       id;                        /* friendly id               */
	pthread_t pthread;                   /* pointer to actual thread  */
	struct thpool_* thpool_p;            /* access to thpool          */
} thread;


/* Threadpool */
typedef struct thpool_{
	thread**   threads;                  /* pointer to threads        */
	volatile int num_threads_alive;      /* threads currently alive   */
	volatile int num_threads_working;    /* threads currently working */
	pthread_mutex_t  thcount_lock;       /* used for thread count etc */
	pthread_cond_t  threads_all_idle;    /* signal to thpool_wait     */
	jobqueue*  jobqueue_p;               /* pointer to the job queue  */    
} thpool_;





/* ========================== PROTOTYPES ============================ */


static int  thread_init(thpool_* thpool_p, struct thread** thread_p, int id);
static void* thread_do(struct thread* thread_p);
static void  thread_hold();
static void  thread_destroy(struct thread* thread_p);

static int   jobqueue_init(thpool_* thpool_p);
static void  jobqueue_clear(thpool_* thpool_p);
static void  jobqueue_push(thpool_* thpool_p, struct job* newjob_p);
static struct job* jobqueue_pull(thpool_* thpool_p);
static void  jobqueue_destroy(thpool_* thpool_p);

static void  bsem_init(struct bsem *bsem_p, int value);
static void  bsem_reset(struct bsem *bsem_p);
static void  bsem_post(struct bsem *bsem_p);
static void  bsem_post_all(struct bsem *bsem_p);
static void  bsem_wait(struct bsem *bsem_p);





/* ========================== THREADPOOL ============================ */


/* Initialise thread pool */
struct thpool_* thpool_create(int num_threads, 
                            int __attribute__((__unused__))wqes, int __attribute__((__unused__))flags){

	threads_on_hold   = 0;
	threads_keepalive = 1;

	if (num_threads < 0){
		num_threads = 0;
	}

	/* Make new thread pool */
	thpool_* thpool_p;
	thpool_p = (struct thpool_*)malloc(sizeof(struct thpool_));
	if (thpool_p == NULL){
		fprintf(stderr, "thpool_init(): Could not allocate memory for thread pool\n");
		return NULL;
	}
	thpool_p->num_threads_alive   = 0;
	thpool_p->num_threads_working = 0;

	/* Initialise the job queue */
	if (jobqueue_init(thpool_p) == -1){
		fprintf(stderr, "thpool_init(): Could not allocate memory for job queue\n");
		free(thpool_p);
		return NULL;
	}

	/* Make threads in pool */
	thpool_p->threads = (struct thread**)malloc(num_threads * sizeof(struct thread *));
	if (thpool_p->threads == NULL){
		fprintf(stderr, "thpool_init(): Could not allocate memory for threads\n");
		jobqueue_destroy(thpool_p);
		free(thpool_p->jobqueue_p);
		free(thpool_p);
		return NULL;
	}

	pthread_mutex_init(&(thpool_p->thcount_lock), NULL);
	pthread_cond_init(&thpool_p->threads_all_idle, NULL);
	
	/* Thread init */
	int n;
	for (n=0; n<num_threads; n++){
		thread_init(thpool_p, &thpool_p->threads[n], n);
		if (THPOOL_DEBUG)
			printf("THPOOL_DEBUG: Created thread %d in pool \n", n);
	}
	
	/* Wait for threads to initialize */
	while (thpool_p->num_threads_alive != num_threads) {}

	return thpool_p;
}


/* Add work to the thread pool */
int thpool_add_work(thpool_* thpool_p, void *(*function_p)(void*), void* arg_p){
	job* newjob;

	newjob=(struct job*)malloc(sizeof(struct job));
	if (newjob==NULL){
		fprintf(stderr, "thpool_add_work(): Could not allocate memory for new job\n");
		return -1;
	}

	/* add function and argument */
	newjob->function=function_p;
	newjob->arg=arg_p;

	/* add job to queue */
	pthread_mutex_lock(&thpool_p->jobqueue_p->rwmutex);
	jobqueue_push(thpool_p, newjob);
	pthread_mutex_unlock(&thpool_p->jobqueue_p->rwmutex);

	return 0;
}


/* Wait until all jobs have finished */
void thpool_wait(thpool_* thpool_p){
	pthread_mutex_lock(&thpool_p->thcount_lock);
	while (thpool_p->jobqueue_p->len || thpool_p->num_threads_working) {
		pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
	}
	pthread_mutex_unlock(&thpool_p->thcount_lock);
}


/* Destroy the threadpool */
void thpool_destroy(thpool_* thpool_p, int __attribute__((__unused__))flags){
	/* No need to destory if it's NULL */
	if (thpool_p == NULL) return ;

	volatile int threads_total = thpool_p->num_threads_alive;

	/* End each thread 's infinite loop */
	threads_keepalive = 0;
	
	/* Give one second to kill idle threads */
	double TIMEOUT = 1.0;
	time_t start, end;
	double tpassed = 0.0;
	time (&start);
	while (tpassed < TIMEOUT && thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue_p->has_jobs);
		time (&end);
		tpassed = difftime(end,start);
	}
	
	/* Poll remaining threads */
	while (thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue_p->has_jobs);
		sleep(1);
	}

	/* Job queue cleanup */
	jobqueue_destroy(thpool_p);
	free(thpool_p->jobqueue_p);
	
	/* Deallocs */
	int n;
	for (n=0; n < threads_total; n++){
		thread_destroy(thpool_p->threads[n]);
	}
	free(thpool_p->threads);
	free(thpool_p);
}


/* Pause all threads in threadpool */
void thpool_pause(thpool_* thpool_p) {
	int n;
	for (n=0; n < thpool_p->num_threads_alive; n++){
		pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
	}
}


/* Resume all threads in threadpool */
void thpool_resume(thpool_ __attribute__((__unused__))*thpool_p) {
	threads_on_hold = 0;
}





/* ============================ THREAD ============================== */


/* Initialize a thread in the thread pool
 * 
 * @param thread        address to the pointer of the thread to be created
 * @param id            id to be given to the thread
 * @return 0 on success, -1 otherwise.
 */
static int thread_init (thpool_* thpool_p, struct thread** thread_p, int id){
	
	*thread_p = (struct thread*)malloc(sizeof(struct thread));
	if (thread_p == NULL){
		fprintf(stderr, "thpool_init(): Could not allocate memory for thread\n");
		return -1;
	}

	(*thread_p)->thpool_p = thpool_p;
	(*thread_p)->id       = id;

	pthread_create(&(*thread_p)->pthread, NULL, (void *)thread_do, (*thread_p));
	pthread_detach((*thread_p)->pthread);
	return 0;
}


/* Sets the calling thread on hold */
static void thread_hold () {
	threads_on_hold = 1;
	while (threads_on_hold){
		sleep(1);
	}
}


/* What each thread is doing
* 
* In principle this is an endless loop. The only time this loop gets interuppted is once
* thpool_destroy() is invoked or the program exits.
* 
* @param  thread        thread that will run this function
* @return nothing
*/
static void* thread_do(struct thread* thread_p){

	/* Set thread name for profiling and debuging */
	char thread_name[128] = {0};
	sprintf(thread_name, "thread-pool-%d", thread_p->id);

#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(thread_name);
#else
	fprintf(stderr, "thread_do(): pthread_setname_np is not supported on this system");
#endif

	/* Assure all threads have been created before starting serving */
	thpool_* thpool_p = thread_p->thpool_p;
	
	/* Register signal handler */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = thread_hold;
	if (sigaction(SIGUSR1, &act, NULL) == -1) {
		fprintf(stderr, "thread_do(): cannot handle SIGUSR1");
	}
	
	/* Mark thread as alive (initialized) */
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive += 1;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	while(threads_keepalive){

		bsem_wait(thpool_p->jobqueue_p->has_jobs);

		if (threads_keepalive){
			
			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working++;
			pthread_mutex_unlock(&thpool_p->thcount_lock);
			
			/* Read job from queue and execute it */
			void*(*func_buff)(void* arg);
			void*  arg_buff;
			job* job_p;
			pthread_mutex_lock(&thpool_p->jobqueue_p->rwmutex);
			job_p = jobqueue_pull(thpool_p);
			pthread_mutex_unlock(&thpool_p->jobqueue_p->rwmutex);
			if (job_p) {
				func_buff = job_p->function;
				arg_buff  = job_p->arg;
				func_buff(arg_buff);
				free(job_p);
			}
			
			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working--;
			if (!thpool_p->num_threads_working) {
				pthread_cond_signal(&thpool_p->threads_all_idle);
			}
			pthread_mutex_unlock(&thpool_p->thcount_lock);

		}
	}
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive --;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	return NULL;
}


/* Frees a thread  */
static void thread_destroy (thread* thread_p){
	free(thread_p);
}





/* ============================ JOB QUEUE =========================== */


/* Initialize queue */
static int jobqueue_init(thpool_* thpool_p){
	
	thpool_p->jobqueue_p = (struct jobqueue*)malloc(sizeof(struct jobqueue));
	if (thpool_p->jobqueue_p == NULL){
		return -1;
	}
	thpool_p->jobqueue_p->len = 0;
	thpool_p->jobqueue_p->front = NULL;
	thpool_p->jobqueue_p->rear  = NULL;

	thpool_p->jobqueue_p->has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
	if (thpool_p->jobqueue_p->has_jobs == NULL){
		return -1;
	}

	pthread_mutex_init(&(thpool_p->jobqueue_p->rwmutex), NULL);
	bsem_init(thpool_p->jobqueue_p->has_jobs, 0);

	return 0;
}


/* Clear the queue */
static void jobqueue_clear(thpool_* thpool_p){

	while(thpool_p->jobqueue_p->len){
		free(jobqueue_pull(thpool_p));
	}

	thpool_p->jobqueue_p->front = NULL;
	thpool_p->jobqueue_p->rear  = NULL;
	bsem_reset(thpool_p->jobqueue_p->has_jobs);
	thpool_p->jobqueue_p->len = 0;

}


/* Add (allocated) job to queue
 *
 * Notice: Caller MUST hold a mutex
 */
static void jobqueue_push(thpool_* thpool_p, struct job* newjob){

	newjob->prev = NULL;

	switch(thpool_p->jobqueue_p->len){

		case 0:  /* if no jobs in queue */
					thpool_p->jobqueue_p->front = newjob;
					thpool_p->jobqueue_p->rear  = newjob;
					break;

		default: /* if jobs in queue */
					thpool_p->jobqueue_p->rear->prev = newjob;
					thpool_p->jobqueue_p->rear = newjob;
					
	}
	thpool_p->jobqueue_p->len++;
	
	bsem_post(thpool_p->jobqueue_p->has_jobs);
}


/* Get first job from queue(removes it from queue)
 * 
 * Notice: Caller MUST hold a mutex
 */
static struct job* jobqueue_pull(thpool_* thpool_p){

	job* job_p;
	job_p = thpool_p->jobqueue_p->front;

	switch(thpool_p->jobqueue_p->len){
		
		case 0:  /* if no jobs in queue */
		  			break;
		
		case 1:  /* if one job in queue */
					thpool_p->jobqueue_p->front = NULL;
					thpool_p->jobqueue_p->rear  = NULL;
					thpool_p->jobqueue_p->len = 0;
					break;
		
		default: /* if >1 jobs in queue */
					thpool_p->jobqueue_p->front = job_p->prev;
					thpool_p->jobqueue_p->len--;
					/* more than one job in queue -> post it */
					bsem_post(thpool_p->jobqueue_p->has_jobs);
					
	}
	
	return job_p;
}


/* Free all queue resources back to the system */
static void jobqueue_destroy(thpool_* thpool_p){
	jobqueue_clear(thpool_p);
	free(thpool_p->jobqueue_p->has_jobs);
}





/* ======================== SYNCHRONISATION ========================= */


/* Init semaphore to 1 or 0 */
static void bsem_init(bsem *bsem_p, int value) {
	if (value < 0 || value > 1) {
		fprintf(stderr, "bsem_init(): Binary semaphore can take only values 1 or 0");
		exit(1);
	}
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = value;
}


/* Reset semaphore to 0 */
static void bsem_reset(bsem *bsem_p) {
	bsem_init(bsem_p, 0);
}


/* Post to at least one thread */
static void bsem_post(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_signal(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Post to all threads */
static void bsem_post_all(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_broadcast(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Wait on semaphore until semaphore has value 0 */
static void bsem_wait(bsem* bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	while (bsem_p->v != 1) {
		pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
	}
	bsem_p->v = 0;
	pthread_mutex_unlock(&bsem_p->mutex);
}

#endif

