#ifndef __RT_STDLIB_H__
#define __RT_STDLIB_H__

#include <stdlib.h>

/** memory flags */
#define MPF_NOFLGS  (0 << 0)
#define MPF_CLR         (1 << 0)      /** Clear it after allocated */

#define __TCalloc(cnt, type)        (type *)calloc(cnt, sizeof(type))
#define __TMalloc(cnt, type)        (type *)malloc(cnt * sizeof(type))
#define __TZalloc(cnt, type)        (type *)zalloc(cnt * sizeof(type))

extern void rt_kmalloc(void **ptr, size_t s);

extern void *kcalloc(int c, int s, 
                int __attribute__((__unused__)) flags, 
                int __attribute__((__unused__)) node);

extern void *kcalloc(int c, int s, 
                        int flags, 
                        int node);

extern void *kmalloc(int s, 
                        int flags, 
                        int __attribute__((__unused__)) node);

extern void *krealloc(void *sp,  int s, 
                        int flags, 
                        int __attribute__((__unused__)) node);

extern void kfree(void *p);

#endif

