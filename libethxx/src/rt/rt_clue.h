#ifndef __RT_CLUE_H__
#define __RT_CLUE_H__

#include <stdint.h>
#include "rt_sync.h"


enum MAIL_PROTO {SMTP=0x0001,POP3=0x0010,WEBMAIL=0x0100,IMAP=0x1000};

struct rt_clue_mail{
    int id;
    enum {SUBTYPE_MAIL, SUBTYPE_KEYWORD} subtype;
    enum MAIL_PROTO proto;
    char *mail, *keyword;
    enum {MM_EXACT, MM_FUZZY}mmtype;
    int kwpl;
    int lifttime;
};

struct rt_clue_entry{
    enum {CLUE_EMAIL, CLUE_IPPORT, CLUE_HTTP, CLUE_UNUSED} type;    
    int valid;
    void *val;
    enum {MD_ADD, MD_DEL, MD_CTRL, MD_NCTRL, MD_UNUSED}action;
    struct rt_clue_entry *next, *prev;
};


struct rt_clue_list{
    int has_been_initialized;
    rt_mutex  entries_mtx;
    struct rt_clue_entry *head, *tail;
    int count;
};

int  rt_clue_str2type(const char *, ssize_t);
const char *rt_clue_type2str(int );
int rt_clue_str2action(const char *clue_action, ssize_t size);

int
rt_clue_mail_add(uint64_t clue_id, 
    const char *subtype, 
    const char *protocol,
    const char *mail,
    const char *keyword,
    const char *match_method,
    uint64_t lifttime,
    const char *kwpl);
    

struct rt_clue_list *
rt_clue_find(int clue_type);

#endif
