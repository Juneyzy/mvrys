/*
*   rt_threadpool.h
*   Created by Tsihang <qihang@semptian.com>
*   1 June, 2015
*   Func: Threadpool implementation file
*   Personal.Q
*/

#ifndef __RT_THREADPOOL_H__
#define __RT_THREADPOOL_H__

#if 0
 /**
 * Increase this constants at your own risk
 * Large values might slow down your system
 */
#define MAX_THREADS 64
#define MAX_QUEUE 65536

typedef enum {
    threadpool_invalid        = -1,
    threadpool_lock_failure   = -2,
    threadpool_queue_full     = -3,
    threadpool_shutdown       = -4,
    threadpool_thread_failure = -5
} threadpool_error_t;

typedef enum {
    threadpool_graceful       = 1
} threadpool_destroy_flags_t;

/**
 * @function threadpool_create
 * @brief Creates a struct threadpool_t object.
 * @param thread_count Number of worker threads.
 * @param queue_size   Size of the queue.
 * @param flags        Unused parameter.
 * @return a newly created thread pool or NULL
 */
struct threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);

/**
 * @function threadpool_add
 * @brief add a new task in the queue of a thread pool
 * @param pool     Thread pool to which add the task.
 * @param function Pointer to the function that will perform the task.
 * @param argument Argument to be passed to the function.
 * @param flags    Unused parameter.
 * @return 0 if all goes well, negative values in case of error (@see
 * threadpool_error_t for codes).
 */
int threadpool_add(struct threadpool_t *pool, void (*routine)(void *),
                   void *arg, int flags);

/**
 * @function threadpool_destroy
 * @brief Stops and destroys a thread pool.
 * @param pool  Thread pool to destroy.
 * @param flags Flags for shutdown
 *
 * Known values for flags are 0 (default) and threadpool_graceful in
 * which case the thread pool doesn't accept any new tasks but
 * processes all pending tasks before shutdown.
 */
int threadpool_destroy(struct threadpool_t *pool, int flags);
#else

/* =================================== API ======================================= */


typedef struct thpool_* threadpool_t;


/**
 * @brief  Initialize threadpool_t
 * 
 * Initializes a threadpool_t. This function will not return untill all
 * threads have initialized successfully.
 * 
 * @example
 * 
 *    ..
 *    threadpool_t thpool;                     //First we declare a threadpool_t
 *    thpool = thpool_init(4);               //then we initialize it to 4 threads
 *    ..
 * 
 * @param  num_threads   number of threads to be created in the threadpool_t
 * @return threadpool_t    created threadpool_t on success,
 *                       NULL on error
 */
threadpool_t thpool_create(int num_threads, 
                            int __attribute__((__unused__))wqes, int __attribute__((__unused__))flags);


/**
 * @brief Add work to the job queue
 * 
 * Takes an action and its argument and adds it to the threadpool_t's job queue.
 * If you want to add to work a function with more than one arguments then
 * a way to implement this is by passing a pointer to a structure.
 * 
 * NOTICE: You have to cast both the function and argument to not get warnings.
 * 
 * @example
 * 
 *    void print_num(int num){
 *       printf("%d\n", num);
 *    }
 * 
 *    int main() {
 *       ..
 *       int a = 10;
 *       thpool_add_work(thpool, (void*)print_num, (void*)a);
 *       ..
 *    }
 * 
 * @param  threadpool_t    threadpool_t to which the work will be added
 * @param  function_p    pointer to function to add as work
 * @param  arg_p         pointer to an argument
 * @return 0 on successs, -1 otherwise.
 */
int thpool_add_work(threadpool_t, void *(*function_p)(void*), void* arg_p);

#define threadpool_add(p, f, a, unused) thpool_add_work(p, f, a)

/**
 * @brief Wait for all queued jobs to finish
 * 
 * Will wait for all jobs - both queued and currently running to finish.
 * Once the queue is empty and all work has completed, the calling thread
 * (probably the main program) will continue.
 * 
 * Smart polling is used in wait. The polling is initially 0 - meaning that
 * there is virtually no polling at all. If after 1 seconds the threads
 * haven't finished, the polling interval starts growing exponentially 
 * untill it reaches max_secs seconds. Then it jumps down to a maximum polling
 * interval assuming that heavy processing is being used in the threadpool_t.
 *
 * @example
 * 
 *    ..
 *    threadpool_t thpool = thpool_init(4);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    thpool_wait(thpool);
 *    puts("All added work has finished");
 *    ..
 * 
 * @param threadpool_t     the threadpool_t to wait for
 * @return nothing
 */
void thpool_wait(threadpool_t);


/**
 * @brief Pauses all threads immediately
 * 
 * The threads will be paused no matter if they are idle or working.
 * The threads return to their previous states once thpool_resume
 * is called.
 * 
 * While the thread is being paused, new work can be added.
 * 
 * @example
 * 
 *    threadpool_t thpool = thpool_init(4);
 *    thpool_pause(thpool);
 *    ..
 *    // Add a bunch of work
 *    ..
 *    thpool_resume(thpool); // Let the threads start their magic
 * 
 * @param threadpool_t    the threadpool_t where the threads should be paused
 * @return nothing
 */
void thpool_pause(threadpool_t);


/**
 * @brief Unpauses all threads if they are paused
 * 
 * @example
 *    ..
 *    thpool_pause(thpool);
 *    sleep(10);              // Delay execution 10 seconds
 *    thpool_resume(thpool);
 *    ..
 * 
 * @param threadpool_t     the threadpool_t where the threads should be unpaused
 * @return nothing
 */
void thpool_resume(threadpool_t);


/**
 * @brief Destroy the threadpool_t
 * 
 * This will wait for the currently active threads to finish and then 'kill'
 * the whole threadpool_t to free up memory.
 * 
 * @example
 * int main() {
 *    threadpool_t thpool1 = thpool_init(2);
 *    threadpool_t thpool2 = thpool_init(2);
 *    ..
 *    thpool_destroy(thpool1);
 *    ..
 *    return 0;
 * }
 * 
 * @param threadpool_t     the threadpool_t to destroy
 * @return nothing
 */
void thpool_destroy(threadpool_t, int __attribute__((__unused__))flags);

#endif
#endif /* __RT_THREADPOOL_H__ */

