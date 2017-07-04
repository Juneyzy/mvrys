#ifndef __UTIL_STRING_H__
#define __UTIL_STRING_H__

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#ifndef STRLEN
#define STRLEN(STR) ((int)strlen((const char *)STR))
#endif

#ifndef STRCMP
#define STRCMP(S,D) (strcmp((const char *)S, (const char *)D))
#endif

#ifndef STRNCMP
#define STRNCMP(S,D,L) (strncmp((const char *)S, (const char *)D, L))
#endif

#ifndef SPRINTF
#define SPRINTF(D,FMT,ARGS...) (sprintf((char *)D,(const char *)FMT,##ARGS))
#endif

#ifndef SNPRINTF
#define SNPRINTF(D,C,FMT,ARGS...) (snprintf((char *)D,C,(const char *)FMT,##ARGS))
#endif

#ifndef strcase_equal
#define strcase_equal(STRSRC, STRDST) (strcasecmp(STRSRC, STRDST) == 0)
#endif

#ifndef strnocase_equal
#define strnocase_equal(STRSRC, STRDST) (strnocasecmp(STRSRC, STRDST) == 0)
#endif

#ifndef STRCAT
#define STRCAT(D,S) (strcat((char *)D, (const char *)S))
#endif

#ifndef STRNCAT
#define STRNCAT(D,S,C) (strncat((char *)D, (const char *)S, C))
#endif

#ifndef STRCPY
#define STRCPY(D,S) (strcpy((char *)D, (const char *)S))
#endif

#ifndef STRNCPY
#define STRNCPY(D,S,C) (strncpy((char *)D, (const char *)S, C))
#endif

#ifndef STRSTR
#define STRSTR(D,S) (strstr((const char *)D,(const char *)S))
#endif

#define clear_memory(addr, size) memset(addr, 0, size)

extern void *memcpy64(void *dest, const void *src, size_t count);
extern void *memset64(void *s, int c, size_t count);

#endif
