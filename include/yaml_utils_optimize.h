#ifndef __UTILS_OPTIMIZE_H__
#define __UTILS_OPTIMIZE_H__

/** Not defined */
#if CPPCHECK==1
#define likely
#define unlikely
#else
#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif
#endif

/** from http://en.wikipedia.org/wiki/Memory_ordering
 *
 *  C Compiler memory barrier
 */
#define cc_barrier() __asm__ __volatile__("": : :"memory")

/** from http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
 *
 * Hardware memory barrier
 */
#define hw_barrier() __sync_synchronize()


#endif
