
#ifndef __SESSOIN_H__
#define __SESSOIN_H__


typedef struct
{   
    uint32_t                    valid;
    void                         *tuple;

    struct hlist_head       *hash_list;
    struct list_head         *list;
    
}scb_t;


#endif  /* __SESSOIN_H__ */

