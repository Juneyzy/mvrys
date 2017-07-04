#include "sysdefs.h"
#include "rt_clue.h"


static struct rt_clue_list *clue_master[CLUE_UNUSED] = {NULL};

#define rtss_printf(fmt,args...) printf(fmt,##args)
#define rtss_foreach_entry(el, entry)  \
    for(entry = ((struct rt_clue_list*)(el))->head; entry; entry = entry->next)
        
static inline void *__attribute__((always_inline))
rt_clue_list_init()
{
#if 0
    int type = 0;

    struct rt_clue_list *list = NULL;
    struct rt_clue_entry *clue = NULL;
    for (type = 0; type < CLUE_UNUSED; type ++){
        list = &clue_master[type];
        rtss_foreach_entry(list, clue){
            clue->type = type;
            rtss_printf ("clue->type=%d\n", clue->type);
        }
    }

    return NULL;
#else

    struct rt_clue_list *el = (struct rt_clue_list *)malloc(sizeof(struct rt_clue_list));
    memset(el, 0, sizeof(struct rt_clue_list));
    rt_mutex_init(&el->entries_mtx, NULL);
    el->count = 0;
    el->head = el->tail = 0;
    el->has_been_initialized = 1;
    
    return el;

#endif
}

static inline void __attribute__((always_inline))
rt_clue_list_add(struct rt_clue_list *list, struct rt_clue_entry *entry)
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

static inline struct rt_clue_entry *__attribute__((always_inline))
rt_clue_list_del(struct rt_clue_list *list, struct rt_clue_entry *entry)
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

static inline struct rt_clue_entry *__attribute__((always_inline))
rt_clue_list_del_and_free(struct rt_clue_list *list, 
    struct rt_clue_entry *entry, 
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

static inline struct rt_clue_entry *__attribute__((always_inline))
rt_clue_list_trim_head(struct rt_clue_list *list)
{
    if (list->head)
        return rt_clue_list_del(list, list->head);

    return NULL;
}

static inline struct rt_clue_entry *__attribute__((always_inline))
rt_clue_list_trim_tail(struct rt_clue_list *list)
{
    if (list->head)
        return rt_clue_list_del(list, list->tail);

    return NULL;
}

static inline size_t
rt_clue_entry_format(char *result, int len,
    uint64_t router __attribute__((__unused__)), uint64_t port __attribute__((__unused__)),
    void *xentry)
{
    size_t rsize = 0;
    char date[32] = {0};

    struct rt_clue_entry *entry = xentry;
    
    if(!result || !entry)
        goto finish;

    //rt_tms2str (entry->tm, EVAL_TM_STYLE_EXT, date, 32);

	rsize = snprintf(result, len - 1, "%s\"%s\n", "time:", date);

#if 0   
    rsize = snprintf(result, len - 1, "%s\"%s\" in:{%s%lu %s%f %s%lu %s%f} out:{%s%lu %s%f %s%lu %s%f}\n",
                     "time:", date,
                     "bs:", entry->clue.bs.in.iv,
                     "bps:", entry->clue.bps.in.fv,
                     "ps:", entry->clue.ps.in.iv,
                     "pps:", entry->clue.pps.in.fv,
                     "bs:", entry->clue.bs.out.iv,
                     "bps:", entry->clue.bps.out.fv,
                     "ps:", entry->clue.ps.out.iv,
                     "pps:", entry->clue.pps.out.fv);
#endif

finish:
    return rsize;
}
static int
rt_clue_retrieve(struct rt_clue_list *el, 
    const char * __attribute__((__unused__))desc ) 
{
    struct rt_clue_entry *entry = NULL;
    //char entry_str[4096] = {0};

    rtss_printf("\n\n\n============= %s cache (%d entries) ==============\n", desc ? desc : "ANY", el->count);
    rtss_foreach_entry(el, entry) {
        //rt_clue_entry_format(entry_str, 4096, 0, 0, (void *)entry);
        //rtss_printf("%s", entry_str);
    }

    return 0;
}

static int
rt_clue_cache(
    struct rt_clue_list *rrl,
    void *entry)
{
    rt_mutex_lock(&rrl->entries_mtx);
    rt_clue_list_add(rrl, entry);
    rt_mutex_unlock(&rrl->entries_mtx);
#ifdef RTSS_DETAILED_DEBUG
    rtss_printf("[A]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, rrl->count);
#endif
    return XSUCCESS;
}

static int __attribute__((__unused__))
rt_clue_decache(
    struct rt_clue_list *rrl,
    void *entry)
{
    rt_mutex_lock(&rrl->entries_mtx);
    rt_clue_list_del(rrl, entry);
    rt_mutex_unlock(&rrl->entries_mtx);

    struct rt_clue_mail *rc = (struct rt_clue_mail *)((struct rt_clue_entry *)entry)->val;
    if (rc){
        if (rc->keyword) free(rc->keyword);
        if (rc->mail) free(rc->mail);
        free(rc);
    }
    
#ifdef RTSS_DETAILED_DEBUG
    rtss_printf("[A]    entry=%p, entry->next=%p, entry->prev=%p, remain=%d\n",
            entry, entry->next, entry->prev, rrl->count);
#endif
    return XSUCCESS;
}

static inline void __attribute__((__unused__))
rt_clue_alloc(struct rt_clue_entry **rtss)
{
    *rtss = (struct rt_clue_entry *)malloc(sizeof(struct rt_clue_entry));
    assert(*rtss);
    memset(*rtss, 0, sizeof(struct rt_clue_entry));
}

static int __attribute__((__unused__))
rt_clue_cleanup(void *xel)
{
    struct rt_clue_entry *entry, *s, *next;
    struct rt_clue_list *el = xel;
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
        rt_clue_list_del_and_free(el, entry, free);
    }
    rt_mutex_unlock(&el->entries_mtx);

#ifdef RT_THROUGHPUT_DEBUG
    rtss_printf("[D]    %d entries, remain=%d\n",
        rcount, count-rcount);
#endif
    entry = s = next = NULL;

    return 0;
}

struct rt_clue_list *
rt_clue_get(int clue_type)
{
    return clue_master[clue_type%CLUE_UNUSED];
}

void rt_clue_set(int clue_type, void *el)
{
    clue_master[clue_type%CLUE_UNUSED] = el;
}

struct clue_type_map{
    int id;
    char *key;
    char *desc;
}CTM[CLUE_UNUSED] = {
    {CLUE_EMAIL,     "email",   "this is an email clue"},
    {CLUE_IPPORT,   "ipport",   "this is an ipport clue"}, 
    {CLUE_HTTP,      "http",       "this is a http clue"}
};

int rt_clue_str2type(const char *clue_type, ssize_t size)
{
    int type = CLUE_UNUSED;
    
    if(!clue_type || !size)
        goto finish;
    
   for (type = 0; type < CLUE_UNUSED; type ++){
        if (!STRNCMP(CTM[type].key, clue_type, size)){
            goto finish;
        }
   }
   
finish:
    return type;
}

const char *rt_clue_type2str(int clue_type)
{
    return CTM[clue_type%CLUE_UNUSED].key;
}

struct clue_action_map{
    int action;
    char *key;
    char *desc;
}CMM[MD_UNUSED] = {
    {MD_ADD,      "add",  "add a clue, but this clue is not enabled\n"},
    {MD_DEL,       "del",   "delete a clue, but this clue is not enabled\n"}, 
    {MD_CTRL,     "ctrl",   "enable a clue\n"},
    {MD_NCTRL,   "nctrl", "disable a clue\n"}
};
int rt_clue_str2action(const char *clue_action, ssize_t size)
{
    int action = MD_UNUSED;
    
    if(!clue_action || !size)
        goto finish;
    
   for (action = 0; action < MD_UNUSED; action ++){
        if (!STRNCMP(CMM[action].key, clue_action, size)){
            goto finish;
        }
   }
   
finish:
    return action;
}

const char *rt_clue_action2str(int clue_action)
{
    return CMM[clue_action%MD_UNUSED].key;
}

int
rt_clue_mail_add(uint64_t clue_id, 
    const char *subtype, 
    const char __attribute__((__unused__))*protocol,
    const char *mail,
    const char *keyword,
    const char *match_method,
    uint64_t lifttime,
    const char __attribute__((__unused__))*kwpl)
{
    int xret = -1;
    struct rt_clue_entry *rc = NULL;
    struct rt_clue_mail *rc_mail = NULL;

    rc_mail = (struct rt_clue_mail *)malloc(sizeof(struct rt_clue_mail));
    if(!rc_mail) {
        xret = ERRNO_MEM_ALLOC;
        goto finish;
    }

    rc_mail->id = clue_id;
    rc_mail->proto = SMTP;
    rc_mail->lifttime = lifttime;
    rc_mail->mmtype = (!STRNCMP(match_method, "exact", STRLEN("exact")) ? MM_EXACT : MM_FUZZY);
    rc_mail->subtype = (!STRNCMP(subtype, "keyword", STRLEN("keyword")) ? SUBTYPE_KEYWORD : SUBTYPE_MAIL);
    if (rc_mail->subtype == SUBTYPE_KEYWORD
        && !keyword)
        rc_mail->keyword = strdup(keyword);
    else
        if (!mail)
        rc_mail->mail = strdup(mail);
        
    rt_clue_alloc(&rc);
    if(!rc) {
        free(rc_mail);
        xret = ERRNO_MEM_ALLOC;
        goto finish;
    }

    rc->val = rc_mail;
    rc->action = MD_ADD;
    struct rt_clue_list *el = NULL;
    el = rt_clue_get(CLUE_EMAIL);
    while(!el){
        el = rt_clue_list_init();
        rt_clue_set(CLUE_EMAIL, el);
        BUG_ON(!el);
    }
    xret = rt_clue_cache(el, rc);
    rt_clue_retrieve(el, "email");

    //xret = rt_clue_decache(clue_master[CLUE_EMAIL], rc);
    //rt_clue_retrieve(clue_master[CLUE_EMAIL], "email");
    
finish:
    return xret;
}



int
rt_clue_proc()
{
     return 0;
}


