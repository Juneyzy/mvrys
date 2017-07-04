#include <stdint.h>
#include <string.h>

void *memcpy64(void *dest, const void *src, size_t count)
{
    uint64_t    *ls, *ld;
    int tms, remain;
    char *tmp = dest;
    const char *s = src;
    int dword_size = sizeof(uint64_t);
    
    tms = count / dword_size;
    remain = count % dword_size;

    ld = (uint64_t *)dest;
    ls = (uint64_t *)src;
    
    while(tms --){
        *ld++ = *ls++;
        tmp += dword_size;
        s += dword_size;
    };

    while (remain--){
        *tmp++ = *s++;
    }
    
    return dest;
}

#include <stdio.h>
void *memset64(void *s, int c, size_t count)
{
    uint64_t    *ld;
    int tms;
    int dword_size = sizeof(uint64_t);
    uint64_t    v = c;
    
    tms = count / dword_size;
    ld = (uint64_t *)s;
    
    while(tms --){
        *ld++ = v;
    };

    return s;
}

