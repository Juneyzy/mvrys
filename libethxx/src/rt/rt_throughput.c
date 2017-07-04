#include "sysdefs.h"
#include "rt_throughput.h"
#include "rt_json.h"
#include "json_object.h"

#define RT_THROUGHPUT_DEBUG
#define RT_THROUGHPUT_SAMPLE_INTERVAL    1000000
#define RT_THROUGHPUT_ARCHIVE_INTERVAL   3000000
#define mutiple 4
#define FILE_BUF_LEN 128
#define BUF_LEN 64

#define rtss_printf(fmt,args...) //printf(fmt,##args)
#define rtss_foreach_entry(el, entry)  \
    for(entry = ((struct rt_throughput_list*)(el))->head; entry; entry = entry->next)
        
static struct rt_throughput_mgmt *RT_throughput;

static inline void *tp_kmalloc(int s)
{
    return kmalloc(s, MPF_CLR, -1);
}

static inline void *__attribute__((always_inline))
rt_throughput_list_init()
{
    struct rt_throughput_list *el;

    el = (struct rt_throughput_list *)tp_kmalloc(sizeof(struct rt_throughput_list));
    if(likely(el)){
        el->count = 0;
        el->head = el->tail = 0;
        rt_mutex_init(&el->entries_mtx, NULL);
    }
    return el;
}

static inline void __attribute__((always_inline))
rt_throughput_list_add(struct rt_throughput_list *list, struct rt_throughput_entry *entry)
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

static inline struct rt_throughput_entry *__attribute__((always_inline))
rt_throughput_list_del(struct rt_throughput_list *list, struct rt_throughput_entry *entry)
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

static inline struct rt_throughput_entry *__attribute__((always_inline))
rt_throughput_list_del_and_free(struct rt_throughput_list *list, 
    struct rt_throughput_entry *entry, 
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

static inline struct rt_throughput_entry *__attribute__((always_inline))
rt_throughput_list_trim_head(struct rt_throughput_list *list)
{
    if (list->head)
        return rt_throughput_list_del(list, list->head);

    return NULL;
}

static inline struct rt_throughput_entry *__attribute__((always_inline))
rt_throughput_list_trim_tail(struct rt_throughput_list *list)
{
    if (list->head)
        return rt_throughput_list_del(list, list->tail);

    return NULL;
}

static inline size_t
rt_throughput_entry_format(char *result, int len,
    uint64_t router __attribute__((__unused__)), uint64_t port __attribute__((__unused__)),
    void *xentry)
{
    size_t rsize = 0;
    char date[32] = {0};

    struct rt_throughput_entry *entry = xentry;
    
    if(!result || !entry)
        goto finish;

    rt_tms2str (entry->tm, EVAL_TM_STYLE_EXT, date, 32);

	rsize = snprintf(result, len - 1, "%s\"%s\n", "time:", date);

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
rt_throughput_param()
{
    return RT_throughput;
}

static int __attribute__((__unused__))
rt_throughput_retrieve(struct rt_throughput_list *el, 
    const char * desc __attribute__((__unused__))) 
{
    struct rt_throughput_entry *entry = NULL;
    char entry_str[4096] = {0};

    rtss_printf("\n\n\n============= %s cache (%d entries) ==============\n", desc ? desc : "ANY", el->count);
    rtss_foreach_entry(el, entry) {
        rt_throughput_entry_format(entry_str, 4096, 0, 0, (void *)entry);
        rtss_printf("%s", entry_str);
    }

    return 0;
}

static int
rt_throughput_cache(
    struct rt_throughput_list *rrl,
    void *entry)
{
    rt_mutex_lock(&rrl->entries_mtx);
    rt_throughput_list_add(rrl, entry);
    rt_mutex_unlock(&rrl->entries_mtx);
#ifdef RTSS_DETAILED_DEBUG
    rtss_printf("[A]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, rrl->count);
#endif
    return XSUCCESS;
}

static inline void *
rt_throughput_alloc(struct rt_throughput_entry **rtss)
{
    *rtss = (struct rt_throughput_entry *)tp_kmalloc(sizeof(struct rt_throughput_entry));
    return *rtss;
}

static int
rt_throughput_cleanup(void *xel)
{
    struct rt_throughput_entry *entry, *s, *next;
    struct rt_throughput_list *el = xel;
#ifdef RT_THROUGHPUT_DEBUG
    int count __attribute__((__unused__)) = el->count;
    int rcount = 0;
#endif

    rt_mutex_lock(&el->entries_mtx);
    for (s = el->head; s; s = next)
    {
        next = s->next;
        entry = s;
       #ifdef RTSS_DETAILED_DEBUG
        eval_printf("[D]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, el->count);
       #endif
       #ifdef RT_THROUGHPUT_DEBUG
       rcount ++;
       #endif
        rt_throughput_list_del_and_free(el, entry, free);
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
rt_throughput_mk_file(uint64_t router, uint64_t port,
    const char *path, const char *prefix, char *fname, size_t len)
{

    declare_array(char, file, BUF_LEN);
    declare_array(char, realpath, FILE_BUF_LEN);
    declare_array(char, cur_day, BUF_LEN);
    
    rt_file_mk_by_time(NULL, EVAL_TM_STYLE_YD, cur_day, BUF_LEN);
    snprintf (realpath, FILE_BUF_LEN - 1, "%s/routers/r%lu/p%lu/%s/%s", 
                path, router, port, prefix, cur_day);

    if (!rt_dir_exsit(realpath)){
        declare_array(char, mkdir, FILE_BUF_LEN);
        snprintf (mkdir, FILE_BUF_LEN - 1, "mkdir -p %s", realpath);
        do_system (mkdir);
    }
    
    rt_file_mk_by_time(NULL, EVAL_TM_STYLE_YH, file, BUF_LEN);
    snprintf (fname, len - 1, "%s/%s_%s.dat", realpath, prefix, file);
}

static int rt_throughput_write_file(FILE *fp, 
    uint64_t router, uint64_t port, void *entry, 
    size_t (*format_fn)(char *, int, uint64_t, uint64_t, void *))
{
    int xret = 0;
    declare_variable(char *, p, NULL);
    declare_variable(size_t, size, 0);
    
    if (!format_fn){
        xret = -1;
        goto finish;
    }
    
    rt_kmalloc((void**)&p, mutiple * sizeof (struct rt_throughput_entry));
    size = format_fn (p, mutiple * sizeof (struct rt_throughput_entry),  router, port, entry);
    BUG_ON(rt_file_write(fp, 
        (void *)p, size, NULL) == EOF);
    free(p);

finish:
    return xret;        
}

static int rt_throughput_write_disk(const char *realpath, 
    void (*rt_throughput_mkfile_fn)(uint64_t, uint64_t,
    const char *, const char *, char *, size_t),
    struct rt_throughput_list *el)
{
    static const  int offset = 60 * 60;    
    int flag = 0;
    int wl = 0;
    const char *prefix = "RT_throughput";
    FILE *fp = NULL;
    declare_array(char, file, FILE_BUF_LEN);
    struct rt_throughput_entry *entry, *s, *next;
#ifdef RT_THROUGHPUT_DEBUG
    int count __attribute__((__unused__)) = el->count;
    int rcount = 0;
#endif
    
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

        if((flag == 0) || (flag == -1)){ 
            if(flag == -1){
                fclose(fp);
            }
            rt_throughput_mkfile_fn(el->router, el->port, realpath, prefix, file, FILE_BUF_LEN);
            rt_file_open(file, &fp);
            if(!fp) goto finish;
        }
        flag = time(NULL)%offset - 1;
        
        if (!rt_throughput_write_file(fp, 
            el->router, el->port, (void *)entry, 
            rt_entry_to_string))
            wl ++;
 
        rt_throughput_list_del_and_free(el, entry, free);
    }

#ifdef RT_THROUGHPUT_DEBUG
    rtss_printf("[W]    file=\"%s\", %d entries, remain=%d\n",
        file, rcount, count-rcount);
#endif

    fclose(fp);
finish:
    return wl;
}


/** Rewrite cache evaluation entry to files */
static int
rt_throughput_flush(struct rt_throughput_list *el, const char *path)
{
    if (el->count <= 0)
        goto finish;
    
    rt_mutex_lock(&el->entries_mtx);
    rt_throughput_write_disk(path, rt_throughput_mk_file, el);
    rt_mutex_unlock(&el->entries_mtx);

finish:    
    return 0;
}

static inline void
rt_throughput_load_selective(
    FILE *fp,
    char *from, char *to, uint64_t interval,
    json_object *rt_array_obj)
{
#define LINE_SIZE   (mutiple* sizeof (struct rt_throughput_entry))

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

        json_object_array_add(rt_array_obj, json_tokener_parse(line));
        clear_memory(timestamp, BUF_LEN);
        clear_memory(line, LINE_SIZE);
    }

finish:
    return;
}

static inline void
rt_throughput_load_sightless(
    FILE *fp, uint64_t interval,
    json_object *rt_array_obj)
{
#define LINE_SIZE   (mutiple* sizeof (struct rt_throughput_entry))

    declare_array(char, line, LINE_SIZE);
    rt_kmalloc((void**)&line, LINE_SIZE);
    
    const int times = (interval * 1000000)/RT_THROUGHPUT_SAMPLE_INTERVAL;
    int xtimes __attribute__((__unused__)) = 0;

    xtimes = times;
    while(fgets(line, LINE_SIZE - 1, fp))
    {
        json_object_array_add(rt_array_obj, json_tokener_parse(line));
        clear_memory(line, LINE_SIZE);
    }

    return;
}

static inline void
rt_throughput_load_by_second(char *realpath, uint64_t interval,
    uint64_t start_ts , uint64_t end_ts ,
    json_object *rt_array_obj)
{
    const char *prefix = "RT_throughput";
    FILE *fp = NULL;
    declare_array(char, key_sf, BUF_LEN);
    declare_array(char, key_ef, BUF_LEN);
    declare_array(char, file_key, BUF_LEN);
    declare_array(char, file, FILE_BUF_LEN);    
    declare_array(char, path, BUF_LEN);

    rt_tms2str(start_ts, EVAL_TM_STYLE_FULL, key_sf, BUF_LEN);
    rt_tms2str(end_ts, EVAL_TM_STYLE_FULL, key_ef, BUF_LEN);
    
    rt_tms2str(start_ts, EVAL_TM_STYLE_YD, path, BUF_LEN);
    rt_tms2str(start_ts, EVAL_TM_STYLE_YH, file_key, BUF_LEN);
    snprintf(file, FILE_BUF_LEN-1, "%s/%s/%s_%s.dat", realpath, path, prefix, file_key);
    if(rt_file_exsit(file)){
        fp = fopen(file, "r");
        assert(fp != NULL);        
        rt_throughput_load_selective(fp, key_sf, key_ef, interval, rt_array_obj);              
        fclose(fp);
    }
    
}

static inline void
rt_throughput_load_by_hour(char *realpath, uint64_t interval,
    uint64_t start_ts , uint64_t end_ts ,
    json_object *rt_array_obj)
{
    const char *prefix = "RT_throughput";
    FILE *fp = NULL;
    declare_array(char, file_key, BUF_LEN);
    declare_array(char, file, FILE_BUF_LEN);    
    declare_array(char, path, BUF_LEN);
    uint64_t tm =0;    
    uint64_t st_tail = 0;
    uint64_t et_head = 0;
    static const  uint64_t offset = 60 * 60;

    st_tail = start_ts - start_ts%offset + offset;
    et_head = end_ts - end_ts%offset;

    if(st_tail != start_ts){
        rt_throughput_load_by_second(realpath, interval,
                start_ts, st_tail, rt_array_obj); 
    }

    tm = st_tail;
    while(tm < et_head){
        rt_tms2str(tm, EVAL_TM_STYLE_YD, path, BUF_LEN);
        rt_tms2str(tm, EVAL_TM_STYLE_YH, file_key, BUF_LEN);
        snprintf(file, FILE_BUF_LEN-1, "%s/%s/%s_%s.dat", realpath, path, prefix, file_key);
        if(rt_file_exsit(file)){
            fp = fopen(file, "r");
            assert(fp != NULL);            
            rt_throughput_load_sightless(fp, interval, rt_array_obj);              
            fclose(fp);
        }
        tm += offset;
    }    

    if(et_head != end_ts){
        rt_throughput_load_by_second(realpath, interval,
                et_head, end_ts, rt_array_obj); 
    }
    
}

static int
rt_throughput_load(uint64_t router ,
    uint64_t port , uint64_t start_ts , uint64_t end_ts , 
    uint64_t interval , json_object *rt_array_obj)
{
    int xret = XSUCCESS;
    declare_array(char, sf, BUF_LEN);
    declare_array(char, ef, BUF_LEN);
    declare_array(char, realpath, FILE_BUF_LEN);
    const char *prefix = "RT_throughput";

    snprintf(realpath, FILE_BUF_LEN - 1, EVAL_FILE_PATH"/routers/r%lu/p%lu/%s", 
        router, port, prefix);

    rt_tms2str(start_ts, EVAL_TM_STYLE_YH, sf, BUF_LEN);
    rt_tms2str(end_ts, EVAL_TM_STYLE_YH, ef, BUF_LEN);        
    
    if(!strcmp(sf, ef)){
        rt_throughput_load_by_second(realpath, interval,
            start_ts , end_ts , rt_array_obj);
    }else {
        rt_throughput_load_by_hour(realpath, interval,
            start_ts , end_ts , rt_array_obj);  
    }
    
    goto finish; 
    
finish:
    return xret;
}

int rt_throughput_query(uint64_t router ,
    uint64_t port , int cmd_type, uint64_t start_ts ,
    uint64_t end_ts , uint64_t interval , json_object *rt_array_obj)
{
    cmd_type = cmd_type;
    int xret = XSUCCESS;
    declare_variable(struct rt_throughput_list *, el, RT_throughput->cache);
    declare_variable(char *, path, RT_throughput->default_path);
    declare_variable(struct rt_throughput_entry *, entry, NULL);

    if ((start_ts == end_ts) &&
            (start_ts == 0)){
        rt_mutex_lock(&el->entries_mtx);
        entry = el->tail;
        json_object_array_add(rt_array_obj, 
            rt_entry_to_object(entry, router, port));
        rt_mutex_unlock(&el->entries_mtx);
    }else if ((start_ts < end_ts) ||
            (start_ts = end_ts)){
        rt_throughput_flush(el, path);
        xret = rt_throughput_load(router, port, start_ts, end_ts, 
                interval, rt_array_obj);
    }else{
        xret = -(ERRNO_THROUGHPUT_INVALID);
    }

    return xret;
}

static void THIS_API_IS_ONLY_FOR_TEST
rt_throughput_generator(struct rt_throughput_entry **rt_entry)
{
    rt_throughput_alloc(rt_entry);

    (*rt_entry)->tm = time(NULL);
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
    
}

#define rt_throughput_foreach_router
static void *
rt_throughput_do_cache(void *param)
{
    declare_variable(struct rt_throughput_mgmt *, xrtss_mgmt, (struct rt_throughput_mgmt *)param);
    declare_variable(struct rt_throughput_list *, rt_list, NULL);
    declare_variable(struct rt_throughput_entry *, rt_entry, NULL);
    
    FOREVER {
        rt_list = xrtss_mgmt->cache;
        rt_throughput_foreach_router {
            rt_throughput_generator(&rt_entry);
            rt_throughput_cache(rt_list, rt_entry);      
            //rt_throughput_retrieve(rt_list, "MAIN RT_throughput");
        }
        usleep(xrtss_mgmt->sample_threshold);
    }
    
    return NULL;
}

static void *
rt_throughput_do_archive(void *param)
{
    declare_variable(struct rt_throughput_mgmt *, xrtss_mgmt, (struct rt_throughput_mgmt *)param);

    FOREVER {
        usleep(xrtss_mgmt->flush_threshold);
        STDIO_RENDERING_STD_ERROR_AND_CONTINUE(
            rt_throughput_flush(xrtss_mgmt->cache, xrtss_mgmt->default_path));
    }
	
    return NULL;
}

static void
rt_throughput_run_init()
{
    RT_throughput = tp_kmalloc(sizeof(struct rt_throughput_mgmt));
    if(likely(RT_throughput)){
        RT_throughput->cache = rt_throughput_list_init();
        RT_throughput->flush_threshold  = RT_THROUGHPUT_ARCHIVE_INTERVAL;
        RT_throughput->sample_threshold = RT_THROUGHPUT_SAMPLE_INTERVAL;
        snprintf (RT_throughput->default_path, PATH_SIZE - 1, "%s", EVAL_FILE_PATH);
    }
}

static struct eval_template 
rt_throughput_template = {
    {
        .desc = "RealTime Throughput",
        .type = TEMP_RTSS,
        .init = rt_throughput_run_init,
        .param = rt_throughput_param,
    },
    .cache = NULL,
    .retrieve = NULL,
    .cleanup = rt_throughput_cleanup,
    .cache_ctx = rt_throughput_do_cache,
    .archive_ctx = rt_throughput_do_archive
};

void
rt_throughput_init()
{
    template_install(TEMP_RTSS, &rt_throughput_template);
    return;
}

