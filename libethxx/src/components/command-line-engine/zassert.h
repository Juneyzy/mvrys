#ifndef _QUAGGA_ASSERT_H
#define _QUAGGA_ASSERT_H

extern void _zlog_assert_failed(const char *assertion, const char *file,
                                unsigned int line, const char *function)
__attribute__((noreturn));

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#ifndef __ASSERT_FUNCTION
#define __ASSERT_FUNCTION    __func__
#endif
#elif defined(__GNUC__)
#ifndef __ASSERT_FUNCTION
#define __ASSERT_FUNCTION    __FUNCTION__
#endif
#else
#define __ASSERT_FUNCTION    NULL
#endif

#define zassert(EX) ((void)((EX) ?  0 : \
                            (fprintf(stdout,"Assertion `%s' failed in file %s, line %u, function %s", \
                                     #EX, __FILE__, __LINE__, __ASSERT_FUNCTION ), \
                             0)))

#undef assert
#define assert(EX) zassert(EX)

#endif
