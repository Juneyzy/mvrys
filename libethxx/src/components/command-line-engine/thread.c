/* Thread management routine
 * Copyright (C) 1998, 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* #define DEBUG */

#include "zebra.h"

#include "thread.h"
#include "memory.h"
#include "log.h"
#include "hash.h"
#include "command.h"

//#include "linux.h"

static struct hash *cpu_record = NULL;

/* Struct timeval's tv_usec one second value.  */
#define TIMER_SECOND_MICRO 1000000L

struct timeval
timeval_adjust(struct timeval a)
{
    while (a.tv_usec >= TIMER_SECOND_MICRO)
    {
        a.tv_usec -= TIMER_SECOND_MICRO;
        a.tv_sec++;
    }
    
    while (a.tv_usec < 0)
    {
        a.tv_usec += TIMER_SECOND_MICRO;
        a.tv_sec--;
    }
    
    if (a.tv_sec < 0)
    {
        a.tv_sec = 0;
        a.tv_usec = 10;
    }
    
    if (a.tv_sec > TIMER_SECOND_MICRO)
        a.tv_sec = TIMER_SECOND_MICRO;
        
    return a;
}

static struct timeval
timeval_subtract(struct timeval a, struct timeval b)
{
    struct timeval ret;
    
    ret.tv_usec = a.tv_usec - b.tv_usec;
    ret.tv_sec = a.tv_sec - b.tv_sec;
    
    return timeval_adjust(ret);
}

static int
timeval_cmp(struct timeval a, struct timeval b)
{
    return (a.tv_sec == b.tv_sec
            ? a.tv_usec - b.tv_usec : a.tv_sec - b.tv_sec);
}

static unsigned long
timeval_elapsed(struct timeval a, struct timeval b)
{
    return (((a.tv_sec - b.tv_sec) * TIMER_SECOND_MICRO)
            + (a.tv_usec - b.tv_usec));
}

static unsigned int
cpu_record_hash_key(struct cpu_thread_history *a)
{
    return (uintptr_t) a->func;
}

static int
cpu_record_hash_cmp(struct cpu_thread_history *a,
                    struct cpu_thread_history *b)
{
    return a->func == b->func;
}

static void *
cpu_record_hash_alloc(struct cpu_thread_history *a)
{
    struct cpu_thread_history *new;
    new = XCALLOC(MTYPE_THREAD_STATS, sizeof(struct cpu_thread_history));
    new->func = a->func;
    new->funcname = XSTRDUP(MTYPE_THREAD_FUNCNAME, a->funcname);
    return new;
}
static inline void
vty_out_cpu_thread_history(struct vty *vty,
                           struct cpu_thread_history *a)
{
#ifdef HAVE_RUSAGE
    vty_out(vty, "%7ld.%03ld %9d %8ld %9ld %8ld %9ld",
            a->cpu.total / 1000, a->cpu.total % 1000, a->total_calls,
            a->cpu.total / a->total_calls, a->cpu.max,
            a->total / a->total_calls, a->max);
#else
    vty_out(vty, "%7ld.%03ld %9d %8ld %9ld",
            a->total / 1000, a->total % 1000, a->total_calls,
            a->total / a->total_calls, a->max);
#endif
    vty_out(vty, " %c%c%c%c%c%c %s%s",
            a->types & (1 << THREAD_READ) ? 'R' : ' ',
            a->types & (1 << THREAD_WRITE) ? 'W' : ' ',
            a->types & (1 << THREAD_TIMER) ? 'T' : ' ',
            a->types & (1 << THREAD_EVENT) ? 'E' : ' ',
            a->types & (1 << THREAD_EXECUTE) ? 'X' : ' ',
            a->types & (1 << THREAD_BACKGROUND) ? 'B' : ' ',
            a->funcname, VTY_NEWLINE);
}
static void
cpu_record_hash_print(struct hash_backet *bucket,
                      void *args[])
{
    struct cpu_thread_history *totals = args[0];
    unsigned char *filter = args[2];
    struct cpu_thread_history *a = bucket->data;
    a = bucket->data;
    
    if (!(a->types & *filter))
        return;
        
    /*  vty_out_cpu_thread_history(vty,a); */
    totals->total += a->total;
    totals->total_calls += a->total_calls;
    
    if (totals->max < a->max)
        totals->max = a->max;
}
static void
cpu_record_print(struct vty *vty, thread_type filter)
{
    struct cpu_thread_history tmp;
    void *args[3] = {&tmp, vty, &filter};
    memset(&tmp, 0, sizeof tmp);
    tmp.funcname = (char *) "TOTAL";
    tmp.types = filter;
#ifdef HAVE_RUSAGE
    vty_out(vty, "%21s %18s %18s%s",
            "", "CPU (user+system):", "Real (wall-clock):", VTY_NEWLINE);
#endif
    vty_out(vty, "Runtime(ms)   Invoked Avg uSec Max uSecs");
#ifdef HAVE_RUSAGE
    vty_out(vty, " Avg uSec Max uSecs");
#endif
    vty_out(vty, "  Type  Thread%s", VTY_NEWLINE);
    hash_iterate(cpu_record,
                 (void ( *)(struct hash_backet *, void *)) cpu_record_hash_print,
                 args);
                 
    if (tmp.total_calls > 0)
        vty_out_cpu_thread_history(vty, &tmp);
}
DEFUN(show_thread_cpu,
      show_thread_cpu_cmd,
      "show thread cpu [FILTER]",
      SHOW_STR
      "Thread information\n"
      "Thread CPU usage\n"
      "Display filter (rwtexb)\n")
{
    self = self;
    int i = 0;
    thread_type filter = (thread_type) - 1U;
    
    if (argc > 0)
    {
        filter = 0;
        
        while (argv[0][i] != '\0')
        {
            switch (argv[0][i])
            {
                case 'r':
                case 'R':
                    filter |= (1 << THREAD_READ);
                    break;
                    
                case 'w':
                case 'W':
                    filter |= (1 << THREAD_WRITE);
                    break;
                    
                case 't':
                case 'T':
                    filter |= (1 << THREAD_TIMER);
                    break;
                    
                case 'e':
                case 'E':
                    filter |= (1 << THREAD_EVENT);
                    break;
                    
                case 'x':
                case 'X':
                    filter |= (1 << THREAD_EXECUTE);
                    break;
                    
                case 'b':
                case 'B':
                    filter |= (1 << THREAD_BACKGROUND);
                    break;
                    
                default:
                    break;
            }
            
            ++i;
        }
        
        if (filter == 0)
        {
            vty_out(vty, "Invalid filter \"%s\" specified,"
                    " must contain at least one of 'RWTEXB'%s",
                    argv[0], VTY_NEWLINE);
            return CMD_WARNING;
        }
    }
    
    cpu_record_print(vty, filter);
    return CMD_SUCCESS;
}
/* Allocate new thread master.  */
struct thread_master *
thread_master_create()
{
    if (cpu_record == NULL)
    {
        cpu_record = hash_create_size(1011, cpu_record_hash_key, cpu_record_hash_cmp);
    }
    
    return (struct thread_master *) XCALLOC(MTYPE_THREAD_MASTER,
                                            sizeof(struct thread_master));
}

/* Add a new thread to the list.  */
static void
thread_list_add(struct thread_list *list, struct thread *thread)
{
    thread->next = NULL;
    thread->prev = list->tail;
    
    if (list->tail)
        list->tail->next = thread;
        
    else
        list->head = thread;
        
    list->tail = thread;
    list->count++;
}

/* Add a new thread just before the point.  */
static void
thread_list_add_before(struct thread_list *list,
                       struct thread *point,
                       struct thread *thread)
{
    thread->next = point;
    thread->prev = point->prev;
    
    if (point->prev)
        point->prev->next = thread;
        
    else
        list->head = thread;
        
    point->prev = thread;
    list->count++;
}

/* Delete a thread from the list. */
static struct thread *
thread_list_delete(struct thread_list *list, struct thread *thread)
{
    if (thread->next)
        thread->next->prev = thread->prev;
        
    else
        list->tail = thread->prev;
        
    if (thread->prev)
        thread->prev->next = thread->next;
        
    else
        list->head = thread->next;
        
    thread->next = thread->prev = NULL;
    list->count--;
    return thread;
}

/* Move thread to unuse list. */
static void
thread_add_unuse(struct thread_master *m, struct thread *thread)
{
    assert(m != NULL);
    assert(thread->next == NULL);
    assert(thread->prev == NULL);
    assert(thread->type == THREAD_UNUSED);
    thread_list_add(&m->unuse, thread);
    /* XXX: Should we deallocate funcname here? */
}

/* Delete top of the list and return it. */
struct thread *
thread_trim_head(struct thread_list *list)
{
    if (list->head)
        return thread_list_delete(list, list->head);
        
    return NULL;
}

/* Trim blankspace and "()"s */
static char *
strip_funcname(const char *funcname)
{
    char buff[100];
    char tmp, *ret, *e, *b = buff;
    strncpy(buff, funcname, sizeof(buff));
    buff[ sizeof(buff) - 1] = '\0';
    e = buff + strlen(buff) - 1;
    
    /* Wont work for funcname ==  "Word (explanation)"  */
    
    while (*b == ' ' || *b == '(')
        ++b;
        
    while (*e == ' ' || *e == ')')
        --e;
        
    e++;
    tmp = *e;
    *e = '\0';
    ret  = XSTRDUP(MTYPE_THREAD_FUNCNAME, b);
    *e = tmp;
    return ret;
}

/* Get new thread.  */
static struct thread *
thread_get(struct thread_master *m, u_char type,
           int (*func)(struct thread *), void *arg, const char *funcname)
{
    struct thread *thread;
    
    if (m->unuse.head)
    {
        thread = thread_trim_head(&m->unuse);
        
        if (thread->funcname)
            XFREE(MTYPE_THREAD_FUNCNAME, thread->funcname);
    }
    else
    {
        thread = XCALLOC(MTYPE_THREAD, sizeof(struct thread));
        m->alloc++;
    }
    
    thread->type = type;
    thread->add_type = type;
    thread->master = m;
    thread->func = func;
    thread->arg = arg;
    
    thread->funcname = strip_funcname(funcname);
    
    return thread;
}

/* Add new read thread. */
struct thread *
funcname_thread_add_read(struct thread_master *m,
                         int (*func)(struct thread *), void *arg, int fd, const char *funcname)
{
    struct thread *thread;
    
    assert(m != NULL);
    
    if (FD_ISSET(fd, &m->readfd))
    {
        fprintf(stderr, "Warning! There is already read fd [%d]\n", fd);
        return NULL;
    }
    
    thread = thread_get(m, THREAD_READ, func, arg, funcname);
    FD_SET((unsigned int) fd, &m->readfd);
    thread->u.fd = fd;
    thread_list_add(&m->read, thread);
    
    return thread;
}

/* Add new write thread. */
struct thread *
funcname_thread_add_write(struct thread_master *m,
                          int (*func)(struct thread *), void *arg, int fd, const char *funcname)
{
    struct thread *thread;
    
    assert(m != NULL);
    
    if (FD_ISSET(fd, &m->writefd))
    {
        fprintf(stderr, "Warning! There is already write fd [%d]\n", fd);
        return NULL;
    }
    
    thread = thread_get(m, THREAD_WRITE, func, arg, funcname);
    FD_SET((unsigned int) fd, &m->writefd);
    thread->u.fd = fd;
    thread_list_add(&m->write, thread);
    
    return thread;
}


static struct thread *
funcname_thread_add_timer_timeval(struct thread_master *m,
                                  int (*func)(struct thread *),
                                  void *arg,
                                  struct timeval *time_relative,
                                  const char *funcname)
{
    struct thread *thread;
    struct timeval timer_now;
#ifndef TIMER_NO_SORT
    struct thread *tt;
#endif /* TIMER_NO_SORT */
    
    assert(m != NULL);
    
    thread = thread_get(m, THREAD_TIMER, func, arg, funcname);
    
    /* Do we need jitter here? */
    gettimeofday(&timer_now, NULL);
    timer_now.tv_sec += time_relative->tv_sec;
    timer_now.tv_usec += time_relative->tv_usec;
    timeval_adjust(timer_now);
    thread->u.sands = timer_now;
    
    /* Sort by timeval. */
#ifdef TIMER_NO_SORT
    thread_list_add(&m->timer, thread);
#else
    
    for (tt = m->timer.head; tt; tt = tt->next)
        if (timeval_cmp(thread->u.sands, tt->u.sands) <= 0)
            break;
    
    if (tt)
        thread_list_add_before(&m->timer, tt, thread);
    
    else
        thread_list_add(&m->timer, thread);
    
#endif /* TIMER_NO_SORT */
    
    return thread;
}

/* Add timer event thread. */
struct thread *
funcname_thread_add_timer(struct thread_master *m,
                          int (*func)(struct thread *),
                          void *arg, long timer, const char *funcname)
{
    struct timeval trel;
    
    assert(m != NULL);
    
    trel.tv_sec = timer;
    trel.tv_usec = 0;
    
    return funcname_thread_add_timer_timeval(m, func, arg, &trel, funcname);
}

/* Add timer event thread with "millisecond" resolution */
struct thread *
funcname_thread_add_timer_msec(struct thread_master *m,
                               int (*func)(struct thread *),
                               void *arg, long timer, const char *funcname)
{
    struct timeval trel;
    
    assert(m != NULL);
    
    trel.tv_sec = timer / 1000;
    trel.tv_usec = 1000 * (timer % 1000);
    
    return funcname_thread_add_timer_timeval(m, func, arg, &trel, funcname);
}


/* Add simple event thread. */
struct thread *
funcname_thread_add_event(struct thread_master *m,
                          int (*func)(struct thread *), void *arg, int val, const char *funcname)
{
    struct thread *thread;
    
    assert(m != NULL);
    
    thread = thread_get(m, THREAD_EVENT, func, arg, funcname);
    thread->u.val = val;
    thread_list_add(&m->event, thread);
    
    return thread;
}

void
thread_cancel(struct thread *thread)
{
    switch (thread->type)
    {
        case THREAD_READ:
            assert(FD_ISSET(thread->u.fd, &thread->master->readfd));
            FD_CLR((unsigned int) thread->u.fd, &thread->master->readfd);
            thread_list_delete(&thread->master->read, thread);
            break;
            
        case THREAD_WRITE:
            assert(FD_ISSET(thread->u.fd, &thread->master->writefd));
            FD_CLR((unsigned int) thread->u.fd, &thread->master->writefd);
            thread_list_delete(&thread->master->write, thread);
            break;
            
        case THREAD_TIMER:
            thread_list_delete(&thread->master->timer, thread);
            break;
            
        case THREAD_EVENT:
            thread_list_delete(&thread->master->event, thread);
            break;
            
        case THREAD_READY:
            thread_list_delete(&thread->master->ready, thread);
            break;
            
        default:
            break;
    }
    
    thread->type = THREAD_UNUSED;
    thread_add_unuse(thread->master, thread);
}
struct timeval *
thread_timer_wait(struct thread_master *m, struct timeval *timer_val)
{
    struct timeval timer_now;
    struct timeval timer_min;
    
    if (m->timer.head)
    {
        gettimeofday(&timer_now, NULL);
        timer_min = m->timer.head->u.sands;
        timer_min = timeval_subtract(timer_min, timer_now);
        
        if (timer_min.tv_sec < 0)
        {
            timer_min.tv_sec = 0;
            timer_min.tv_usec = 10;
        }
        
        *timer_val = timer_min;
        return timer_val;
    }
    
    return NULL;
}

struct thread *
thread_run(struct thread_master *m, struct thread *thread,
           struct thread *fetch)
{
    *fetch = *thread;
    thread->type = THREAD_UNUSED;
    thread_add_unuse(m, thread);
    return fetch;
}

int
thread_process_fd(struct thread_master *m, struct thread_list *list,
                  fd_set *fdset, fd_set *mfdset)
{
    struct thread *thread;
    struct thread *next;
    int ready = 0;
    
    for (thread = list->head; thread; thread = next)
    {
        next = thread->next;
        
        if (FD_ISSET(THREAD_FD(thread), fdset))
        {
            assert(FD_ISSET(THREAD_FD(thread), mfdset));
            FD_CLR((unsigned int) THREAD_FD(thread), mfdset);
            thread_list_delete(list, thread);
            thread_list_add(&m->ready, thread);
            thread->type = THREAD_READY;
            ready++;
        }
    }
    
    return ready;
}

/* Fetch next ready thread. */
struct thread *
thread_fetch(struct thread_master *m, struct thread *fetch)
{
    int num;
    /** int ready; */
    struct thread *thread;
    fd_set readfd;
    fd_set writefd;
    fd_set exceptfd;
    struct timeval timer_now;
    struct timeval timer_val;
    struct timeval *timer_wait;
    
    /**
    struct timeval timer_nowait;
    
    timer_nowait.tv_sec = 0;
    timer_nowait.tv_usec = 0; */
    
    while (1)
    {
        /* Signals are highest priority */
        /*      quagga_sigevent_process ();  */
        
        /* Normal event are the next highest priority.  */
        if ((thread = thread_trim_head(&m->event)) != NULL)
            return thread_run(m, thread, fetch);
            
        /* Execute timer.  */
        gettimeofday(&timer_now, NULL);
        
        for (thread = m->timer.head; thread; thread = thread->next)
            if (timeval_cmp(timer_now, thread->u.sands) >= 0)
            {
                thread_list_delete(&m->timer, thread);
                return thread_run(m, thread, fetch);
            }
            
        /* If there are any ready threads, process top of them.  */
        if ((thread = thread_trim_head(&m->ready)) != NULL)
            return thread_run(m, thread, fetch);
            
        /* Structure copy.  */
        readfd = m->readfd;
        writefd = m->writefd;
        exceptfd = m->exceptfd;
        /* Calculate select wait timer. */
        timer_wait = thread_timer_wait(m, &timer_val);
        num = select(FD_SETSIZE, &readfd, &writefd, &exceptfd, timer_wait);
        
        if (num == 0)
            continue;
            
        if (num < 0)
        {
            if (errno == EINTR)
            {
                /* signal received */
                /* quagga_sigevent_process ();  */
                continue;
            }
            
            /** fprintf(stderr, "Warning! select() error: %s\r\n", safe_strerror(errno)); */
            return NULL;
        }
        
        /* Normal priority read thead. */
        /** ready = */thread_process_fd(m, &m->read, &readfd, &m->readfd);
        /* Write thead. */
        /*ready = */
        thread_process_fd(m, &m->write, &writefd, &m->writefd);
        
        if ((thread = thread_trim_head(&m->ready)) != NULL)
            return thread_run(m, thread, fetch);
    }
}

static unsigned long
thread_consumed_time(RUSAGE_T *now, RUSAGE_T *start)
{
    unsigned long thread_time;
#ifdef HAVE_RUSAGE
    /* This is 'user + sys' time.  */
    thread_time = timeval_elapsed(now->ru_utime, start->ru_utime);
    thread_time += timeval_elapsed(now->ru_stime, start->ru_stime);
#else
    /* When rusage is not available, simple elapsed time is used.  */
    thread_time = timeval_elapsed(*now, *start);
#endif /* HAVE_RUSAGE */
    return thread_time;
}


/* We check thread consumed time. If the system has getrusage, we'll
   use that to get indepth stats on the performance of the thread.  If
   not - we'll use gettimeofday for some guestimation.  */
void
thread_call(struct thread *thread)
{
    unsigned long thread_time;
    RUSAGE_T ru;
    struct cpu_thread_history tmp, *cpu;
    tmp.func = thread->func;
    tmp.funcname = thread->funcname;
    cpu = hash_get(cpu_record, &tmp, cpu_record_hash_alloc);
    GETRUSAGE(&thread->ru);
    (*thread->func)(thread);
    GETRUSAGE(&ru);
    thread_time = thread_consumed_time(&ru, &thread->ru);
    cpu->total += thread_time;
    
    if (cpu->max < thread_time)
        cpu->max = thread_time;
        
    ++cpu->total_calls;
    cpu->types |= (1 << thread->add_type);
#ifdef THREAD_CONSUMED_TIME_CHECK
    
    if (thread_time > 200000L)
    {
        /*
         * We have a CPU Hog on our hands.
         * Whinge about it now, so we're aware this is yet another task
         * to fix.
         */
        zlog_err("CPU HOG task %s (%lx) ran for %ldms",
                 thread->funcname,
                 (unsigned long) thread->func,
                 thread_time / 1000L);
    }
    
#endif /* THREAD_CONSUMED_TIME_CHECK */
}

void re_calculate_all_time(unsigned int old_time, unsigned int new_time)
{
    old_time = old_time;
    new_time = new_time;
    
#if 0
    struct thread *timer_thread;
    unsigned int time_diff = new_time - old_time;
    /*  printf("old_time = %d, new_time = %d\n", old_time, new_time);*/
    timer_thread = master->timer.head;
    
    while (timer_thread)
    {
        timer_thread->u.sands.tv_sec += time_diff;
        timer_thread = timer_thread->next;
    }
    
#endif
}

