#include "sysdefs.h"
#include "session.h"

#if 0
struct stream_t{
#define MAX_HSIZE   1024

    struct {
        struct hlist_head list[MAX_HSIZE];
    }_hash;

    struct list_head    *head;

    uint32_t    (*hash_routine)(void *tuple);
    
};

struct stream_t  *stream;

void ethxx_stream_add()
{

}
#endif

