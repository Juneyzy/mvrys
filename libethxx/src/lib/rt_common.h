/*
 *   rt_common.h
 *   Created by Tsihang <qihang@semptian.com>
 *   1 June, 2015
 *   Func: System timer control interface of SPASR
 *   Personal.Q
 */

#ifndef __RT_COMMON_H__
#define __RT_COMMON_H__

#ifndef VALID
#define VALID 1
#define INVALID 0
#endif

#ifndef uninitialized
#define uninitialized 0
#define initialized 1
#endif

#ifndef ENABLE
#define ENABLE  1
#define DISABLE 0
#endif

#ifndef FOREVER
#define FOREVER for(;;)
#endif

#ifndef IN
#define IN
#define OUT
#endif

#ifndef THIS_API_IS_ONLY_FOR_TEST
#define THIS_API_IS_ONLY_FOR_TEST
#endif

#ifndef The_interface_has_not_been_tested
#define The_interface_has_not_been_tested
#define THE_INTERFACE_HAS_NOT_BEEN_TESTED
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define declare_array(type, name, size)  type name[size] = {0}
#define declare_variable(type, name, val)  type name = val

#include <assert.h>
#define BUG_ON(chk) assert (!(chk));


#ifdef __linux__
#ifndef taskDelay
#include <unistd.h>
#define taskDelay(a)                usleep(a * 100)
#endif
#endif

#define EVAL_TM_STYLE_YH   "%Y%m%d%H"
#define EVAL_TM_STYLE_YD   "%Y%m%d"

#define EVAL_TM_STYLE   "%Y-%m-%d"
#define EVAL_TM_STYLE_EXT   "%H:%M:%S"
#define EVAL_TM_STYLE_FULL "%Y-%m-%d %H:%M:%S"
#define EVAL_FILE_PATH  "/home/tsihang/eval/db4spasr"


/** Alway treated the expr as true */
#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif

/** Alway treated the expr as false */
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

#define THIS 0

enum {
    ALLOWED, 
    FORBIDDEN
}; 

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type,member));})
#endif

#define RT_ETHXX_CAPTURE_ADVANCED
#define RT_TMR_ADVANCED

#define	__rt_always_inline__	__attribute__((always_inline)) inline


#define	FILTER_NORMAL		0
#define	FILTER_VRS_LOCAL	1
#define	FILTER_VRS_YJ		2

#endif
