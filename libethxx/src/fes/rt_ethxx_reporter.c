#include "sysdefs.h"
#include "rt_ethxx_pcap.h"
#include "rt_ethxx_reporter.h"

static LIST_HEAD(report_list);
static INIT_MUTEX(report_lock);

struct rt_report_t{
    int ready;
    #define SNAP_FNAME_LENGTH  256
    char *file;
};

static struct rt_pool_t *report_mempool;

#define REPORT_INITIALIZE(p) do { \
            (p)->ready = 0;\
         } while (0)

static inline void *report_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

static void *
report_alloc()
{
    struct rt_report_t *p;

    p = (struct rt_report_t *)report_kmalloc(sizeof(struct rt_report_t));
    if (likely(p)) {
        REPORT_INITIALIZE(p);
        p->file = (char *)report_kmalloc(SNAP_FNAME_LENGTH);
        if (unlikely(!p->file)) {
            free(p);
            p = NULL;
        }
    }

    return p;
}

static void
report_free(void *priv_data)
{
    struct rt_report_t *p = (struct rt_report_t *)priv_data;

    if(likely(p)){
        if(likely(p->file))
            free(p->file);
        free(p);
    }
}

void report_lineup(const char __attribute__((__unused__))*path,
    size_t __attribute__((__unused__))ps,
    const char *file,
    size_t __attribute__((__unused__))fs)
{
    struct rt_report_t *report_data = NULL;

    struct rt_pool_bucket_t *bucket = rt_pool_bucket_get_new(report_mempool, NULL);
    if (likely(bucket)){
        report_data = (struct rt_report_t *)bucket->priv_data;
        if (likely(report_data)){
            SNPRINTF(report_data->file, SNAP_FNAME_LENGTH - 1, "%s", file);
            report_data->ready = 1;
            rt_mutex_lock(&report_lock);
            list_add_tail(&bucket->list, &report_list);
            rt_mutex_unlock(&report_lock);
            REPORTER_EQ_ADD(1);
        }else{
            rt_pool_bucket_push(report_mempool,
                bucket);
        }
    }
}

static void keep_silence()
{
    ;
}

static inline void 
report_further_proc(struct rt_report_t *bucket,
    void(*routine)(char *file, void *resv), void *argument)
{
    bucket->ready ? routine(bucket->file, argument) : keep_silence();
}

int
rt_report_proc(void __attribute__((__unused__))*param0,
               void __attribute__((__unused__))*param1,
               void(*routine)(char *file, void *resv), void *argument)
{
    volatile int counter = 0;
    struct rt_pool_bucket_t *this, *p;
    
    rt_mutex_lock(&report_lock);
    list_for_each_entry_safe(this, p, &report_list, list){
        list_del(&this->list);
        REPORTER_DQ_ADD(1);
        report_further_proc((struct rt_report_t *)this->priv_data,
            routine, argument);
        rt_pool_bucket_push (report_mempool, this);
        counter++;
    }
    rt_mutex_unlock(&report_lock);
    
    return counter;
}

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)

static void
rt_ethxx_do_job(char *file, 
    void __attribute__((__unused__)) *resv)
{
    if (file){
        file = file;
        #if 0
        FILE *fp = fopen("trash.txt", "a+");
        rt_file_write(fp, file, 0, NULL);
        fclose(fp);
        #else
        rt_ethxx_pcap_del_disk(file, 0);
        #endif
    }
}

static void *
rt_ethxx_report_proc(void __attribute__((__unused__))*argvs)
{
   FOREVER{
       rt_report_proc(NULL, NULL, rt_ethxx_do_job, NULL);
   }
   return NULL;
}

static struct rt_task_t rt_report_proc_task =
{
    .module = THIS,
    .name = "Advanced Pcap File Report Task",
    .core = INVALID_CORE,
    .prio = KERNEL_SCHED,
    .argvs = NULL,
    .routine = rt_ethxx_report_proc,
};
#endif

void
rt_ethxx_reporter_init(int buckets)
{

    report_mempool = rt_pool_initialize(buckets, report_alloc, \
                                           report_free,\
                                           0);

#if SPASR_BRANCH_EQUAL(BRANCH_LOCAL)
    task_registry(&rt_report_proc_task);
#endif
}


