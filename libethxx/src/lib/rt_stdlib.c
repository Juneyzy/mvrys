
#include <stdlib.h>
#include <string.h>
#include "rt_common.h"
#include "rt_atomic.h"
#include "rt_stdlib.h"

static atomic64_t times;

void *kmalloc(int s, 
                        int flags, 
                        int __attribute__((__unused__)) node)
{
    void *p;
    
    if(likely((p = malloc(s)) != NULL)){
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic64_inc(&times);
    }
    
    return p;
}

void *kcalloc(int c, int s, 
                        int flags, 
                        int node)
{ 
    return kmalloc(c * s, flags, node);
}

void *krealloc(void *sp,  int s, 
                        int flags, 
                        int __attribute__((__unused__)) node)
{
    void *p;
    
    if(likely((p = realloc(sp, s)) != NULL)){
        if(flags & MPF_CLR)
            memset(p, 0, s);
        atomic64_inc(&times);
    }
    
    return p;
}

void kfree(void *p)
{
    if(likely(p != NULL)){
        free(p);
        atomic64_dec(&times);
    }
    
    p = NULL;
}

void
rt_kmalloc(void **p, size_t s)
{
    if(likely((*p = kmalloc(s, MPF_CLR, -1)) != NULL))
        memset (*p, 0, s);
}

