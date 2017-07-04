#include "sysdefs.h"
#include "rt_throughput_exce.h"
#include "rt_json.h"

#define RT_THROUGHPUT_DEBUG
#define RT_THROUGHPUT_SAMPLE_INTERVAL    1000000
#define RT_THROUGHPUT_ARCHIVE_INTERVAL   3000000
#define mutiple  4
#define BUF_LEN 64
#define FILE_BUF_LEN 128

#define rtss_printf(fmt,args...) //printf(fmt,##args)
#define rtss_exce_foreach_entry(el, entry)  \
    for(entry = ((struct rt_throughput_exce_list *)(el))->head; entry; entry = entry->next)
        
static struct rt_throughput_exce_mgmt *RT_exce_throughput;

static inline void *tp_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

static inline void *__attribute__((always_inline))
rt_exce_throughput_list_init()
{
    struct rt_throughput_exce_list *el;

    el = (struct rt_throughput_exce_list *)tp_kmalloc(sizeof(struct rt_throughput_exce_list));
    if(likely(el)){
        el->count = 0;
        el->head = el->tail = 0;
        rt_mutex_init(&el->entries_mtx, NULL);
    }
    
    return el;
}

static inline void __attribute__((always_inline))
rt_exce_throughput_list_add(struct rt_throughput_exce_list *list, struct rt_throughput_exce_entry *entry)
{
    entry->next = NULL;
    entry->prev = list->tail;

    if (list->tail)
        list->tail->next = entry;
    else
        list->head = entry;

    list->tail = entry;
    list->count++;
}

static inline struct rt_throughput_exce_entry *__attribute__((always_inline))
rt_exce_throughput_list_del(struct rt_throughput_exce_list *list, struct rt_throughput_exce_entry *entry)
{
    if (entry->next)
        entry->next->prev = entry->prev;
    else
        list->tail = entry->prev;

    if (entry->prev)
        entry->prev->next = entry->next;
    else
        list->head = entry->next;

    entry->next = entry->prev = NULL;
    list->count--;
    return entry;
}

static inline struct rt_throughput_exce_entry *__attribute__((always_inline))
rt_exce_throughput_list_del_and_free(struct rt_throughput_exce_list *list, 
    struct rt_throughput_exce_entry *entry, 
    void (*free_fn)(void *))
{
    if (entry->next)
        entry->next->prev = entry->prev;
    else
        list->tail = entry->prev;

    if (entry->prev)
        entry->prev->next = entry->next;
    else
        list->head = entry->next;

    entry->next = entry->prev = NULL;
    list->count--;

    if (free_fn) {
        free_fn (entry);
        entry = NULL;
    }
    return entry;
}

static inline struct rt_throughput_exce_entry *__attribute__((always_inline))
rt_exce_throughput_list_trim_head(struct rt_throughput_exce_list *list)
{
    if (list->head)
        return rt_exce_throughput_list_del(list, list->head);

    return NULL;
}

static inline struct rt_throughput_exce_entry *__attribute__((always_inline))
rt_exce_throughput_list_trim_tail(struct rt_throughput_exce_list *list)
{
    if (list->head)
        return rt_exce_throughput_list_del(list, list->tail);

    return NULL;
}

static inline size_t
rt_exce_throughput_entry_format(char *result, int len,
    uint64_t router __attribute__((__unused__)), uint64_t port __attribute__((__unused__)),
    void *xentry)
{
    size_t rsize = 0;
    char date[32] = {0};

    struct rt_throughput_exce_entry *entry = xentry;
    if(!result || !entry)
        goto finish;

    rt_tms2str (entry->tm, EVAL_TM_STYLE_EXT, date, 32);

	rsize = snprintf(result, len - 1, "------------");

#if 0    
    rsize = snprintf(result, len - 1, "%s\"%s\" in:{%s%lu %s%f %s%lu %s%f} out:{%s%lu %s%f %s%lu %s%f}\n",
                     "time:", date,
                     "bs:", entry->throughput.bs.in.iv,
                     "bps:", entry->throughput.bps.in.fv,
                     "ps:", entry->throughput.ps.in.iv,
                     "pps:", entry->throughput.pps.in.fv,
                     "bs:", entry->throughput.bs.out.iv,
                     "bps:", entry->throughput.bps.out.fv,
                     "ps:", entry->throughput.ps.out.iv,
                     "pps:", entry->throughput.pps.out.fv);
#endif

finish:
    return rsize;
}

static void *
rt_exce_throughput_param()
{
    return RT_exce_throughput;
}

static int __attribute__((__unused__))
rt_exce_throughput_retrieve(struct rt_throughput_exce_list *el, 
    const char __attribute__((__unused__))*desc)
{
    struct rt_throughput_exce_entry *entry = NULL;
    char entry_str[4096] = {0};

    rtss_printf("\n\n\n============= %s cache (%d entries) ==============\n", desc ? desc : "ANY", el->count);
    rtss_exce_foreach_entry(el, entry) {
        rt_exce_throughput_entry_format(entry_str, 4096, 0, 0, entry);
        rtss_printf("%s", entry_str);
    }

    return 0;
}

static int
rt_exce_throughput_cache(
    struct rt_throughput_exce_list *rrl,
    void *entry)
{
    rt_mutex_lock(&rrl->entries_mtx);
    rt_exce_throughput_list_add(rrl, entry);
    rt_mutex_unlock(&rrl->entries_mtx);
#ifdef RTSS_DETAILED_DEBUG
    rtss_printf("[A]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, rrl->count);
#endif
    return XSUCCESS;
}

static inline void *
rt_exce_throughput_alloc(struct rt_throughput_exce_entry **rtss)
{
    *rtss = (struct rt_throughput_exce_entry *)tp_kmalloc(sizeof(struct rt_throughput_exce_entry));
     return *rtss;
}

static int
rt_exce_throughput_cleanup(void *xel)
{
    struct rt_throughput_exce_entry *entry, *s, *next;
    struct rt_throughput_exce_list *el = xel;
#ifdef RT_THROUGHPUT_DEBUG
    int __attribute__((__unused__))count = el->count;
    int rcount = 0;
#endif

    rt_mutex_lock(&el->entries_mtx);
    for (s = el->head; s; s = next)
    {
        next = s->next;
        entry = s;
       #ifdef RTSS_DETAILED_DEBUG
        rtss_printf("[D]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, el->count);
       #endif
       #ifdef RT_THROUGHPUT_DEBUG
       rcount ++;
       #endif
        rt_exce_throughput_list_del_and_free(el, entry, free);
    }
    rt_mutex_unlock(&el->entries_mtx);

#ifdef RT_THROUGHPUT_DEBUG
    rtss_printf("[D]    %d entries, remain=%d\n",
        rcount, count-rcount);
#endif
    entry = s = next = NULL;

    return 0;
}

static void
rt_exce_throughput_mk_file(uint64_t router, uint64_t port,
    const char *path, const char *prefix, char *fname, size_t len)
{
    declare_array(char, file, FILE_BUF_LEN);
    declare_array(char, realpath, FILE_BUF_LEN);

    snprintf (realpath, FILE_BUF_LEN - 1, "%s/routers/r%lu/p%lu/%s", path, router, port, prefix);

    if (!rt_dir_exsit(realpath)){
        declare_array(char, mkdir, FILE_BUF_LEN);
        snprintf (mkdir, FILE_BUF_LEN - 1, "mkdir -p %s", realpath);
        do_system (mkdir);
    }
    
    rt_file_mk_by_time(NULL, EVAL_TM_STYLE, file, FILE_BUF_LEN);
    snprintf (fname, len - 1, "%s/%s_%s", realpath, prefix, file);
}

static int rt_exce_throughput_write_file(FILE *fp, 
    uint64_t router, uint64_t port, void *entry, 
    size_t (*format_fn)(char *, int, uint64_t, uint64_t, void *))
{
    declare_variable(char *, p, NULL);
    declare_variable(size_t, size, 0);
    
    if (!format_fn)
        goto finish;
    
    rt_kmalloc((void **)&p, mutiple * sizeof (struct rt_throughput_exce_entry));
    size = format_fn (p, mutiple * sizeof (struct rt_throughput_exce_entry),  router, port, entry);
    BUG_ON(rt_file_write(fp, 
        (void *)p, size, NULL) == EOF);
    free(p);
finish:
    return 0;        
}

static int rt_exce_throughput_write_disk(const char *realpath, 
    void (*rt_throughput_mkfile_fn)(uint64_t, uint64_t,
    const char *, const char *, char *, size_t),
    struct rt_throughput_exce_list *el)
{
    FILE *fp = NULL;
    declare_array(char, file, FILE_BUF_LEN);
    struct rt_throughput_exce_entry *entry, *s, *next;
#ifdef RT_THROUGHPUT_DEBUG
    int __attribute__((__unused__))count = el->count;
    int rcount = 0;
#endif

    rt_throughput_mkfile_fn(el->router, el->port, realpath, "RT_throughput_exception", file, FILE_BUF_LEN);
    rt_file_open(file, &fp);
    if(!fp) goto finish;
    
    for (s = el->head; s; s = next)
    {
        next = s->next;
        entry = s;
        
 #ifdef RT_THROUGHPUT_DEBUG
     #ifdef RTSS_DETAILED_DEBUG
        rtss_printf("[W]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, el->count);
     #endif
       rcount ++;
 #endif
    
        rt_exce_throughput_write_file(fp, 
            el->router, el->port, entry, 
            rt_exce_entry_to_string);
 
        rt_exce_throughput_list_del_and_free(el, entry, free);
    }

#ifdef RT_THROUGHPUT_DEBUG
    rtss_printf("[W]    file=\"%s\", %d entries, remain=%d\n",
        file, rcount, count-rcount);
#endif

    fclose(fp);
finish:
    return 0;
}


/** Rewrite cache evaluation entry to files */
static int
rt_exce_throughput_flush(struct rt_throughput_exce_list *el, const char *path)
{
    if (el->count <= 0)
        goto finish;
    
    rt_mutex_lock(&el->entries_mtx);
    rt_exce_throughput_write_disk(path, rt_exce_throughput_mk_file, el);
    rt_mutex_unlock(&el->entries_mtx);

finish:    
    return 0;
}

static inline void
rt_throughput_exce_load_selective(
    FILE *fp,
    char *from, char *to, uint64_t interval,
   json_object *exce_array_obj)
{
#define LINE_SIZE   (mutiple * sizeof (struct rt_throughput_exce_entry))

    declare_array(char, line, LINE_SIZE);
    declare_array(char, timestamp, BUF_LEN);
    rt_kmalloc((void**)&line, LINE_SIZE);
    
    const int times = (interval * 1000000)/RT_THROUGHPUT_SAMPLE_INTERVAL;
    int xtimes __attribute__((__unused__)) = 0;

    xtimes = times;
    while(fgets(line, LINE_SIZE - 1, fp))
    {
        sscanf(line, "%*[^:]:%*[^\"]\"%[^\"]\"", timestamp);
        if (strcmp(timestamp, from) < 0){
            continue;
        }

        if (strcmp(to, timestamp) < 0){
            printf("timestamp = %s\n", timestamp);
            goto finish;
        }

        json_object_array_add(exce_array_obj, json_tokener_parse(line));
        clear_memory(timestamp, BUF_LEN);
        clear_memory(line, LINE_SIZE);
    }
finish:
    return;
}

static int 
rt_exce_throughput_load(uint64_t router ,
    uint64_t port ,
    uint64_t start_ts , uint64_t end_ts ,
    uint64_t interval ,json_object *exce_array_obj)
{
    uint64_t tm = 0;
    int xret = XSUCCESS;
    FILE *fp = NULL;
    declare_array(char, sf, BUF_LEN);
    declare_array(char, ef, BUF_LEN);
    declare_array(char, key_sf, BUF_LEN);
    declare_array(char, key_ef, BUF_LEN);
    declare_array(char, sf_realpath, FILE_BUF_LEN);
    declare_array(char, ef_realpath, FILE_BUF_LEN);    
    declare_array(char, realpath, FILE_BUF_LEN);
    declare_array(char, file, FILE_BUF_LEN);
    
    char *prefix = "RT_throughput_exce";
    static const  uint64_t offset = 24 * 60 * 60;
    
     snprintf(realpath, FILE_BUF_LEN - 1, EVAL_FILE_PATH"/routers/r%lu/p%lu/%s", 
        router, port, prefix);
    
    rt_tms2str(start_ts, EVAL_TM_STYLE, sf, BUF_LEN);
    snprintf(sf_realpath, FILE_BUF_LEN - 1, "%s/%s_%s", realpath, prefix, sf);
    
    rt_tms2str(end_ts, EVAL_TM_STYLE, ef, BUF_LEN);
    snprintf(ef_realpath, FILE_BUF_LEN - 1, "%s/%s_%s", realpath, prefix, ef);
    
    rt_tms2str(start_ts, EVAL_TM_STYLE_FULL, key_sf, BUF_LEN);
    rt_tms2str(end_ts, EVAL_TM_STYLE_FULL, key_ef, BUF_LEN);
    
    /** 3rd. */
    if(!rt_file_exsit(sf_realpath) &&
        !rt_file_exsit(ef_realpath)){
        xret = (-ERRNO_THROUGHPUT_NO_FILE);
        goto finish;
    }
    
#ifdef RT_THROUGHPUT_DEBUG
        rtss_printf("[P]    started file=\"%s\", ended file=\"%s\"\n",  sf_realpath, ef_realpath);
#endif
    
    if (!strcmp(sf, ef)){
        fp = fopen(sf_realpath, "r");
        assert(fp != NULL);
        rt_throughput_exce_load_selective(fp, key_sf, key_ef, interval, exce_array_obj);
        fclose(fp);
    }else{
            tm = start_ts;
            while(tm < (end_ts + offset)){            
                snprintf(file, FILE_BUF_LEN - 1, "%s/%s_%s", realpath, prefix, sf);
                if(rt_file_exsit(file)){
                    fp = fopen(file, "r");
                    assert(fp != NULL);
                    
#ifdef RT_THROUGHPUT_DEBUG
                rtss_printf("[F]    related file=\"%s\"\n",  file);
#endif
                    rt_throughput_exce_load_selective(fp, key_sf, key_ef, interval, exce_array_obj);              
                    fclose(fp);
                 }
                
                tm += offset;
                clear_memory(sf, BUF_LEN);
                rt_tms2str(tm, EVAL_TM_STYLE, sf, BUF_LEN);
                clear_memory(file, FILE_BUF_LEN);
            }
        goto finish;
}
   
finish:

    return xret;
}


int rt_exce_throughput_query(uint64_t router ,
    uint64_t port ,
    int cmd_type ,
    uint64_t start_ts , uint64_t end_ts ,
    uint64_t interval ,json_object *exce_array_obj)
{
    cmd_type = cmd_type;
    int xret = XSUCCESS;
    declare_variable(struct rt_throughput_exce_list *, el, RT_exce_throughput->cache);
    declare_variable(char *, path, RT_exce_throughput->default_path);
    declare_variable(struct rt_throughput_exce_entry *, entry, NULL);

    if ((start_ts == end_ts) &&
            (start_ts == 0)){
            rt_mutex_lock(&el->entries_mtx);
            entry = el->tail;
            json_object_array_add(exce_array_obj, 
                    rt_exce_entry_to_object(entry, router, port));
            rt_mutex_unlock(&el->entries_mtx);
    }
    else{ 
        rt_exce_throughput_flush(el, path);
        xret = rt_exce_throughput_load(router, port, start_ts, end_ts, 
                interval, exce_array_obj);
    }

    return xret;
}

static void THIS_API_IS_ONLY_FOR_TEST
rt_exce_throughput_generator(struct rt_throughput_exce_entry **rt_entry)
{
    rt_exce_throughput_alloc(rt_entry);
    srand(time(NULL));

    (*rt_entry)->tm = time(NULL);
    int i = 0;
    int val = 1000;
    int val1 = 100;
    
    for (i= 0; i<EXCE_MSG_THROUGHPUT; i++){
        (*rt_entry)->throughput[i].bs.in.iv = rand()%val;
        (*rt_entry)->throughput[i].bps.in.iv = rand()%val;
        (*rt_entry)->throughput[i].ps.in.iv = rand()%val;    
        (*rt_entry)->throughput[i].pps.in.iv = rand()%val;
        (*rt_entry)->throughput[i].bw_usage.in.iv = rand()%val;

        (*rt_entry)->throughput[i].bs.out.iv = rand()%val1;
        (*rt_entry)->throughput[i].bps.out.iv = rand()%val1;
        (*rt_entry)->throughput[i].ps.out.iv = rand()%val1;    
        (*rt_entry)->throughput[i].pps.out.iv = rand()%val1;
        (*rt_entry)->throughput[i].bw_usage.out.iv = rand()%val1;

        
        (*rt_entry)->throughput[i].bs.total.iv = rand()%val;
        (*rt_entry)->throughput[i].bps.total.iv = rand()%val;
        (*rt_entry)->throughput[i].ps.total.iv = rand()%val;    
        (*rt_entry)->throughput[i].pps.total.iv = rand()%val;
        (*rt_entry)->throughput[i].bw_usage.total.iv = rand()%val;

    }
	#if 0
    (*rt_entry)->throughput.bs.in.iv = 2;
    (*rt_entry)->throughput.bps.in.fv = 1;
    (*rt_entry)->throughput.ps.in.iv = 2;    
    (*rt_entry)->throughput.pps.in.fv = 1;
    (*rt_entry)->throughput.bw_usage.in.fv = 0.2;
    
    (*rt_entry)->throughput.bs.out.iv = 2;
    (*rt_entry)->throughput.bps.out.fv = 1;
    (*rt_entry)->throughput.ps.out.iv = 2;    
    (*rt_entry)->throughput.pps.out.fv = 1;
    (*rt_entry)->throughput.bw_usage.out.fv = 0.2;
	#endif
	
}

#define rt_throughput_foreach_router
static void *
rt_exce_throughput_do_cache(void *param)
{
    declare_variable(struct rt_throughput_exce_mgmt *, xrtss_mgmt, (struct rt_throughput_exce_mgmt *)param);
    declare_variable(struct rt_throughput_exce_list *, rt_list, NULL);
    declare_variable(struct rt_throughput_exce_entry *, rt_entry, NULL);
    
    FOREVER {
        rt_list = xrtss_mgmt->cache;
        rt_throughput_foreach_router {
            rt_exce_throughput_generator(&rt_entry);
            rt_exce_throughput_cache(rt_list, rt_entry);      
            //rt_message_throughput_retrieve(rt_list, "MAIN RT_throughput");
        }
        usleep(xrtss_mgmt->sample_threshold);
    }
    
    return NULL;
}

static void *
rt_exce_throughput_do_archive(void *param)
{
    declare_variable(struct rt_throughput_exce_mgmt *, xrtss_mgmt, (struct rt_throughput_exce_mgmt *)param);
        
    FOREVER {
        usleep(xrtss_mgmt->flush_threshold);
        STDIO_RENDERING_STD_ERROR_AND_CONTINUE(
            rt_exce_throughput_flush(xrtss_mgmt->cache, xrtss_mgmt->default_path));
    }
    return NULL;
}

static void
rt_exce_throughput_run_init()
{
    RT_exce_throughput = tp_kmalloc(sizeof(struct rt_throughput_exce_mgmt));
    if(likely(RT_exce_throughput)){
        RT_exce_throughput->cache = rt_exce_throughput_list_init();
        RT_exce_throughput->flush_threshold  = RT_THROUGHPUT_ARCHIVE_INTERVAL;
        RT_exce_throughput->sample_threshold = RT_THROUGHPUT_SAMPLE_INTERVAL;
        snprintf (RT_exce_throughput->default_path, PATH_SIZE - 1, "%s", EVAL_FILE_PATH);
    }
}

static struct eval_template 
rt_exce_throughput_template = {
    {
        .desc = "real time exception message distrition",
        .type = TEMP_RTSS_EXCEPTION,
        .init = rt_exce_throughput_run_init,
        .param = rt_exce_throughput_param,
    },
    .cache = NULL,
    .retrieve = NULL,
    .cleanup = rt_exce_throughput_cleanup,
    .cache_ctx = rt_exce_throughput_do_cache,
    .archive_ctx = rt_exce_throughput_do_archive
};

void
rt_throughput_exce_init()
{
    template_install(TEMP_RTSS_EXCEPTION, &rt_exce_throughput_template);
    return;
}

