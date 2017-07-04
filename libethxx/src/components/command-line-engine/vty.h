/* Virtual terminal [aka TeletYpe] interface routine
   Copyright (C) 1997 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _ZEBRA_VTY_H
#define _ZEBRA_VTY_H

#include "thread.h"

#define VTY_BUFSIZ 512*2
#define VTY_MAXHIST 20

#define LAUGUAGE_ENGLISH    0
#define LAUGUAGE_CHINESE   1

/* VTY struct. */
struct vty
{
    /* File descripter of this vty. */
    int fd;
    
    /* Is this vty connect to file or not */
    enum {VTY_TERM, VTY_FILE, VTY_SHELL, VTY_SHELL_SERV} type;
    
    /** Add by tsihang */
    int socket_close;
    
    /* Node status of this vty */
    int node;
    
    /* What address is this vty comming from. */
    char *address;
    
    /* Failure count */
    int fail;
    
    /* Output buffer. */
    struct buffer *obuf;
    
    /* Command input buffer */
    char *buf;
    
    /* Command cursor point */
    int cp;
    
    /* Command length */
    int length;
    
    /* Command max length. */
    int max;
    
    /* Histry of command */
    char *hist[VTY_MAXHIST];
    
    /* History lookup current point */
    int hp;
    
    /* History insert end point */
    int hindex;
    
    /* For current referencing point of interface, route-map,
       access-list etc... */
    void *index;
    
    /* For multiple level index treatment such as key chain and key. */
    void *index_sub;
    
    /* For escape character. */
    unsigned char escape;
    
    /* Current vty status. */
    enum {VTY_NORMAL, VTY_CLOSE, VTY_MORE, VTY_MORELINE} status;
    
    /* IAC handling */
    unsigned char iac;
    
    /* IAC SB handling */
    unsigned char iac_sb_in_progress;
    struct buffer *sb_buffer;
    
    /* Window width/height. */
    int width;
    int height;
    
    /* Configure lines. */
    int lines;
    
    /* Terminal monitor. */
    int monitor;
    
    /* In configure mode. */
    int config;
    
    /* Read and write thread. */
    struct thread *t_read;
    struct thread *t_write;
    
    /* Timeout seconds and thread. */
    unsigned long v_timeout;
    struct thread *t_timeout;
    
    /* Alive */
    char alive;
    
    /* restore flag */
    int restore_flag;
    
    /* 0: English; 1: Chinese */
    int language;

    int reboot_wisper;
};

/* Integrated configuration file. */
#define INTEGRATE_DEFAULT_CONFIG "Quagga.conf"

/* Small macro to determine newline is newline only or linefeed needed. */
#define VTY_NEWLINE  ((vty == NULL || vty->type == VTY_TERM) ? "\r\n" : "\n")

/* Default time out value */
#define VTY_TIMEOUT_DEFAULT 0 /* never expired */

/* Vty read buffer size. */
#define VTY_READ_BUFSIZ 512

/* Default vty configure lines */
#define VTY_CONFIGURE_LINES_DEFAULT     -1

/* Max count of ip address range is not more than it */
#define VTY_TELNET_PERMIT_IP_RANGE_MAX_COUNT    4
extern char telnet_permit_ip_range [][20];

/* Directory separator. */
#ifndef DIRECTORY_SEP
#define DIRECTORY_SEP '/'
#endif /* DIRECTORY_SEP */

#ifndef IS_DIRECTORY_SEP
#define IS_DIRECTORY_SEP(c) ((c) == DIRECTORY_SEP)
#endif

/* GCC have printf type attribute check.  */
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif /* __GNUC__ */

/* Exported variables */
extern char integrate_default[];
extern char kill_all_vty;
/* Prototypes. */
void vty_init(struct thread_master *);
void vty_kill_all();
void vty_init_vtysh(void);
void vty_reset(void);
void vty_finish(void);
struct vty *vty_new(void);
int vty_out_internal(struct vty *, const char *, va_list args);
int vty_out(struct vty *, const char *, ...) PRINTF_ATTRIBUTE(2, 3);
int vty_out_line(struct vty *vty, const char *format, ...);
void vty_read_config(char *, char *);
void vty_config_password();
void vty_time_print(struct vty *, int);
void vty_serv_sock(const char *, unsigned short, const char *);
void vty_close(struct vty *);
int vty_config_lock(struct vty *);
int vty_config_unlock(struct vty *);
int vty_shell(struct vty *);
int vty_shell_serv(struct vty *);
void vty_hello(struct vty *);
void vty_write(struct vty *vty, char *buf, size_t nbytes);

void vty_set_multi_tasking_mode(char enable);
void vty_get_multi_tasking_mode(char *enable);

int vty_console_socket(struct thread *thread);
unsigned int vty_lookup_vector_max();
struct vty *vty_lookup_vector_slot(unsigned int i);
int telnet_out_directly(struct vty *vty, char *string);
int vty_out_directly(struct vty *vty, const char *format, ...);

/* Utility macros to convert VTY argument to unsigned long or integer, tsihang. */
#define VTY_GET_LONG(NAME,V,STR) \
    { \
        char *endptr = NULL; \
        (V) = strtoul ((STR), &endptr, 10); \
        if (*endptr != '\0' || (V) == ULONG_MAX) \
        { \
            vty_out (vty, "%% Invalid %s value%s", NAME, VTY_NEWLINE); \
            return CMD_WARNING; \
        } \
    }

#define VTY_GET_INTEGER_RANGE(NAME,V,STR,MIN,MAX) \
    { \
        unsigned long tmpl; \
        VTY_GET_LONG(NAME, tmpl, STR) \
        if ((long)tmpl < (MIN) || (long)tmpl > (MAX)) \
        { \
            vty_out (vty, "%% Invalid %s value, range in <%d-%d>%s", NAME, MIN, MAX, VTY_NEWLINE); \
            return CMD_WARNING; \
        } \
        (V) = tmpl; \
    }

#define VTY_GET_INTEGER(NAME,V,STR) \
    VTY_GET_INTEGER_RANGE(NAME,V,STR,0U,UINT32_MAX)


#define SOCKUNION_SU2STR(su) \
    ((su == NULL) ? NULL:sockunion_su2str (su))


#define VTY_RENDERING_STD_ERROR(rv) \
    {\
        int sv = rv;\
        if (sv)\
        {   \
            if (!vty->restore_flag)\
                vty_out (vty, "%% Error rv=%d %s", sv, VTY_NEWLINE);   \
            return CMD_WARNING; \
        }\
    }

#define VTY_RENDERING_STD_ERROR_AND_CONTINUE(rv) \
    {\
        int sv = rv;\
        if (sv)\
        {   \
            if (!vty->restore_flag)\
                vty_out (vty, "%% Error rv=%d %s", sv, VTY_NEWLINE);   \
            continue; \
        }\
    }

/** Defined by Tsihang. */
#define VTY_PORT_RANGE_CONVERT(PORT_TYPE_STR, PORT_RANGE_STR, PORT_RANGE_SET)\
    int8_t PORT_RANGE_BUFFER[128] = {0};\
    if (Token((uint8_t *)PORT_TYPE_STR, (uint8_t *)PORT_RANGE_STR, PORT_RANGE_BUFFER))\
    { \
        vty_out (vty, "%% Invalid port name.%s", VTY_NEWLINE);\
        return CMD_WARNING; \
    } \
    PORT_RANGE_SET = get_rangeset_by_range ("{0-64}", (const char *)&PORT_RANGE_BUFFER[0]);\
    if (!PORT_RANGE_SET)\
    {\
        vty_out (vty, "%% Convert port range failure.%s", VTY_NEWLINE);\
        return CMD_WARNING;\
    }
#define VTY_PORT_RANGE_FOREACH(PORT_RANGE_SET, PORT)\
    for (struct range *PORT_RANGE_TEMP = PORT_RANGE_SET; PORT_RANGE_TEMP; PORT_RANGE_TEMP = PORT_RANGE_TEMP->next)\
        for (PORT = PORT_RANGE_TEMP->ul_start; (int)PORT <= (int)PORT_RANGE_TEMP->ul_end; PORT++)

#define VTY_PORT_RANGE_DESTROY(PORT_RANGE_SET) del_rangeset (PORT_RANGE_SET)


/** Defined by Tsihang. */
#define VTY_RANGE_CONVERT(EXPLAIN_STR,RANGE_TEMPLATE_STR, RANGE_STR, RANGE_SET)\
    RANGE_SET = get_rangeset_by_range (RANGE_TEMPLATE_STR, RANGE_STR);\
    if (!RANGE_SET)\
    {\
        vty_out (vty, "%% Invalid %s value, range in %s.%s", EXPLAIN_STR, RANGE_TEMPLATE_STR, VTY_NEWLINE);\
        return -1;\
    }
#define VTY_RANGE_FOREACH(RANGE_SET, ELEMENT)\
    struct range *RANGE_TEMP = NULL;\
    for (RANGE_TEMP = RANGE_SET; RANGE_TEMP; RANGE_TEMP = RANGE_TEMP->next)\
        for (ELEMENT = (typeof(ELEMENT))RANGE_TEMP->ul_start; (typeof(ELEMENT))ELEMENT <= (typeof(ELEMENT))RANGE_TEMP->ul_end; ELEMENT++)

#define VTY_RANGE_DESTROY(RANGE_SET) del_rangeset (RANGE_SET)

#define VTY_OUT_NEWLINE vty_out(vty, "%s", VTY_NEWLINE)

/** Used in restore runtime., added by tsihang. */
#define vty_out_wisper(vty, fmt, ...)    \
    if (!vty->restore_flag)\
        vty_out (vty, ""fmt"", ##__VA_ARGS__)

#define VTY_DUMP_ARGV(vty, argc, argv) /** \
    int __index = 0; \
    for (__index = 0; __index < argc; __index ++)  \
    {   \
        vty_out (vty, "%s ", argv[__index]); \
    }   \
vty_out (vty, "\r\n") */

#endif /* _ZEBRA_VTY_H */
