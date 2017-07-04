#ifndef __UTIL_ENDIAN_H__
#define __UTIL_ENDIAN_H__

#include <arpa/inet.h>

#define IS_BIG_ENDIAN    (__BYTE_ORDER == __BIG_ENDIAN)

typedef union 
{
    uint64_t data64;
#if IS_BIG_ENDIAN
    struct{
        uint32_t h32;
        uint32_t l32;
    };
#else
    struct{
        uint32_t l32;
        uint32_t h32;
    };
#endif
}data64_un;

#if IS_BIG_ENDIAN
#define HTONS
#define NTOHS
#define HTONL
#define NTOHL
#else
#define HTONS(a)   htons(a)
#define NTOHS(a)   ntohs(a)
#define HTONL(a)   htonl(a)
#define NTOHL(a)   ntohl(a) 
#endif

#if IS_BIG_ENDIAN

#define HTONLL(data64)  data64
#define NTOHLL(data64)  data64

#else

static __rt_always_inline__ uint64_t HTONLL(uint64_t data64)
{
    data64_un data;
    data.h32 = HTONL(((data64_un*)&data64)->l32);
    data.l32 = HTONL(((data64_un*)&data64)->h32);
    return data.data64;
}
static __rt_always_inline__ uint64_t  NTOHLL(uint64_t data64)
{
    data64_un data;
    data.h32 = NTOHL(((data64_un*)&data64)->l32);
    data.l32 = NTOHL(((data64_un*)&data64)->h32);
    return data.data64;
}

#endif


#endif
