/*
 * Virtual terminal [aka TeletYpe] interface routine.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "privs.h"
#include "zebra.h"

#include "linklist.h"
#include "thread.h"
#include "buffer.h"
#include "command.h"
#include "sockunion.h"
#include "memory.h"
#include "str.h"
#include "log.h"
#include "prefix.h"
#include "filter.h"
#include "zserv.h"
#include "vty.h"

/* Vty events */
enum event
{
    VTY_SERV,
    VTY_READ,
    VTY_WRITE,
    VTY_TIMEOUT_RESET,
#ifdef VTYSH
    VTYSH_SERV,
    VTYSH_READ,
    VTYSH_WRITE
#endif /* VTYSH */
};

/* Minimum size of output buffers; to be rounded up to multiple of pagesize. */
#define VTY_OBUF_SIZE   4096

#define VTY_CLOSE_ERROR -13

#define VTY_LOOKUP_VECTOR_SIZE  64

static void vty_event(enum event, int, struct vty *);

/* run multi-tasking, one task per CLI client */
static char multi_tasking_mode = 1;

/* new val for multi_tasking_mode */
static char new_multi_tasking_mode = 1;

/* deletion flag - delete all vtys */
char kill_all_vty = 0;

/* Extern host structure from command.c */
extern struct host host;

/* Vector which store each vty structure. */
vector vtyvec;

/* Vector which store each vty structure. used for fast lookup */
static struct vty *vty_lookup_vec[VTY_LOOKUP_VECTOR_SIZE] = {0};

/* Vty timeout value. */
static unsigned long vty_timeout_val = VTY_TIMEOUT_DEFAULT;

/* Vty access-class command */
static char *vty_accesslist_name = NULL;

/* Vty access-calss for IPv6. */
static char *vty_ipv6_accesslist_name = NULL;

/* VTY server thread. */
vector Vvty_serv_thread;

/* Current directory. */
char *vty_cwd = NULL;

/* Configure lock. */
static int vty_config;

/* Login password check. */
static int no_password_check = 0;

/* Integrated configuration file path */
char integrate_default[] = "Quagga.conf";

/* Master of the threads. */
static struct thread_master *master;


/* vty lookup vector utils */
unsigned int vty_lookup_vector_max()
{
    return VTY_LOOKUP_VECTOR_SIZE;
}

struct vty *vty_lookup_vector_slot(unsigned int i)
{
    return vty_lookup_vec[i];
}

void vty_lookup_vector_add(struct vty *vty)
{
    unsigned int i;
    
    for (i = 0; i < VTY_LOOKUP_VECTOR_SIZE; i++)
    {
        if (vty_lookup_vec[i] == NULL)   /* find the next free place */
        {
            vty_lookup_vec[i] = vty;
            return;
        }
    }
}
void vty_lookup_vector_remove(struct vty *vty)
{
    unsigned int i;
    
    for (i = 0; i < VTY_LOOKUP_VECTOR_SIZE; i++)
    {
        if (vty_lookup_vec[i] != NULL && vty_lookup_vec[i]->fd == vty->fd)
            vty_lookup_vec[i] = NULL;
    }
}

char vty_lookup_vector_is_exist(struct vty *vty)
{
    unsigned int i;
    
    for (i = 0; i < VTY_LOOKUP_VECTOR_SIZE; i++)
    {
        if (vty_lookup_vec[i] != NULL && vty_lookup_vec[i]->fd == vty->fd)
            return 1;
    }
    
    return 0;
}

/** Display input to console */
int vty_out_directly(struct vty *vty, const char *format, ...)
{
    vty = vty;
    va_list args;
    int len = 0;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    return len;
}

int telnet_out_directly(struct vty *vty, char *string)
{
    return write(vty->fd, string, strlen(string));
}
/** Added by tsihang for save configuration, 4 Nov, 2014. */
int
vty_file(struct vty *vty)
{
    return vty->type == VTY_FILE ? 1 : 0;
}
/* VTY standard output function. */
int
vty_out_internal(struct vty *vty, const char *format, va_list args)
{
    int len = 0;
    int size = 16000;
    char buf[63488]; /* data_len*3 + data_len/bytes_per_line + 1 = 1600*3 + 1600/20 + 1 + buffer */
    char *p = NULL;
    
    if (vty_shell(vty))
    {
        vprintf(format, args);
    }
    
    else
    {
        /* Try to write to initial buffer.  */
        len = vsnprintf(buf, sizeof(buf), format, args);
        
        /* Initial buffer is not enough.  */
        if (len < 0 || len >= size)
        {
            while (1)
            {
                if (len > -1)
                    size = len + 1;
                    
                else
                    size = size * 2;
                    
                p = XREALLOC(MTYPE_VTY_OUT_BUF, p, size);
                
                if (! p)
                    return -1;
                    
                len = vsnprintf(p, size, format, args);
                
                if (len > -1 && len < size)
                    break;
            }
        }
        
        /* When initial buffer is enough to store all output.  */
        if (! p)
            p = buf;
            
        /* Pointer p must point out buffer. */
        if (vty_shell_serv(vty) || vty_file(vty))     /** Added by tsihang for save configuration, 4 Nov, 2014. */
        {
            if ( -1 == write(vty->fd, (u_char *) p, len))
                perror ("write error");
        }
            
        else
            buffer_write(vty->obuf, (u_char *) p, len);
            
        /* If p is not different with buf, it is allocated buffer.  */
        if (p != buf)
            XFREE(MTYPE_VTY_OUT_BUF, p);
    }
    
    return len;
}

/* VTY standard output function. */
int
vty_out_internal_line(struct vty *vty, const char *format, va_list args)
{
    int len = 0;
    int size = 16000;
    char buf[63488]; /* data_len*3 + data_len/bytes_per_line + 1 = 1600*3 + 1600/20 + 1 + buffer */
    char *p = NULL;
    
    if (vty_shell(vty))
    {
        vprintf(format, args);
    }
    
    else
    {
        /* Try to write to initial buffer.  */
        len = vsnprintf(buf, sizeof(buf), format, args);
        
        /* Initial buffer is not enough.  */
        if (len < 0 || len >= size)
        {
            while (1)
            {
                if (len > -1)
                    size = len + 1;
                    
                else
                    size = size * 2;
                    
                p = XREALLOC(MTYPE_VTY_OUT_BUF, p, size);
                
                if (! p)
                    return -1;
                    
                len = vsnprintf(p, size, format, args);
                
                if (len > -1 && len < size)
                    break;
            }
        }
        
        /* When initial buffer is enough to store all output.  */
        if (! p)
            p = buf;
            
        /* Pointer p must point out buffer. */
        if (-1 == write(vty->fd, (u_char *) p, len))
            perror ("write error");
        
        /* If p is not different with buf, it is allocated buffer.  */
        if (p != buf)
            XFREE(MTYPE_VTY_OUT_BUF, p);
    }
    
    return len;
}
int vty_out(struct vty *vty, const char *format, ...)
{
    va_list args;
    int     len = 0;
    
    if (vty != NULL)
    {
        va_start(args, format);
        len = vty_out_internal(vty, format, args);
        va_end(args);
    }
    
    else
    {
        va_start(args, format);
        va_end(args);
    }
    
    return len;
}

int vty_out_line(struct vty *vty, const char *format, ...)
{
    va_list args;
    int     len = 0;
    
    if (vty != NULL)
    {
        va_start(args, format);
        len = vty_out_internal_line(vty, format, args);
        va_end(args);
    }
    
    else
    {
        va_start(args, format);
        va_end(args);
    }
    
    return len;
}

/**
static int
vty_log_out(struct vty *vty, const char *level, const char *proto_str,
            const char *format, va_list va)
{
    int len;
    char buf[1024];

    if (level)
        snprintf(buf, sizeof buf, "%s: %s: ", level, proto_str);

    else
        snprintf(buf, sizeof buf, "%s: ", proto_str);

    write(vty->fd, buf, strlen(buf));
    len = vsnprintf(buf, sizeof buf, format, va);

    if (len < 0)
        return -1;

    write(vty->fd, (u_char *) buf, len);
    snprintf(buf, sizeof buf, "\r\n");
    write(vty->fd, buf, 2);
    return len;
}
*/
/* Output current time to the vty. */
void
vty_time_print(struct vty *vty, int cr)
{
    time_t clock;
    struct tm *tm;
#define TIME_BUF 25
    char buf [TIME_BUF];
    int ret;
    time(&clock);
    tm = localtime(&clock);
    ret = strftime(buf, TIME_BUF, "%Y/%m/%d %H:%M:%S", tm);
    
    if (ret == 0)
    {
        fprintf(stderr, "Error! strftime error");
        return;
    }
    
    if (cr)
        vty_out(vty, "%s\n", buf);
        
    else
        vty_out(vty, "%s ", buf);
        
    return;
}

/* Say hello to vty interface. */
void
vty_hello(struct vty *vty)
{
    /* Changed by Tsihang. Printing Semptian opening message */
    if (host.motd)
        vty_out(vty, "%s", host.motd);
        
    else
    {
        vty_out(vty, "The Command Line Interface Starting....%s%s", VTY_NEWLINE, VTY_NEWLINE);
        vty_out(vty, "This software is licensed for use according to the terms set by the %s SDK license agreement. %sCopyright %s.%s",
                MANUFACTURE, VTY_NEWLINE, MANUFACTURE, VTY_NEWLINE);
        vty_out(vty, "Running CLI V%d.%02d %s%s",
                CLI_MAJOR_VER, CLI_MINOR_VER, VTY_NEWLINE, VTY_NEWLINE);
    }
}

/* Put out prompt and wait input from user. */
static void
vty_prompt(struct vty *vty)
{
    /* Changed by Tsihang, Semptian. */
    /* hostname is not used       */
    /*
      struct utsname names;
      const char*hostname;
    */
    if (vty->type == VTY_TERM)
    {
        /* hostname = host.name;
        if (!hostname)
        {
          xs_gethostname(&names);
          hostname = names.nodename;
        }
        */
        switch (vty->node)
        {
            case MARVELL_SWITCH_NODE:
            case BCM_SWITCH_NODE:
                vty_out(vty, cmd_prompt(vty->node), "SWITCH");
                break;
                
            default:
                vty_out(vty, cmd_prompt(vty->node), "SPASR");
                break;
        }
    }
    
    /** Add by tsihang */
    else //if ((vty->type == VTY_SHELL)&&vty->socket_close==0)
    {
        vty_out(vty, cmd_prompt(vty->node), "SPASR-CONSOLE");
    }
}

/* Send WILL TELOPT_ECHO to remote server. */
void
vty_will_echo(struct vty *vty)
{
    unsigned char cmd[] = { IAC, WILL, TELOPT_ECHO, '\0' };
    vty_out(vty, "%s", cmd);
}

/* Make suppress Go-Ahead telnet option. */
static void
vty_will_suppress_go_ahead(struct vty *vty)
{
    unsigned char cmd[] = { IAC, WILL, TELOPT_SGA, '\0' };
    vty_out(vty, "%s", cmd);
}

/* Make don't use linemode over telnet. */
static void
vty_dont_linemode(struct vty *vty)
{
    unsigned char cmd[] = { IAC, DONT, TELOPT_LINEMODE, '\0' };
    vty_out(vty, "%s", cmd);
}

/* Use window size. */
static void
vty_do_window_size(struct vty *vty)
{
    unsigned char cmd[] = { IAC, DO, TELOPT_NAWS, '\0' };
    vty_out(vty, "%s", cmd);
}
void
vty_clear_buf(struct vty *vty)
{
    memset(vty->buf, 0, vty->max);
}

#if 0 /* Currently not used. */
/* Make don't use lflow vty interface. */
static void
vty_dont_lflow_ahead(struct vty *vty)
{
    unsigned char cmd[] = { IAC, DONT, TELOPT_LFLOW, '\0' };
    vty_out(vty, "%s", cmd);
}
#endif /* 0 */

int vty_console_socket(struct thread *thread)
{
    struct vty *vty;
    int console_sock = THREAD_FD(thread);
    /* Allocate new vty structure and set up default values. */
    vty = vty_new();
    /*for(i=0; i < VTYSH_INDEX_MAX; i ++)
    {
       vty->vtysh_client[i].fd=-1;
    }*/
    vty->fd = console_sock;
    vty->type = VTY_SHELL;
    vty->address = NULL;
    /*vtysh_connect_all(vty);*/
    
    if (no_password_check)
    {
        if (host.advanced)
            vty->node = ENABLE_NODE;
            
        else
            vty->node = VIEW_NODE;
    }
    
    else
        vty->node = AUTH_NODE;
        
    vty->fail = 0;
    vty->cp = 0;
    vty_clear_buf(vty);
    vty->length = 0;
    memset(vty->hist, 0, sizeof(vty->hist));
    vty->hp = 0;
    vty->hindex = 0;
    /*  vector_set_index (LIB_VTHSH, vtyvec[LIB_VTHSH], vty_sock, vty);
    */
    vty->status = VTY_NORMAL;
    vty->v_timeout = VTY_TIMEOUT_DEFAULT;
    
    if (host.lines >= 0)
        vty->lines = host.lines;
        
    else
        vty->lines = -1;
        
    vty->width = 150/*80*/;
    vty->height = 25;
    vty->iac = 0;
    vty->iac_sb_in_progress = 0;
    vty->sb_buffer = buffer_new(VTY_READ_BUFSIZ);
    vty->socket_close = 0;
    
    if (! no_password_check)
    {
        /* Vty is not available if password isn't set. */
        if (host.password == NULL && host.password_encrypt == NULL)
        {
            vty_out(vty, "Vty password is not set.%s", VTY_NEWLINE);
            vty->status = VTY_CLOSE;
            vty_close(vty);
            return -1;
        }
    }
    
    if (vty->node == AUTH_NODE)
    {
        /*
        if(FirstConfig(first_config_judge))
        {
         vtysh_setup(vty);
         CreatConfigCheckFile(first_config_judge);
        
        }
        */
    }
    
    /* Say hello to the world. */
    vty_hello(vty);
    
    if (! no_password_check)
        vty_out(vty, "%sUser Access Verification%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
        
    vty_prompt(vty);
    /* Add read/write thread. */
    vty_event(VTY_WRITE, console_sock, vty);
    vty_event(VTY_READ, console_sock, vty);
    return 0;
}

int console_fd;

void vty_console_open(void)
{
    int console_sock = 0;
    console_sock = open(ZEBRA_CONSOLE, O_RDWR, 0);
    
    if (!console_sock)
    {
        printf("console sock\n");
        return;
    }
    
    dup2(console_sock, console_fd);
    if (-1 == write(console_sock, "sssssssssssssssssss\n", 6))
        perror ("write error");
#if 0
    /*(void) ioctl (console_sock,  FIOBAUDRATE, CONSOLE_BAUD_RATE);*/
    (void) ioctl(console_sock,  FIOSETOPTIONS, OPT_TERMINAL);
    (void) ioctl(console_sock,  FIOSETOPTIONS, OPT_RAW);
#endif
    thread_add_write(master, vty_console_socket, NULL, console_sock);
}

/* Allocate new vty struct. */
struct vty *
vty_new()
{
    struct vty *new = XCALLOC(MTYPE_VTY, sizeof(struct vty));
    int pgsz = getpagesize();
    
    new->obuf = (struct buffer *) buffer_new((((VTY_OBUF_SIZE - 1) / pgsz) + 1) *pgsz);
    /* Changed by Tsihang, Semptian - increase the first buffer alocation to 1536 bytes
       because there is a bug when sending command bigger than 512 bytes
    */
    new->buf = XCALLOC(MTYPE_VTY, VTY_BUFSIZ * 3);
    new->max = VTY_BUFSIZ * 3;
    new->sb_buffer = NULL;
    
    return new;
}

/* Authentication of vty */
static void
vty_auth(struct vty *vty, char *buf)
{
    char *passwd = NULL;
    char *username = NULL;
    enum node_type next_node = 0;
    int fail;
    char *crypt(const char *, const char *);
    
    switch (vty->node)
    {
#ifdef __USER_AUTH__
    
        case AUTH_USER_NODE:
            username = host.name;
            /** vty_out(vty, "host.name: %s %s", username, "\r\n"); */
            next_node = AUTH_NODE;
            break;
#endif
            
        case AUTH_NODE:
            if (host.encrypt)
                passwd = host.password_encrypt;
                
            else
                passwd = host.password;
                
            //vty_out(vty, "host.password: %s %s", passwd, "\r\n");
            if (host.advanced)
            {
                next_node = host.enable ? VIEW_NODE : ENABLE_NODE;
            }
            
            else
            {
                /** next_node = VIEW_NODE; */   /** Ignore VIEW NODE to ENABLE NODE */
                next_node = ENABLE_NODE;
            }
            
            break;
            
        case AUTH_ENABLE_NODE:
            if (host.encrypt)
                passwd = host.enable_encrypt;
                
            else
                passwd = host.enable;
                
            //vty_out(vty, "host.enable: %s %s", passwd, "\r\n");
            next_node = ENABLE_NODE;
            break;
    }
    
#ifdef __USER_AUTH__
    
    /* Check username */
    if (username)
    {
        fail = strcmp(buf, username);
        
        //vty_out(vty, "vty buf: %s %s", buf, "\r\n");
        if (!fail)
        {
            vty->fail = 0;
            vty->node = next_node;
            return;
        }
        
        else
        {
            goto FAIL;
        }
    }
    
#endif
    
    /* Check password */
    if (passwd)
    {
        fail = strcmp(buf, passwd);
    }
    
    else
        fail = 1;
        
    if (! fail)
    {
        vty->fail = 0;
        vty->node = next_node;    /* Success ! */
    }
    
    else
    {
#ifdef __USER_AUTH__
FAIL:
#endif
        vty->fail++;
        
        if (vty->fail >= 3)
        {
#ifdef __USER_AUTH__
        
            if (vty ->node == AUTH_USER_NODE)
            {
                vty->fail = 0;  /* recount */
                vty_out(vty, "%% Bad username, too many failures!%s", VTY_NEWLINE);
#ifndef __EXTERNAL_LOGIN__
                vty->status = VTY_CLOSE;
#endif
            }
            
            else if (vty->node == AUTH_NODE)
            {
                vty->fail = 0; /* recount */
                vty_out(vty, "%% Bad passwords, too many failures!%s", VTY_NEWLINE);
#ifndef __EXTERNAL_LOGIN__
                vty->status = VTY_CLOSE;
#else
                vty->node = AUTH_USER_NODE;
#endif
            }
            
            else
            {
                /* AUTH_ENABLE_NODE */
                vty->fail = 0;
                vty_out(vty, "%% Bad enable passwords, too many failures!%s", VTY_NEWLINE);
                vty->node = VIEW_NODE;
            }
            
#else
            
            if (vty->node == AUTH_NODE)
            {
                vty_out(vty, "%% Bad passwords, too many failures!%s", VTY_NEWLINE);
                vty->status = VTY_CLOSE;
            }
            
            else
            {
                /* AUTH_ENABLE_NODE */
                vty->fail = 0;    /* recount */
                vty_out(vty, "%% Bad enable passwords, too many failures!%s", VTY_NEWLINE);
                vty->node = VIEW_NODE;
            }
            
#endif
        }
    }
}

/* Command execution over the vty interface. */
int
vty_command(struct vty *vty, char *buf)
{
    int ret;
    vector vline;
    /* Split readline string up into the vector */
    vline = cmd_make_strvec(buf);
    
    if (vline == NULL)
        return CMD_SUCCESS;
        
    ret = cmd_execute_command(vline, vty, NULL, 0);
    
    if (ret != CMD_SUCCESS)
        switch (ret)
        {
            case CMD_WARNING:
                if (vty->type == VTY_FILE)
                    vty_out(vty, "Warning...%s", VTY_NEWLINE);
                    
                break;
                
            case CMD_ERR_AMBIGUOUS:
                vty_out(vty, "%% Ambiguous command.%s", VTY_NEWLINE);
                break;
                
            case CMD_ERR_NO_MATCH:
                vty_out(vty, "%% Unknown command.%s", VTY_NEWLINE);
                break;
                
            case CMD_ERR_INCOMPLETE:
                vty_out(vty, "%% Command incomplete.%s", VTY_NEWLINE);
                break;
                
            case CMD_ERR_EXEED_ARGC_MAX:
                vty_out(vty, "%% Exceed max arguments number (%d) .%s", CMD_ARGC_MAX, VTY_NEWLINE);
                break;
        }
        
    cmd_free_strvec(vline);
    return ret;
}

char telnet_backward_char = 0x08;
char telnet_space_char = ' ';

/* Basic function to write buffer to vty. */
void
vty_write(struct vty *vty, char *buf, size_t nbytes)
{
    if ((vty->node == AUTH_NODE) || (vty->node == AUTH_ENABLE_NODE))
        return;
        
    /* Should we do buffering here ?  And make vty_flush (vty) ? */
    buffer_write(vty->obuf, (u_char *) buf, nbytes);
}

/* Ensure length of input buffer.  Is buffer is short, double it. */
static void
vty_ensure(struct vty *vty, int length)
{
    if (vty->max <= length)
    {
        vty->max *= 2;
        vty->buf = XREALLOC(MTYPE_VTY, vty->buf, vty->max);
    }
}

/* Basic function to insert character into vty. */
static void
vty_self_insert(struct vty *vty, char c)
{
    int i;
    int length;
    vty_ensure(vty, vty->length + 1);
    length = vty->length - vty->cp;
    memmove(&vty->buf[vty->cp + 1], &vty->buf[vty->cp], length);
    vty->buf[vty->cp] = c;
    vty_write(vty, &vty->buf[vty->cp], length + 1);
    
    for (i = 0; i < length; i++)
        vty_write(vty, &telnet_backward_char, 1);
        
    vty->cp++;
    vty->length++;
}

/* Self insert character 'c' in overwrite mode. */
static void
vty_self_insert_overwrite(struct vty *vty, char c)
{
    vty_ensure(vty, vty->length + 1);
    vty->buf[vty->cp++] = c;
    
    if (vty->cp > vty->length)
        vty->length++;
        
    if ((vty->node == AUTH_NODE) || (vty->node == AUTH_ENABLE_NODE))
        return;
        
    vty_write(vty, &c, 1);
}

/* Insert a word into vty interface with overwrite mode. */
static void
vty_insert_word_overwrite(struct vty *vty, char *str)
{
    int len = strlen(str);
    vty_write(vty, str, len);
    strcpy(&vty->buf[vty->cp], str);
    vty->cp += len;
    vty->length = vty->cp;
}

/* Forward character. */
static void
vty_forward_char(struct vty *vty)
{
    if (vty->cp < vty->length)
    {
        vty_write(vty, &vty->buf[vty->cp], 1);
        vty->cp++;
    }
}

/* Backward character. */
static void
vty_backward_char(struct vty *vty)
{
    if (vty->cp > 0)
    {
        vty->cp--;
        vty_write(vty, &telnet_backward_char, 1);
    }
}

/* Move to the beginning of the line. */
static void
vty_beginning_of_line(struct vty *vty)
{
    while (vty->cp)
        vty_backward_char(vty);
}

/* Move to the end of the line. */
static void
vty_end_of_line(struct vty *vty)
{
    while (vty->cp < vty->length)
        vty_forward_char(vty);
}

static void vty_kill_line_from_beginning(struct vty *);
static void vty_redraw_line(struct vty *);

/* Print command line history.  This function is called from
   vty_next_line and vty_previous_line. */
static void
vty_history_print(struct vty *vty)
{
    int length;
    vty_kill_line_from_beginning(vty);
    /* Get previous line from history buffer */
    length = strlen(vty->hist[vty->hp]);
    memcpy(vty->buf, vty->hist[vty->hp], length);
    vty->cp = vty->length = length;
    /* Redraw current line */
    vty_redraw_line(vty);
}

/* Show next command line history. */
void
vty_next_line(struct vty *vty)
{
    int try_index;
    
    if (vty->hp == vty->hindex)
        return;
        
    /* Try is there history exist or not. */
    try_index = vty->hp;
    
    if (try_index == (VTY_MAXHIST - 1))
        try_index = 0;
        
    else
        try_index++;
        
    /* If there is not history return. */
    if (vty->hist[try_index] == NULL)
        return;
        
    else
        vty->hp = try_index;
        
    vty_history_print(vty);
}

/* Show previous command line history. */
void
vty_previous_line(struct vty *vty)
{
    int try_index;
    try_index = vty->hp;
    
    if (try_index == 0)
        try_index = VTY_MAXHIST - 1;
        
    else
        try_index--;
        
    if (vty->hist[try_index] == NULL)
        return;
        
    else
        vty->hp = try_index;
        
    vty_history_print(vty);
}

/* This function redraw all of the command line character. */
static void
vty_redraw_line(struct vty *vty)
{
    vty_write(vty, vty->buf, vty->length);
    vty->cp = vty->length;
}

/* Forward word. */
static void
vty_forward_word(struct vty *vty)
{
    while (vty->cp != vty->length && vty->buf[vty->cp] != ' ')
        vty_forward_char(vty);
        
    while (vty->cp != vty->length && vty->buf[vty->cp] == ' ')
        vty_forward_char(vty);
}

/* Backward word without skipping training space. */
static void
vty_backward_pure_word(struct vty *vty)
{
    while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
        vty_backward_char(vty);
}

/* Backward word. */
static void
vty_backward_word(struct vty *vty)
{
    while (vty->cp > 0 && vty->buf[vty->cp - 1] == ' ')
        vty_backward_char(vty);
        
    while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
        vty_backward_char(vty);
}

/* When '^D' is typed at the beginning of the line we move to the down
   level. */
static void
vty_down_level(struct vty *vty)
{
    vty_out(vty, "%s", VTY_NEWLINE);
    (*config_exit_cmd.func)(NULL, vty, 0, NULL);
    vty_prompt(vty);
    vty->cp = 0;
}

/* When '^Z' is received from vty, move down to the enable mode. */
void
vty_end_config(struct vty *vty)
{
    vty_out(vty, "%s", VTY_NEWLINE);
    
    switch (vty->node)
    {
        case VIEW_NODE:
        case ENABLE_NODE:
            /* Nothing to do. */
            break;
            
        case FW_CRAFT_NODE:
        case CONFIG_NODE:
        case INTERFACE_NODE:
        case RIPNG_NODE:
        case RMAP_NODE:
        case OSPF_NODE:
        case OSPF6_NODE:
        case ISIS_NODE:
        case MASC_NODE:
        case VTY_NODE:
            vty_config_unlock(vty);
            vty->node = ENABLE_NODE;
            break;
            
        default:
            /* Unknown node, we have to ignore it. */
            break;
    }
    
    vty_prompt(vty);
    vty->cp = 0;
}

/* Delete a charcter at the current point. */
static void
vty_delete_char(struct vty *vty)
{
    int i;
    int size;
    
    if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
        return;
        
    if (vty->length == 0)
    {
        vty_down_level(vty);
        return;
    }
    
    if (vty->cp == vty->length)
        return;         /* completion need here? */
        
    size = vty->length - vty->cp;
    vty->length--;
    memmove(&vty->buf[vty->cp], &vty->buf[vty->cp + 1], size - 1);
    vty->buf[vty->length] = '\0';
    vty_write(vty, &vty->buf[vty->cp], size - 1);
    vty_write(vty, &telnet_space_char, 1);
    
    for (i = 0; i < size; i++)
        vty_write(vty, &telnet_backward_char, 1);
}

/* Delete a character before the point. */
static void
vty_delete_backward_char(struct vty *vty)
{
    if (vty->cp == 0)
        return;
        
    vty_backward_char(vty);
    vty_delete_char(vty);
}

/* Kill rest of line from current point. */
static void
vty_kill_line(struct vty *vty)
{
    int i;
    int size;
    size = vty->length - vty->cp;
    
    if (size == 0)
        return;
        
    for (i = 0; i < size; i++)
        vty_write(vty, &telnet_space_char, 1);
        
    for (i = 0; i < size; i++)
        vty_write(vty, &telnet_backward_char, 1);
        
    memset(&vty->buf[vty->cp], 0, size);
    vty->length = vty->cp;
}

/* Kill line from the beginning. */
static void
vty_kill_line_from_beginning(struct vty *vty)
{
    vty_beginning_of_line(vty);
    vty_kill_line(vty);
}

/* Delete a word before the point. */
static void
vty_forward_kill_word(struct vty *vty)
{
    while (vty->cp != vty->length && vty->buf[vty->cp] == ' ')
        vty_delete_char(vty);
        
    while (vty->cp != vty->length && vty->buf[vty->cp] != ' ')
        vty_delete_char(vty);
}

/* Delete a word before the point. */
static void
vty_backward_kill_word(struct vty *vty)
{
    while (vty->cp > 0 && vty->buf[vty->cp - 1] == ' ')
        vty_delete_backward_char(vty);
        
    while (vty->cp > 0 && vty->buf[vty->cp - 1] != ' ')
        vty_delete_backward_char(vty);
}

/* Transpose chars before or at the point. */
static void
vty_transpose_chars(struct vty *vty)
{
    char c1, c2;
    
    /* If length is short or point is near by the beginning of line then
       return. */
    if (vty->length < 2 || vty->cp < 1)
        return;
        
    /* In case of point is located at the end of the line. */
    if (vty->cp == vty->length)
    {
        c1 = vty->buf[vty->cp - 1];
        c2 = vty->buf[vty->cp - 2];
        vty_backward_char(vty);
        vty_backward_char(vty);
        vty_self_insert_overwrite(vty, c1);
        vty_self_insert_overwrite(vty, c2);
    }
    
    else
    {
        c1 = vty->buf[vty->cp];
        c2 = vty->buf[vty->cp - 1];
        vty_backward_char(vty);
        vty_self_insert_overwrite(vty, c1);
        vty_self_insert_overwrite(vty, c2);
    }
}

/* Do completion at vty interface. */
static void
vty_complete_command(struct vty *vty)
{
    int i;
    int ret;
    char **matched = NULL;
    vector vline;
    
    if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
        return;
        
    vline = cmd_make_strvec(vty->buf);
    
    if (vline == NULL)
        return;
        
    /* In case of 'help \t'. */
    if (isspace((int) vty->buf[vty->length - 1]))
        vector_set(vline, '\0');
        
    matched = cmd_complete_command(vline, vty, &ret);
    cmd_free_strvec(vline);
    vty_out(vty, "%s", VTY_NEWLINE);
    
    switch (ret)
    {
        case CMD_ERR_AMBIGUOUS:
            vty_out(vty, "%% Ambiguous command.%s", VTY_NEWLINE);
            vty_prompt(vty);
            vty_redraw_line(vty);
            break;
            
        case CMD_ERR_NO_MATCH:
            /* vty_out (vty, "%% There is no matched command.%s", VTY_NEWLINE); */
            vty_prompt(vty);
            vty_redraw_line(vty);
            break;
            
        case CMD_COMPLETE_FULL_MATCH:
            vty_prompt(vty);
            vty_redraw_line(vty);
            vty_backward_pure_word(vty);
            vty_insert_word_overwrite(vty, matched[0]);
            vty_self_insert(vty, ' ');
            XFREE(MTYPE_TMP, matched[0]);
            break;
            
        case CMD_COMPLETE_MATCH:
            vty_prompt(vty);
            vty_redraw_line(vty);
            vty_backward_pure_word(vty);
            vty_insert_word_overwrite(vty, matched[0]);
            XFREE(MTYPE_TMP, matched[0]);
            vector_only_index_free(matched);
            return;
            break;
            
        case CMD_COMPLETE_LIST_MATCH:
            for (i = 0; matched[i] != NULL; i++)
            {
                /* Changed by Tsihang. make TAB key to show option vertical, in order to make it
                   compatible to Cisco.   */
                /*    if (i != 0 && ((i % 6) == 0))
                      vty_out (vty, "%s", VTY_NEWLINE);
                      vty_out (vty, "%-10s %s", matched[i], VTY_NEWLINE);
                      XFREE (MTYPE_TMP, matched[i]);  */
                vty_out(vty, "  %s%s", matched[i], VTY_NEWLINE);
                XFREE(MTYPE_TMP, matched[i]);
            }
            
            /*      vty_out (vty, "%s", VTY_NEWLINE); */
            vty_prompt(vty);
            vty_redraw_line(vty);
            break;
            
        case CMD_ERR_NOTHING_TODO:
            vty_prompt(vty);
            vty_redraw_line(vty);
            break;
            
        default:
            break;
    }
    
    if (matched)
        vector_only_index_free(matched);
}

void
vty_describe_fold(struct vty *vty, int cmd_width,
                  unsigned int desc_width, struct desc *desc)
{
    char *buf;
    const char *cmd, *p;
    int pos;
    cmd = desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd;
    
    if (desc_width <= 0)
    {
        vty_out(vty, "  %-*s  %s%s", cmd_width, cmd, desc->str, VTY_NEWLINE);
        return;
    }
    
    buf = XCALLOC(MTYPE_TMP, strlen(desc->str) + 1);
    
    for (p = desc->str; strlen(p) > desc_width; p += pos + 1)
    {
        for (pos = desc_width; pos > 0; pos--)
            if (* (p + pos) == ' ')
                break;
                
        if (pos == 0)
            break;
            
        strncpy(buf, p, pos);
        buf[pos] = '\0';
        vty_out(vty, "  %-*s  %s%s", cmd_width, cmd, buf, VTY_NEWLINE);
        cmd = "";
    }
    
    vty_out(vty, "  %-*s  %s%s", cmd_width, cmd, p, VTY_NEWLINE);
    XFREE(MTYPE_TMP, buf);
}

/* Describe matched command function. */
static void
vty_describe_command(struct vty *vty)
{
    int ret;
    vector vline;
    vector describe;
    unsigned int i, width, desc_width;
    struct desc *desc, *desc_cr = NULL;
    vline = cmd_make_strvec(vty->buf);
    
    /* In case of '> ?'. */
    if (vline == NULL)
    {
        vline = vector_init(1);
        vector_set(vline, '\0');
    }
    
    else if (isspace((int) vty->buf[vty->length - 1]))
        vector_set(vline, '\0');
        
    describe = cmd_describe_command(vline, vty, &ret);
    vty_out(vty, "%s", VTY_NEWLINE);
    
    /* Ambiguous error. */
    switch (ret)
    {
        case CMD_ERR_AMBIGUOUS:
            cmd_free_strvec(vline);
            vty_out(vty, "%% Ambiguous command.%s", VTY_NEWLINE);
            vty_prompt(vty);
            vty_redraw_line(vty);
            return;
            break;
            
        case CMD_ERR_NO_MATCH:
            cmd_free_strvec(vline);
            vty_out(vty, "%% There is no matched command.%s", VTY_NEWLINE);
            vty_prompt(vty);
            vty_redraw_line(vty);
            return;
            break;
    }
    
    /* Get width of command string. */
    width = 0;
    
    for (i = 0; i < vector_max(describe); i++)
        if ((desc = vector_slot(describe, i)) != NULL)
        {
            unsigned int len;
            
            if (desc->cmd[0] == '\0')
                continue;
                
            len = strlen(desc->cmd);
            
            if (desc->cmd[0] == '.')
                len--;
                
            if (width < len)
                width = len;
        }
        
    /* Get width of description string. */
    desc_width = vty->width - (width + 6);
    
    /* Print out description. */
    for (i = 0; i < vector_max(describe); i++)
        if ((desc = vector_slot(describe, i)) != NULL)
        {
            if (desc->cmd[0] == '\0')
                continue;
                
            if (strcmp(desc->cmd, "<cr>") == 0)
            {
                desc_cr = desc;
                continue;
            }
            
            if (!desc->str || !desc->cstr)
                vty_out(vty, "  %-s%s",
                        desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
                        VTY_NEWLINE);
                        
            else if ((desc_width >= strlen(desc->str)) && (desc_width >= strlen(desc->cstr)))
                vty_out(vty, "  %-*s  %s%s", \
                        width,
                        desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
                        (vty->language == LAUGUAGE_ENGLISH) ? desc->str : desc->cstr,
                        VTY_NEWLINE);
                        
            else
                vty_describe_fold(vty, width, desc_width, desc);
                
#if 0
            vty_out(vty, "  %-*s %s%s", width
                    desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
                    desc->str ? desc->str : "", VTY_NEWLINE);
#endif /* 0 */
        }
        
    if ((desc = desc_cr))
    {
        if (!desc->str || !desc->cstr)
            vty_out(vty, "  %-s%s",
                    desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
                    VTY_NEWLINE);
                    
        else if ((desc_width >= strlen(desc->str)) && (desc_width >= strlen(desc->cstr)))
            vty_out(vty, "  %-*s  %s%s",
                    width,
                    desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
                    (vty->language == LAUGUAGE_ENGLISH) ? desc->str : desc->cstr,
                    VTY_NEWLINE);
                    
        else
            vty_describe_fold(vty, width, desc_width, desc);
    }
    
    cmd_free_strvec(vline);
    vector_free(describe);
    vty_prompt(vty);
    vty_redraw_line(vty);
}

/* ^C stop current input and do not add command line to the history. */
static void
vty_stop_input(struct vty *vty)
{
    vty->cp = vty->length = 0;
    vty_clear_buf(vty);
    vty_out(vty, "%s", VTY_NEWLINE);
    
    switch (vty->node)
    {
        case VIEW_NODE:
        case ENABLE_NODE:
            /* Nothing to do. */
            break;
            
        case CONFIG_NODE:
        case INTERFACE_NODE:
        case RIPNG_NODE:
        case RMAP_NODE:
        case OSPF_NODE:
        case OSPF6_NODE:
        case ISIS_NODE:
        case MASC_NODE:
        case VTY_NODE:
        case FW_CRAFT_NODE:
            vty_config_unlock(vty);
            vty->node = ENABLE_NODE;
            break;
            
        default:
            /* Unknown node, we have to ignore it. */
            break;
    }
    
    vty_prompt(vty);
    /* Set history pointer to the latest one. */
    vty->hp = vty->hindex;
}

/* Add current command line to the history buffer. */
static void
vty_hist_add(struct vty *vty)
{
    int index;
    
    if (vty->length == 0)
        return;
        
    index = vty->hindex ? vty->hindex - 1 : VTY_MAXHIST - 1;
    
    /* Ignore the same string as previous one. */
    if (vty->hist[index])
        if (strcmp(vty->buf, vty->hist[index]) == 0)
        {
            vty->hp = vty->hindex;
            return;
        }
        
    /* Insert history entry. */
    if (vty->hist[vty->hindex])
        XFREE(MTYPE_VTY_HIST, vty->hist[vty->hindex]);
        
    vty->hist[vty->hindex] = XSTRDUP(MTYPE_VTY_HIST, vty->buf);
    /* History index rotation. */
    vty->hindex++;
    
    if (vty->hindex == VTY_MAXHIST)
        vty->hindex = 0;
        
    vty->hp = vty->hindex;
}

/* #define TELNET_OPTION_DEBUG */

/* Get telnet window size. */
static int
vty_telnet_option(struct vty *vty, unsigned char *buf, int nbytes)
{
    nbytes = nbytes;
#ifdef TELNET_OPTION_DEBUG
    int i;
    
    for (i = 0; i < nbytes; i++)
    {
        switch (buf[i])
        {
            case IAC:
                vty_out(vty, "IAC ");
                break;
                
            case WILL:
                vty_out(vty, "WILL ");
                break;
                
            case WONT:
                vty_out(vty, "WONT ");
                break;
                
            case DO:
                vty_out(vty, "DO ");
                break;
                
            case DONT:
                vty_out(vty, "DONT ");
                break;
                
            case SB:
                vty_out(vty, "SB ");
                break;
                
            case SE:
                vty_out(vty, "SE ");
                break;
                
            case TELOPT_ECHO:
                vty_out(vty, "TELOPT_ECHO %s", VTY_NEWLINE);
                break;
                
            case TELOPT_SGA:
                vty_out(vty, "TELOPT_SGA %s", VTY_NEWLINE);
                break;
                
            case TELOPT_NAWS:
                vty_out(vty, "TELOPT_NAWS %s", VTY_NEWLINE);
                break;
                
            default:
                vty_out(vty, "%x ", buf[i]);
                break;
        }
    }
    
    vty_out(vty, "%s", VTY_NEWLINE);
#endif /* TELNET_OPTION_DEBUG */
    
    switch (buf[0])
    {
        case SB:
            buffer_reset(vty->sb_buffer);
            vty->iac_sb_in_progress = 1;
            return 0;
            break;
            
        case SE:
        {
            char *buffer;
            int length;
            
            if (!vty->iac_sb_in_progress)
                return 0;
                
            buffer = (char *) vty->sb_buffer->head->data;
            length = vty->sb_buffer->length;
            
            if (buffer == NULL)
                return 0;
                
            if (buffer[0] == '\0')
            {
                vty->iac_sb_in_progress = 0;
                return 0;
            }
            
            switch (buffer[0])
            {
                case TELOPT_NAWS:
                    if (length < 5)
                        break;
                        
                    vty->width = buffer[2];
                    vty->height = vty->lines >= 0 ? vty->lines : buffer[4];
                    break;
            }
            
            vty->iac_sb_in_progress = 0;
            return 0;
            break;
        }
        
        default:
            break;
    }
    
    return 1;
}


/* Modify by Tsihang */
int execute_fs_cli_command(struct vty *vty)
{
    strcpy(vty->buf, "");
    vty->length = 0;
    return CMD_SUCCESS;
}



/* Execute current command line. */
static int
vty_execute(struct vty *vty)
{
    int ret;
    ret = CMD_SUCCESS;
    
    switch (vty->node)
    {
        case FW_CRAFT_NODE:
            if (strcmp(vty->buf, "exit"))
                execute_fs_cli_command(vty);
                
            else
            {
                ret = vty_command(vty, vty->buf);
                
                if (vty->type == VTY_TERM)
                    vty_hist_add(vty);
            }
            
            break;
#ifdef __USER_AUTH__
            
        case AUTH_USER_NODE:
#endif
        case AUTH_NODE:
        case AUTH_ENABLE_NODE:
            vty_auth(vty, vty->buf);
            break;
            
        default:
            ret = vty_command(vty, vty->buf);
            
            if (vty->type == VTY_TERM)
                vty_hist_add(vty);
                
            break;
    }
    
    /* Clear command line buffer. */
    vty->cp = vty->length = 0;
    vty_clear_buf(vty);
    
    if (vty->status != VTY_CLOSE)
        vty_prompt(vty);
        
    return ret;
}

#define CONTROL(X)  ((X) - '@')
#define VTY_NORMAL     0
#define VTY_PRE_ESCAPE 1
#define VTY_ESCAPE     2

/* Escape character command map. */
static void
vty_escape_map(unsigned char c, struct vty *vty)
{
    switch (c)
    {
        case ('A') :
            vty_previous_line(vty);
            break;
            
        case ('B') :
            vty_next_line(vty);
            break;
            
        case ('C') :
            vty_forward_char(vty);
            break;
            
        case ('D') :
            vty_backward_char(vty);
            break;
            
        default:
            break;
    }
    
    /* Go back to normal mode. */
    vty->escape = VTY_NORMAL;
}

/* Quit print out to the buffer. */
static void
vty_buffer_reset(struct vty *vty)
{
    buffer_reset(vty->obuf);
    vty_prompt(vty);
    vty_redraw_line(vty);
}

static char is_valid_vty(struct vty *vty)
{
    if (vty_lookup_vector_is_exist(vty) && (vty->status != VTY_CLOSE) && (vty->alive))
        return 1;
        
    return 0;
}

/* Read data via vty socket. */
static int
vty_read1(struct vty *vty)
{
    int i;
    int nbytes;
    unsigned char buf[VTY_READ_BUFSIZ];
    int vty_sock = vty->fd;
    vty->t_read = NULL;
    /* Read raw data from socket */
    nbytes = read(vty->fd, buf, VTY_READ_BUFSIZ);
    
    if (nbytes <= 0)
        vty->status = VTY_CLOSE;
        
    for (i = 0; i < nbytes; i++)
    {
        if (buf[i] == IAC)
        {
            if (!vty->iac)
            {
                vty->iac = 1;
                continue;
            }
            
            else
            {
                vty->iac = 0;
            }
        }
        
        if (vty->iac_sb_in_progress && !vty->iac)
        {
            buffer_putc(vty->sb_buffer, buf[i]);
            continue;
        }
        
        if (vty->iac)
        {
            /* In case of telnet command */
            int ret = 0;
            ret = vty_telnet_option(vty, buf + i, nbytes - i);
            vty->iac = 0;
            i += ret;
            continue;
        }
        
        if (vty->status == VTY_MORE)
        {
            switch (buf[i])
            {
                case CONTROL('C') :
                case 'q':
                case 'Q':
                    vty_buffer_reset(vty);
                    break;
#if 0 /* More line does not work for "show ip bgp".  */
                    
                case '\n':
                case '\r':
                    vty->status = VTY_MORELINE;
                    break;
#endif
                    
                default:
                    break;
            }
            
            continue;
        }
        
        /* Escape character. */
        if (vty->escape == VTY_ESCAPE)
        {
            vty_escape_map(buf[i], vty);
            continue;
        }
        
        /* Pre-escape status. */
        if (vty->escape == VTY_PRE_ESCAPE)
        {
            switch (buf[i])
            {
                case '[':
                    vty->escape = VTY_ESCAPE;
                    break;
                    
                case 'b':
                    vty_backward_word(vty);
                    vty->escape = VTY_NORMAL;
                    break;
                    
                case 'f':
                    vty_forward_word(vty);
                    vty->escape = VTY_NORMAL;
                    break;
                    
                case 'd':
                    vty_forward_kill_word(vty);
                    vty->escape = VTY_NORMAL;
                    break;
                    
                case CONTROL('H') :
                case 0x7f:
                    vty_backward_kill_word(vty);
                    vty->escape = VTY_NORMAL;
                    break;
                    
                default:
                    vty->escape = VTY_NORMAL;
                    break;
            }
            
            continue;
        }
        
        switch (buf[i])
        {
            case CONTROL('A') :
                vty_beginning_of_line(vty);
                break;
                
            case CONTROL('B') :
                vty_backward_char(vty);
                break;
                
            case CONTROL('C') :
                vty_stop_input(vty);
                break;
                
            case CONTROL('D') :
                vty_delete_char(vty);
                break;
                
            case CONTROL('E') :
                vty_end_of_line(vty);
                break;
                
            case CONTROL('F') :
                vty_forward_char(vty);
                break;
                
            case CONTROL('H') :
            case 0x7f:
                vty_delete_backward_char(vty);
                break;
                
            case CONTROL('K') :
                vty_kill_line(vty);
                break;
                
            case CONTROL('N') :
                vty_next_line(vty);
                break;
                
            case CONTROL('P') :
                vty_previous_line(vty);
                break;
                
            case CONTROL('T') :
                vty_transpose_chars(vty);
                break;
                
            case CONTROL('U') :
                vty_kill_line_from_beginning(vty);
                break;
                
            case CONTROL('W') :
                vty_backward_kill_word(vty);
                break;
                
            case CONTROL('Z') :
                vty_end_config(vty);
                break;
                
            case '\n':
            case '\r':
                vty_out(vty, "%s", VTY_NEWLINE);
                
                /* Flush buffer. */
                if (! buffer_empty(vty->obuf))
                    buffer_flush_all(vty->obuf, vty->fd);
                    
                vty_execute(vty);
                break;
                
            case '\t':
                vty_complete_command(vty);
                break;
                
            case '?':
                if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
                    vty_self_insert(vty, buf[i]);
                    
                else
                    vty_describe_command(vty);
                    
                break;
                
            case '\033':
                if (i + 1 < nbytes && buf[i + 1] == '[')
                {
                    vty->escape = VTY_ESCAPE;
                    i++;
                }
                
                else
                    vty->escape = VTY_PRE_ESCAPE;
                    
                break;
                
            default:
                if (buf[i] > 31 && buf[i] < 127)
                    vty_self_insert(vty, buf[i]);
                    
                break;
        }
    }
    
    /* Check status. */
    if (!vty->alive)
        return VTY_CLOSE_ERROR; /* closed */
        
    if (vty->status == VTY_CLOSE)
    {
        vty_close(vty);
        return VTY_CLOSE_ERROR; /* closed */
    }
    
    else
    {
#if 0
    
        if (multi_tasking_mode)
        {
            /* Flush buffer. */
            if (! buffer_empty(vty->obuf))
                buffer_flush_all(vty->obuf, vty->fd);
                
            /* vty_event (VTY_WRITE, vty->fd, vty); */
            /* vty_event (VTY_READ, vty->fd, vty);  */
        }
        
        else
#endif
        {
            vty_event(VTY_WRITE, vty_sock, vty);
            vty_event(VTY_READ, vty_sock, vty);
        }
    }
    
    return 0;
}

/*added by tsihang to deal with socket read */
int
vty_socket_read(struct thread *thread)
{
    int i;
    int nbytes;
    unsigned char buf[VTY_READ_BUFSIZ];
    int vty_sock = THREAD_FD(thread);
    struct vty *vty = THREAD_ARG(thread);
    vty->t_read = NULL;
    /* Read raw data from socket */
    memset(buf, 0, VTY_READ_BUFSIZ);
    nbytes = read(vty->fd, buf, VTY_READ_BUFSIZ);
    
    /*if (nbytes <= 0)
      vty->status = VTY_CLOSE;
    */
    for (i = 0; i < nbytes; i++)
    {
        if (buf[i] == IAC)
        {
            if (!vty->iac)
            {
                vty->iac = 1;
                continue;
            }
            
            else
            {
                vty->iac = 0;
            }
        }
        
        if (vty->iac_sb_in_progress && !vty->iac)
        {
            /*buffer_putc( vty->sb_buffer, buf[i]);*/
            continue;
        }
        
#if 0
        
        if (vty->iac)
        {
            /* In case of telnet command */
            ret = vty_telnet_option(vty, buf + i, nbytes - i);
            vty->iac = 0;
            i += ret;
            continue;
        }
        
#endif
        
        switch (buf[i])
        {
            case '\n':
            case '\r':
                /*for(j=0; j < VTYSH_INDEX_MAX; j ++)
                {
                
                #if 1
                if(vty->vtysh_client[j].fd > 0)
                    {
                        close(vty->vtysh_client[j].fd);
                    }
                #endif
                
                
                            vty->vtysh_client[j].fd=-1;
                        }
                       if(vtysh_connect_all( vty) <0)
                       {
                      vty_out ( vty, "No socket resource! Waiting for several seconds!%s", VTY_NEWLINE);
                      break;
                       }*/
                vty_out(vty, "%s", VTY_NEWLINE);
                vty_hello(vty);
                
                if (! no_password_check)
                    vty_out(vty, "%sUser Access Verification%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
                    
                if (no_password_check)
                {
                    if (host.advanced)
                        vty->node = ENABLE_NODE;
                        
                    else
                        vty->node = VIEW_NODE;
                }
                
                else
                    vty->node = AUTH_NODE;
                    
                /* vty_dont_lflow_ahead (vty); */
                vty->socket_close = 0;
                vty_prompt(vty);
                vty->cp = vty->length = 0;
                vty_clear_buf(vty);
                i = nbytes;
                memset(buf, 0, VTY_READ_BUFSIZ);
                break;
                
            default:
                vty_out(vty, " Console Interface has exited now!%s", VTY_NEWLINE);
                vty_out(vty, " Please enter \"Enter\" to enable Console Interface %s", VTY_NEWLINE);
                vty->cp = vty->length = 0;
                vty_clear_buf(vty);
                i = nbytes;
                memset(buf, 0, VTY_READ_BUFSIZ);
                break;
        }
    }
    
    vty_event(VTY_WRITE, vty_sock, vty);
    vty_event(VTY_READ, vty_sock, vty);
    return 0;
}

static int
vty_read(struct thread *thread)
{
    struct vty *vty = THREAD_ARG(thread);
    
    if (!is_valid_vty(vty))
        return 0;
        
    {
        vty_read1(vty);    /* vty_read1 direct call */
    }
    
    return 0;
}

/* Flush buffer to the vty. */
int
vty_flush(struct thread *thread)
{
    int erase;
    int vty_sock = THREAD_FD(thread);
    struct vty *vty = THREAD_ARG(thread);
    
    if (!is_valid_vty(vty))
    {
        return 0;
    }
    
    vty->t_write = NULL;
    
    /* Tempolary disable read thread. */
    if (vty->lines == 0)
        if (vty->t_read)
        {
            thread_cancel(vty->t_read);
            vty->t_read = NULL;
        }
        
    /* Function execution continue. */
    if (vty->status == VTY_MORE || vty->status == VTY_MORELINE)
        erase = 1;
        
    else
        erase = 0;
        
    if (vty->lines == 0)
        buffer_flush_window(vty->obuf, vty->fd, vty->width, 25, 0, 1);
        
    else if (vty->status == VTY_MORELINE)
        buffer_flush_window(vty->obuf, vty->fd, vty->width, 1, erase, 0);
        
    else
        buffer_flush_window(vty->obuf, vty->fd, vty->width,
                            vty->lines >= 0 ? vty->lines : vty->height,
                            erase, 0);
                            
    if (buffer_empty(vty->obuf))
    {
        if (vty->status == VTY_CLOSE)
        {
            vty_close(vty);
        }
        
        else
        {
            vty->status = VTY_NORMAL;
            
            if (vty->lines == 0)
                vty_event(VTY_READ, vty_sock, vty);
        }
    }
    
    else
    {
        vty->status = VTY_MORE;
        
        if (vty->lines == 0)
            vty_event(VTY_WRITE, vty_sock, vty);
    }
    
    return 0;
}

unsigned int get_vty_count()
{
    unsigned int    i, count = 0;
    struct          vty *vty;
    
    for (i = 0; i < vector_max(vtyvec); i++)
        if ((vty = vector_slot(vtyvec, i)) != NULL)
            count++;
            
    return count;
}

/* Create new vty structure. */
struct vty *
vty_create(int vty_sock, union sockunion *su)
{
    struct vty *vty;
#if 0
    int result; 
    /* init the global semaphore only once */
    if (get_vty_count() == 0)
    {        
        if (result != 0)
        {
            return (NULL);
        }
    }
#endif    
    /* Allocate new vty structure and set up default values. */
    vty = vty_new();
    
    vty->status = VTY_NORMAL;
    vty->alive = 1;
        
    vty->fd = vty_sock;
    vty->type = VTY_TERM;
    vty->address = SOCKUNION_SU2STR(su);
    
    /* Changed by tsihang, in order to enable empty password */
    if (!host.password)
        no_password_check = 1;
        
    if (no_password_check)
    {
        /*      if (host.advanced)
            vty->node = ENABLE_NODE;
              else */
        vty->node = VIEW_NODE;
    }
    else
    {
#ifdef __USER_AUTH__
        vty->node = AUTH_USER_NODE;
#else
        vty->node = AUTH_NODE;
#endif
    }
    
    vty->fail = 0;
    vty->cp = 0;
    vty_clear_buf(vty);
    vty->length = 0;
    memset(vty->hist, 0, sizeof(vty->hist));
    vty->hp = 0;
    vty->hindex = 0;
    vector_set_index(vtyvec, vty_sock, vty);
    vty_lookup_vector_add(vty);
    vty->v_timeout = vty_timeout_val;
    
    if (host.lines >= 0)
        vty->lines = host.lines;
        
    else
        /** vty->lines = -1; */
        vty->lines = VTY_CONFIGURE_LINES_DEFAULT;
        
        
    vty->iac = 0;
    vty->iac_sb_in_progress = 0;
    vty->sb_buffer = buffer_new(1024);
    
    if (! no_password_check)
    {
        /* Vty is not available if password isn't set. */
        /*      if (host.password == NULL && host.password_encrypt == NULL)
            {
              vty_out (vty, "Vty password is not set.%s", VTY_NEWLINE);
              vty->status = VTY_CLOSE;
              vty_close (vty);
              return NULL;
            } */
    }
    
    /* Say hello to the world. */
    vty_hello(vty);
    
    if (! no_password_check)
        vty_out(vty, "%sUser Access Verification%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
        
    /* Setting up terminal. */
    vty_will_echo(vty);
    vty_will_suppress_go_ahead(vty);
    
    vty_dont_linemode(vty);
    vty_do_window_size(vty);
    /* vty_dont_lflow_ahead (vty); */
    
    vty_prompt(vty);
    
    /* Add read/write thread. */
    vty_event(VTY_WRITE, vty_sock, vty);
    vty_event(VTY_READ, vty_sock, vty);
    
    return vty;
}

unsigned int vty_time = 0, vty_counter = 0;
#define MAX_VTY_CONN_COUNTER        10
#define VTY_INTERVAL                2
//#define VTY_ACCESS_CHK    1

#define VTY_REQUEST_REFUSING_WARNING        " \r\n Request ip address is denied !\r\n"

/* Allow telnet access by all IP addresses when null(default),
 */
char telnet_permit_ip_range [VTY_TELNET_PERMIT_IP_RANGE_MAX_COUNT][20] = {{0, 0}};
/* Accept connection from the network. */
static int
vty_accept(struct thread *thread)
{
    int vty_sock;
    struct vty *vty;
    union sockunion su;
    int ret;
    unsigned int on;
    int accept_sock;
    struct prefix *p = NULL;
    struct access_list *acl = NULL;
#ifdef VTY_ACCESS_CHK /** Add by tsihang */
    struct prefix p2;
    unsigned int current_time;
#endif
    accept_sock = THREAD_FD(thread);
    /* We continue hearing vty socket. */
    vty_event(VTY_SERV, accept_sock, NULL);
    memset(&su, 0, sizeof(union sockunion));
    /* We can handle IPv4 or IPv6 socket. */
    vty_sock = sockunion_accept(accept_sock, &su);
    
    if (vty_sock < 0)
    {
        fprintf(stdout, " Warning, can't accept vty socket : %s", safe_strerror(errno));
        return -1;
    }
    
    p = sockunion2hostprefix(&su);
    
    /* VTY's accesslist apply. */
    if (p->family == AF_INET && vty_accesslist_name)
    {
        if ((acl = access_list_lookup(AFI_IP, vty_accesslist_name)) &&
                (access_list_apply(acl, p) == FILTER_DENY))
        {
            char *buf;
            fprintf(stdout, "Vty connection refused from %s",
                    (buf = sockunion_su2str(&su)));
            free(buf);
            close(vty_sock);
            /* continue accepting connections */
            vty_event(VTY_SERV, accept_sock, NULL);
            prefix_free(p);
            return 0;
        }
    }
    
#ifdef VTY_ACCESS_CHK /** Add by tsihang */
    /** Added by tsihang for access limit */
    int i = 0;
    unsigned long check_addr1, check_addr2;
    struct in_addr mask;
    str2prefix("127.0.0.1/8", &p2);
    masklen2ip(p2.prefixlen, &mask);
    check_addr1 = (su.sin.sin_addr.s_addr & mask.s_addr);
    check_addr2 = (p2.u.prefix4.s_addr & mask.s_addr);
    
    if (check_addr1 == check_addr2)
        goto ACCESS_CHK;
        
    char addrstr[64];
#include <sys/types.h>
#include <ifaddrs.h>
    struct ifaddrs *ifAddrStruct = NULL;
    void *tmpAddrPtr = NULL;
    getifaddrs(&ifAddrStruct);
    
    /** Check all ipaddress list, compare with request ipaddress until find a match one */
    while (ifAddrStruct != NULL)
    {
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET)
        {
            char address [INET_ADDRSTRLEN], addrMask [INET6_ADDRSTRLEN];
            tmpAddrPtr = & ((struct sockaddr_in *) ifAddrStruct->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, address, INET_ADDRSTRLEN);
            tmpAddrPtr = & ((struct sockaddr_in *) ifAddrStruct->ifa_netmask)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, addrMask, INET_ADDRSTRLEN);
            /**
            printf("%s IP Address %s mask %s\n",
                ifAddrStruct->ifa_name, address, addrMask);
            */
            strncpy(addrstr, address, INET_ADDRSTRLEN);
            str2prefix(addrstr, &p2);
            /** An Local accurate address which can be access from outside, like 192.168.40.244 */
            check_addr1 = su.sin.sin_addr.s_addr;
            check_addr2 = p2.u.prefix4.s_addr;
            
            if (check_addr1 == check_addr2)
                goto ACCESS_CHK;
        }
        
        else if (ifAddrStruct->ifa_addr->sa_family == AF_INET6)
        {
            tmpAddrPtr = & ((struct sockaddr_in *) ifAddrStruct->ifa_addr)->sin_addr;
            char address[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, address, INET6_ADDRSTRLEN);
            printf("%s IP Address %s/n", ifAddrStruct->ifa_name, address);
            strncpy(addrstr, address, INET_ADDRSTRLEN);
            str2prefix(addrstr, &p2);
            check_addr1 = su.sin.sin_addr.s_addr;
            check_addr2 = p2.u.prefix4.s_addr;
            
            if (check_addr1 == check_addr2)
                goto ACCESS_CHK;
        }
        
        ifAddrStruct = ifAddrStruct->ifa_next;
    }
    
    str2prefix(addrstr, &p2);
    check_addr1 = su.sin.sin_addr.s_addr;
    check_addr2 = p2.u.prefix4.s_addr;
    
    if (check_addr1 == check_addr2)
        goto ACCESS_CHK;
        
    /** Check ipaddress white list, find a match one which in the same ipaddress segment (use ipmask) */
    for (i = 0; i < VTY_TELNET_PERMIT_IP_RANGE_MAX_COUNT; i++)
    {
        if (telnet_permit_ip_range[i][0] != 0)
        {
            str2prefix(telnet_permit_ip_range[i], &p2);
            masklen2ip(p2.prefixlen, &mask);
            check_addr1 = (su.sin.sin_addr.s_addr & mask.s_addr);
            check_addr2 = (p2.u.prefix4.s_addr & mask.s_addr);
            
            /*printf( "  \r\ncheck_addr1=%08x, check_addr2=%08x, mask=%08x\r\n", check_addr1, check_addr2, mask.s_addr);*/
            if (check_addr1 == check_addr2)
                break;
        }
    }
    
    if (i == VTY_TELNET_PERMIT_IP_RANGE_MAX_COUNT)
    {
        write(vty_sock, VTY_REQUEST_REFUSING_WARNING, strlen(VTY_REQUEST_REFUSING_WARNING));
        close(vty_sock);
        return -1;
    }
    
#endif
#ifdef HAVE_IPV6
    
    /* VTY's ipv6 accesslist apply. */
    if (p->family == AF_INET6 && vty_ipv6_accesslist_name)
    {
        if ((acl = access_list_lookup(AFI_IP6, vty_ipv6_accesslist_name)) &&
                (access_list_apply(acl, p) == FILTER_DENY))
        {
            char *buf;
            zlog(NULL, LOG_INFO, "Vty connection refused from %s",
                 (buf = sockunion_su2str(&su)));
            free(buf);
            close(vty_sock);
            /* continue accepting connections */
            vty_event(VTY_SERV, accept_sock, NULL);
            prefix_free(p);
            return 0;
        }
    }
    
#endif /* HAVE_IPV6 */
#ifdef VTY_ACCESS_CHK /** Add by tsihang */
ACCESS_CHK:
    current_time = time(NULL);
    
    if (vty_counter >= MAX_VTY_CONN_COUNTER)
    {
        /*          printf( "Error: vty counter is beyond %d %s",  MAX_VTY_CONN_COUNTER, VTY_NEWLINE);*/
        close(vty_sock);
        return -1;
    }
    
    if ((current_time - vty_time) < VTY_INTERVAL)
    {
        /*      printf( "Error: vty interval is less than %d %s ",  VTY_INTERVAL, VTY_NEWLINE);*/
        close(vty_sock);
        return -1;
    }
    
    vty_counter ++;
    vty_time = current_time;
#endif
    prefix_free(p);
    on = 1;
    ret = setsockopt(vty_sock, IPPROTO_TCP, TCP_NODELAY,
                     (char *) &on, sizeof(on));
                     
    if (ret < 0)
        fprintf(stderr, "Error!, can't set sockopt to vty_sock : %s",
                safe_strerror(errno));
                
    vty = vty_create(vty_sock, &su);
    vty = vty;
    return 0;
}

#if defined(HAVE_IPV6) && !defined(NRL)
void
vty_serv_sock_addrinfo(const char *hostname, unsigned short port)
{
    int ret;
    struct addrinfo req;
    struct addrinfo *ainfo;
    struct addrinfo *ainfo_save;
    int sock;
    char port_str[BUFSIZ];
    memset(&req, 0, sizeof(struct addrinfo));
    req.ai_flags = AI_PASSIVE;
    req.ai_family = AF_UNSPEC;
    req.ai_socktype = SOCK_STREAM;
    sprintf(port_str, "%d", port);
    port_str[sizeof(port_str) - 1] = '\0';
    ret = getaddrinfo(hostname, port_str, &req, &ainfo);
    
    if (ret != 0)
    {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret));
        exit(1);
    }
    
    ainfo_save = ainfo;
    
    do
    {
        if (ainfo->ai_family != AF_INET
#ifdef HAVE_IPV6
                && ainfo->ai_family != AF_INET6
#endif /* HAVE_IPV6 */
           )
            continue;
            
        sock = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
        
        if (sock < 0)
            continue;
            
        sockopt_reuseaddr(sock);
        sockopt_reuseport(sock);
        ret = bind(sock, ainfo->ai_addr, ainfo->ai_addrlen);
        
        if (ret < 0)
        {
            close(sock);    /* Avoid sd leak. */
            continue;
        }
        
        ret = listen(sock, 3);
        
        if (ret < 0)
        {
            close(sock);    /* Avoid sd leak. */
            continue;
        }
        
        vty_event(VTY_SERV, sock, NULL);
    }
    while ((ainfo = ainfo->ai_next) != NULL);
    
    freeaddrinfo(ainfo_save);
}
#endif /* HAVE_IPV6 && ! NRL */

/* Make vty server socket. */
void
vty_serv_sock_family(const char *addr, unsigned short port, int family)
{
    int ret;
    union sockunion su;
    int accept_sock;
    void *naddr = NULL;
    memset(&su, 0, sizeof(union sockunion));
    su.sa.sa_family = family;
    
    if (addr)
        switch (family)
        {
            case AF_INET:
                naddr = &su.sin.sin_addr;
#ifdef HAVE_IPV6
                
            case AF_INET6:
                naddr = &su.sin6.sin6_addr;
#endif
        }
        
    if (naddr)
        switch (inet_pton(family, addr, naddr))
        {
            case -1:
                fprintf(stderr, "Error! bad address %s", addr);
                naddr = NULL;
                break;
                
            case 0:
                fprintf(stderr, "Error! translating address %s: %s", addr, safe_strerror(errno));
                naddr = NULL;
        }
        
    /* Make new socket. */
    accept_sock = sockunion_stream_socket(&su);
    
    if (accept_sock < 0)
        return;
        
    /* This is server, so reuse address. */
    sockopt_reuseaddr(accept_sock);
    sockopt_reuseport(accept_sock);
    /* Bind socket to universal address and given port. */
    ret = sockunion_bind(accept_sock, &su, port, naddr);
    
    if (ret < 0)
    {
        /** fprintf(stderr, "Warning, can't bind socket"); */
        close(accept_sock);    /* Avoid sd leak. */
        return;
    }
    
    /* Listen socket under queue 3. */
    ret = listen(accept_sock, 3);
    
    if (ret < 0)
    {
        /** fprintf(stderr, "Warning, can't listen socket");*/
        close(accept_sock);    /* Avoid sd leak. */
        return;
    }
    
    /* Add vty server event. */
    vty_event(VTY_SERV, accept_sock, NULL);
}

#ifdef VTYSH
/* For sockaddr_un. */
#include <sys/un.h>

/* VTY shell UNIX domain socket. */
void
vty_serv_un(const char *path)
{
    int ret;
    int sock, len;
    struct sockaddr_un serv;
    mode_t old_mask;
    struct zprivs_ids_t ids;
    /* First of all, unlink existing socket */
    unlink(path);
    /* Set umask */
    old_mask = umask(0007);
    /* Make UNIX domain socket. */
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    
    if (sock < 0)
    {
        perror("sock");
        return;
    }
    
    /* Make server socket. */
    memset(&serv, 0, sizeof(struct sockaddr_un));
    serv.sun_family = AF_UNIX;
    strncpy(serv.sun_path, path, strlen(path));
#ifdef HAVE_SUN_LEN
    len = serv.sun_len = SUN_LEN(&serv);
#else
    len = sizeof(serv.sun_family) + strlen(serv.sun_path);
#endif /* HAVE_SUN_LEN */
    ret = bind(sock, (struct sockaddr *) &serv, len);
    
    if (ret < 0)
    {
        perror("bind");
        close(sock);    /* Avoid sd leak. */
        return;
    }
    
    ret = listen(sock, 5);
    
    if (ret < 0)
    {
        perror("listen");
        close(sock);    /* Avoid sd leak. */
        return;
    }
    
    umask(old_mask);
    zprivs_get_ids(&ids);
    
    if (ids.gid_vty > 0)
    {
        /* set group of socket */
        if (chown(path, -1, ids.gid_vty))
        {
            zlog_err("vty_serv_un: could chown socket, %s",
                     safe_strerror(errno));
        }
    }
    
    vty_event(VTYSH_SERV, sock, NULL);
}

/* #define VTYSH_DEBUG 1 */

static int
vtysh_accept(struct thread *thread)
{
    int accept_sock;
    int sock;
    int client_len;
    int flags;
    struct sockaddr_un client;
    struct vty *vty;
    accept_sock = THREAD_FD(thread);
    vty_event(VTYSH_SERV, accept_sock, NULL);
    memset(&client, 0, sizeof(struct sockaddr_un));
    client_len = sizeof(struct sockaddr_un);
    sock = accept(accept_sock, (struct sockaddr *) &client, (socklen_t *) &client_len);
    
    if (sock < 0)
    {
        zlog_warn("can't accept vty socket : %s", safe_strerror(errno));
        return -1;
    }
    
    /* set to non-blocking*/
    if (((flags = fcntl(sock, F_GETFL)) == -1)
            || (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1))
    {
        zlog_warn("vtysh_accept: could not set vty socket to non-blocking,"
                  " %s, closing", safe_strerror(errno));
        close(sock);
        return -1;
    }
    
#ifdef VTYSH_DEBUG
    printf("VTY shell accept\n");
#endif /* VTYSH_DEBUG */
    vty = vty_new();
    vty->fd = sock;
    vty->type = VTY_SHELL_SERV;
    vty->node = VIEW_NODE;
    vty_event(VTYSH_READ, sock, vty);
    return 0;
}

static int
vtysh_read(struct thread *thread)
{
    int ret;
    int sock;
    int nbytes;
    struct vty *vty;
    unsigned char buf[VTY_READ_BUFSIZ];
    u_char header[4] = {0, 0, 0, 0};
    sock = THREAD_FD(thread);
    vty = THREAD_ARG(thread);
    vty->t_read = NULL;
    nbytes = read(sock, buf, VTY_READ_BUFSIZ);
    
    if (nbytes <= 0)
    {
        vty_close(vty);
#ifdef VTYSH_DEBUG
        printf("close vtysh\n");
#endif /* VTYSH_DEBUG */
        return 0;
    }
    
#ifdef VTYSH_DEBUG
    printf("line: %s\n", buf);
#endif /* VTYSH_DEBUG */
    vty_ensure(vty, nbytes);
    memcpy(vty->buf, buf, nbytes);
    /* Pass this line to parser. */
    ret = vty_execute(vty);
    vty_clear_buf(vty);
    /* Return result. */
#ifdef VTYSH_DEBUG
    printf("result: %d\n", ret);
    printf("vtysh node: %d\n", vty->node);
#endif /* VTYSH_DEBUG */
    header[3] = ret;
    buffer_write(vty->obuf, header, 4);
    
    if (!vty->t_write && buffer_flush_available(vty->obuf, vty->fd))
        vty_event(VTYSH_WRITE, vty->fd, vty);
        
    vty_event(VTYSH_READ, sock, vty);
    return 0;
}

static int
vtysh_write(struct thread *thread)
{
    struct vty *vty = THREAD_ARG(thread);
    vty->t_write = NULL;
    
    if (buffer_flush_available(vty->obuf, vty->fd))
        vty_event(VTYSH_WRITE, vty->fd, vty);
        
    return 0;
}

#endif /* VTYSH */

/* Determine address family to bind. */
void
vty_serv_sock(const char *addr, unsigned short port, const char *path)
{
    path = path;
    /* If port is set to 0, do not listen on TCP/IP at all! */
    if (port)
    {
#ifdef HAVE_IPV6
#ifdef NRL
        vty_serv_sock_family(addr, port, AF_INET);
        vty_serv_sock_family(addr, port, AF_INET6);
#else /* ! NRL */
        vty_serv_sock_addrinfo(addr, port);
#endif /* NRL*/
#else /* ! HAVE_IPV6 */
        vty_serv_sock_family(addr, port, AF_INET);
#endif /* HAVE_IPV6 */
    }
    
#ifdef VTYSH
    vty_serv_un(path);
#endif /* VTYSH */
}

/* Close vty interface. */
void
vty_close(struct vty *vty)
{
    int i;
    
    if (!vty->alive)
        return;
        
    vty->alive = 0;
#ifdef VTY_ACCESS_CHK /** Add by tsihang */
    
    if (vty->type == VTY_TERM)
    {
        char time_str[128];
        
        if (vty_counter > 0)
            vty_counter--;
            
        /* log */
        if (vty->address != NULL)
        {
            sprintf(time_str, "VTY closed by %s", vty->address);
            log_build(NULL, SYSLOG_DEFAULT_PRIORITY, time_str);
        }
    }
    
#endif
    
    /* Cancel threads.*/
    if (vty->t_read)
        thread_cancel(vty->t_read);
        
    if (vty->t_write)
        thread_cancel(vty->t_write);
        
    if (vty->t_timeout)
        thread_cancel(vty->t_timeout);
        
    /* Flush buffer. */
    if (! buffer_empty(vty->obuf))
        buffer_flush_all(vty->obuf, vty->fd);
        
    /* Free input buffer. */
    buffer_free(vty->obuf);
    
    /* Free SB buffer. */
    if (vty->sb_buffer)
        buffer_free(vty->sb_buffer);
        
    /* Free command history. */
    for (i = 0; i < VTY_MAXHIST; i++)
        if (vty->hist[i])
            XFREE(MTYPE_VTY_HIST, vty->hist[i]);
            
    /* Unset vector. */
    vector_unset(vtyvec, vty->fd);
    vty_lookup_vector_remove(vty);
    
    /* Close socket. */
    if (vty->fd > 0)
        close(vty->fd);
        
    if (vty->address)
        XFREE(0, vty->address);
        
    if (vty->buf)
        XFREE(MTYPE_VTY, vty->buf);
        
    /* Check configure. */
    vty_config_unlock(vty);
    /* OK free vty. */
    XFREE(MTYPE_VTY, vty);
}

/* When time out occur output message then close connection. */
static int
vty_timeout(struct thread *thread)
{
    struct vty *vty;
    vty = THREAD_ARG(thread);
    vty->t_timeout = NULL;
    //vty->v_timeout = 0;
    vty->v_timeout = vty_timeout_val;  /* add by tsihang for timeout reset , not close vty */
    /* Clear buffer*/
    buffer_reset(vty->obuf);
    vty_out(vty, "%sVty connection is timed out.%s", VTY_NEWLINE, VTY_NEWLINE);
#ifdef __EXTERNAL_LOGIN__
#ifdef __USER_AUTH__
    vty->node = AUTH_USER_NODE;
#else
    /* Close connection. */
    vty->status = VTY_CLOSE;
    vty_close(vty);
#endif
#else
    /* Close connection. */
    vty->status = VTY_CLOSE;
    vty_close(vty);
#endif
    return 0;
}

/* Read up configuration file from file_name. */
static void
vty_read_file(FILE *confp)
{
    int ret;
    struct vty *vty;
    vty = vty_new();
    vty->fd = 0;          /* stdout */
    vty->type = VTY_TERM;
    vty->node = CONFIG_NODE;
    /* Execute configuration file */
    ret = config_from_file(vty, confp);
    
    if (!((ret == CMD_SUCCESS) || (ret == CMD_ERR_NOTHING_TODO)))
    {
        switch (ret)
        {
            case CMD_ERR_AMBIGUOUS:
                fprintf(stderr, "Ambiguous command.\n");
                break;
                
            case CMD_ERR_NO_MATCH:
                fprintf(stderr, "There is no such command.\n");
                break;
        }
        
        fprintf(stderr, "Error occured during reading below line.\n%s\n",
                vty->buf);
        /** added by tsihang, 5 Nov, 2014. */
        /*vty_close(vty);
        exit(1);*/
    }
    
    vty_close(vty);
}

void vty_config_password()
{
    int ret;
    /* Configure passwords isn't by calling command it is done here */
#if 0
    struct vty *vty;
    vty = vty_new();
    vty->fd = 0;          /* stdout */
    vty->type = VTY_TERM;
    vty->node = CONFIG_NODE;
#endif
    ret = internal_config_password();
    ret = ret;
    /* Configure passwords isn't by calling command it is done here */
#if 0
    
    if (!((ret == CMD_SUCCESS) || (ret == CMD_ERR_NOTHING_TODO)))
    {
        switch (ret)
        {
            case CMD_ERR_AMBIGUOUS:
                fprintf(stderr, "Ambiguous command.\n");
                break;
                
            case CMD_ERR_NO_MATCH:
                fprintf(stderr, "There is no such command.\n");
                break;
        }
        
        fprintf(stderr, "Error occured during reading below line.\n%s\n",
                vty->buf);
        vty_close(vty);
        exit(1);
    }
    
    vty_close(vty);
#endif
}

#if 0
FILE *
vty_use_backup_config(char *fullpath)
{
    char *fullpath_sav, *fullpath_tmp;
    FILE *ret = NULL;
    struct stat buf;
    int tmp, sav;
    int c;
    char buffer[512];
#ifndef FILES_NOT_SUPPORTED
    fullpath_sav = malloc(strlen(fullpath) + strlen(CONF_BACKUP_EXT) + 1);
    strcpy(fullpath_sav, fullpath);
    strcat(fullpath_sav, CONF_BACKUP_EXT);
    
    if (stat(fullpath_sav, &buf) == -1)
    {
        free(fullpath_sav);
        return NULL;
    }
    
    fullpath_tmp = malloc(strlen(fullpath) + 8);
    sprintf(fullpath_tmp, "%s.XXXXXX", fullpath);
    /* Open file to configuration write. */
    tmp = mkstemp(fullpath_tmp);
    
    if (tmp < 0)
    {
        free(fullpath_sav);
        free(fullpath_tmp);
        return NULL;
    }
    
    sav = open(fullpath_sav, O_RDONLY);
    
    if (sav < 0)
    {
        unlink(fullpath_tmp);
        free(fullpath_sav);
        free(fullpath_tmp);
        return NULL;
    }
    
    while ((c = read(sav, buffer, 512)) > 0)
        write(tmp, buffer, c);
        
    close(sav);
    close(tmp);
    
    if (chmod(fullpath_tmp, CONFIGFILE_MASK) != 0)
    {
        unlink(fullpath_tmp);
        free(fullpath_sav);
        free(fullpath_tmp);
        return NULL;
    }
    
    if (link(fullpath_tmp, fullpath) == 0)
        ret = fopen(fullpath, "r");
        
    unlink(fullpath_tmp);
    free(fullpath_sav);
    free(fullpath_tmp);
    return ret;
#else
    return (EXIT_ERROR);
#endif
}
#endif

/** Read up configuration file from file_name. */
void
vty_read_config(char *config_file,
                char *config_default_dir)
{
#define MAXPATHLEN      4096
    char cwd[MAXPATHLEN];
    FILE *confp = NULL;
    char *fullpath;
    
    /* If -f flag specified. */
    if (config_file != NULL)
    {
        if (! IS_DIRECTORY_SEP(config_file[0]))
        {
            if (NULL == getcwd(cwd, MAXPATHLEN))
                fprintf(stderr, "getcwd error\n");
            fullpath = XMALLOC(MTYPE_TMP,
                               strlen(cwd) + strlen(config_file) + 2);
            sprintf(fullpath, "%s/%s", cwd, config_file);
        }
        
        else
        {
            fullpath = config_file;
        }
        
        confp = fopen(fullpath, "r");
        
        if (confp == NULL)
        {
            fprintf(stderr, "can't open configuration file [%s]\n",
                    config_file);
            /** exit(1); */ /** removed by tsihang, oct 31, 2014. */
            return;
        }
    }
    
    else
    {
#ifdef VTYSH
        int ret;
        struct stat conf_stat;
        
        /* !!!!PLEASE LEAVE!!!!
         * This is NEEDED for use with vtysh -b, or else you can get
         * a real configuration food fight with a lot garbage in the
         * merged configuration file it creates coming from the per
         * daemon configuration files.  This also allows the daemons
         * to start if there default configuration file is not
         * present or ignore them, as needed when using vtysh -b to
         * configure the daemons at boot - MAG
         */
        
        /* Stat for vtysh Zebra.conf, if found startup and wait for
         * boot configuration
         */
        
        if (strstr(config_default_dir, "vtysh") == NULL)
        {
            ret = stat(integrate_default, &conf_stat);
            
            if (ret >= 0)
                return;
        }
        
#endif /* VTYSH */
        confp = fopen(config_default_dir, "r");
        
        if (confp == NULL)
        {
            fprintf(stderr, "can't open configuration file [%s]\n",
                    config_file);
            exit(1);
        }
        
        else
            fullpath = config_default_dir;
    }
    
    vty_read_file(confp);
    fclose(confp);
    host_config_set(fullpath);
    return;
}

int
vty_config_lock(struct vty *vty)
{
    if (vty_config == 0)
    {
        vty->config = 1;
        vty_config = 1;
    }
    
    return vty->config;
}

int
vty_config_unlock(struct vty *vty)
{
    if (vty_config == 1 && vty->config == 1)
    {
        vty->config = 0;
        vty_config = 0;
    }
    
    return vty->config;
}

static void
vty_event(enum event event, int sock, struct vty *vty)
{
    struct thread *vty_serv_thread;
    
    switch (event)
    {
        case VTY_SERV:
            vty_serv_thread = thread_add_read(master, vty_accept, vty, sock);
            vector_set_index(Vvty_serv_thread, sock, vty_serv_thread);
            break;
#ifdef VTYSH
            
        case VTYSH_SERV:
            thread_add_read(master, vtysh_accept, vty, sock);
            break;
            
        case VTYSH_READ:
            vty->t_read = thread_add_read(master, vtysh_read, vty, sock);
            break;
            
        case VTYSH_WRITE:
            vty->t_write = thread_add_write(master, vtysh_write, vty, sock);
            break;
#endif /* VTYSH */
            
        case VTY_READ:
            if (vty->type == VTY_SHELL && vty->socket_close == 1)
            {
                vty->t_read = thread_add_read(master, vty_socket_read, vty, sock);
            }
            
            else
                vty->t_read = thread_add_read(master, vty_read, vty, sock);
                
            /* Time out treatment. */
            if (vty->v_timeout)
            {
                if (vty->t_timeout)
                    thread_cancel(vty->t_timeout);
                    
                vty->t_timeout =
                    thread_add_timer(master, vty_timeout, vty, vty->v_timeout);
            }
            
            break;
            
        case VTY_WRITE:
            if (! vty->t_write)
                vty->t_write = thread_add_write(master, vty_flush, vty, sock);
                
            break;
            
        case VTY_TIMEOUT_RESET:
            if (vty->t_timeout)
            {
                thread_cancel(vty->t_timeout);
                vty->t_timeout = NULL;
            }
            
            if (vty->v_timeout)
            {
                vty->t_timeout =
                    thread_add_timer(master, vty_timeout, vty, vty->v_timeout);
            }
            
            break;
    }
}

DEFUN(config_who,
      config_who_cmd,
      "who",
      "Display list of current CLI users\n"
      "显示当前命令行用户\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    unsigned int i;
    struct vty *v;
    
    for (i = 0; i < vty_lookup_vector_max(); i++)
        if ((v = vty_lookup_vector_slot(i)) != NULL)
        {
            vty_out(vty, "%svty[%d] connected from %s.%s",
                    v->config ? "*" : " ",
                    v->fd, v->address, VTY_NEWLINE);
        }
        
    return CMD_SUCCESS;
}

/* Move to vty configuration mode. */
DEFUN(line_vty,
      line_vty_cmd,
      "line vty",
      "Configure a terminal line\n"
      "进入虚拟终端配置模式\n"
      "Virtual terminal\n"
      "虚拟终端\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    vty->node = VTY_NODE;
    return CMD_SUCCESS;
}

/* Set time out value. */
int
exec_timeout(struct vty *vty, const char *min_str, const char *sec_str)
{
    unsigned long timeout = 0;
    
    /* min_str and sec_str are already checked by parser.  So it must be
       all digit string. */
    if (min_str)
    {
        timeout = strtol(min_str, NULL, 10);
        timeout *= 60;
    }
    
    if (sec_str)
        timeout += strtol(sec_str, NULL, 10);
        
    vty_timeout_val = timeout;
    vty->v_timeout = timeout;
    vty_event(VTY_TIMEOUT_RESET, 0, vty);
    return CMD_SUCCESS;
}


DEFUN(show_timeout,
      show_timeout_cmd,
      "show exec-timeout",
      SHOW_STR
      SHOW_CSTR
      "exec-timeout value\n"
      "定时断开的时间\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    long int time;
    long int minute;
    long int second;
    time = vty->v_timeout;
    second = time % 60;
    minute = (time - second) / 60;
    vty_out(vty, " Exec-timout is %ld minutes ", minute);
    
    if (second > 0)
        vty_out(vty, " %ld  seconds %s", second, VTY_NEWLINE);
        
    else
        vty_out(vty, VTY_NEWLINE);
        
    return CMD_SUCCESS;
}

DEFUN(exec_timeout_min,
      exec_timeout_min_cmd,
      "exec-timeout <0-35791>",
      "Set a timeout value\n"
      "启动与终端用户定时断开连接功能\n"
      "Timeout value in minutes\n"
      "设置定时断开时间(单位为分钟)\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    return exec_timeout(vty, argv[0], NULL);
}

DEFUN(exec_timeout_sec,
      exec_timeout_sec_cmd,
      "exec-timeout <0-35791> <0-2147483>",
      "Set a timeout value\n"
      "启动与终端用户定时断开连接功能\n"
      "Timeout in minutes\n"
      "设置定时断开时间(单位为分钟)\n"
      "Timeout in seconds\n"
      "设置定时断开时间(单位为秒)\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    return exec_timeout(vty, argv[0], argv[1]);
}

DEFUN(no_exec_timeout,
      no_exec_timeout_cmd,
      "no exec-timeout",
      NO_STR
      NO_CSTR
      "Set a timeout value\n"
      "启动与终端用户定时断开连接功能\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    return exec_timeout(vty, NULL, NULL);
}

/* Set vty access class. */
DEFUN(vty_access_class,
      vty_access_class_cmd,
      "access-class WORD",
      "Filter connections based on an IP access list\n"
      "IP access list\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    if (vty_accesslist_name)
        XFREE(MTYPE_VTY, vty_accesslist_name);
        
    vty_accesslist_name = XSTRDUP(MTYPE_VTY, argv[0]);
    return CMD_SUCCESS;
}

/* Clear vty access class. */
DEFUN(no_vty_access_class,
      no_vty_access_class_cmd,
      "no access-class [WORD]",
      NO_STR
      NO_CSTR
      "Filter connections based on an IP access list\n"
      "IP access list\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    if (! vty_accesslist_name || (argc && strcmp(vty_accesslist_name, argv[0])))
    {
        vty_out(vty, "Access-class is not currently applied to vty%s",
                VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    XFREE(MTYPE_VTY, vty_accesslist_name);
    vty_accesslist_name = NULL;
    return CMD_SUCCESS;
}

#ifndef HAVE_IPV6

DEFUN(ipv6_access_list,
      ipv6_access_list_cmd,
      "ipv6 WORD access-list X:X::X:X/M",
      IPV6_STR
      "IPv6 zebra access-list\n"
      "Add an access list entry\n"
      "Prefix to match. e.g. 3ffe:506::/32\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    vty_out(vty, "argc: %d access-list 2 number \r\n", argc);
    return CMD_SUCCESS;
}

#endif /* HAVE_IPV6 */

/* vty login. */
DEFUN(vty_login,
      vty_login_cmd,
      "login",
      "Enable password checking\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    no_password_check = 0;
    return CMD_SUCCESS;
}

DEFUN(no_vty_login,
      no_vty_login_cmd,
      "no login",
      NO_STR
      "Enable password checking\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    no_password_check = 1;
    return CMD_SUCCESS;
}

DEFUN(service_advanced_vty,
      service_advanced_vty_cmd,
      "service advanced-vty",
      "Set up miscellaneous service\n"
      "Enable advanced mode vty interface\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    host.advanced = 1;
    return CMD_SUCCESS;
}

DEFUN(no_service_advanced_vty,
      no_service_advanced_vty_cmd,
      "no service advanced-vty",
      NO_STR
      "Set up miscellaneous service\n"
      "Enable advanced mode vty interface\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    host.advanced = 0;
    return CMD_SUCCESS;
}


DEFUN(show_history,
      show_history_cmd,
      "show history",
      SHOW_STR
      SHOW_CSTR
      "Display the session command history\n"
      "显示会话历史\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int index;
    
    for (index = vty->hindex + 1; index != vty->hindex;)
    {
        if (index == VTY_MAXHIST)
        {
            index = 0;
            continue;
        }
        
        if (vty->hist[index] != NULL)
            vty_out(vty, "  %s%s", vty->hist[index], VTY_NEWLINE);
            
        index++;
    }
    
    return CMD_SUCCESS;
}

/* Display current configuration. */
int
vty_config_write(struct vty *vty)
{
    vty_out(vty, "line vty%s", VTY_NEWLINE);
    
    if (vty_accesslist_name)
        vty_out(vty, " access-class %s%s",
                vty_accesslist_name, VTY_NEWLINE);
                
    if (vty_ipv6_accesslist_name)
        vty_out(vty, " ipv6 access-class %s%s",
                vty_ipv6_accesslist_name, VTY_NEWLINE);
                
    /* exec-timeout */
    if (vty_timeout_val != VTY_TIMEOUT_DEFAULT)
        vty_out(vty, " exec-timeout %ld %ld%s",
                vty_timeout_val / 60,
                vty_timeout_val % 60, VTY_NEWLINE);
                
    /* login */
    if (no_password_check)
        vty_out(vty, " no login%s", VTY_NEWLINE);
        
    /** vty_out(vty, "!%s", VTY_NEWLINE); */
    return CMD_SUCCESS;
}

struct cmd_node vty_node =
{
    VTY_NODE,
    "%s(config-line)# ",
    1,
    NULL,
    NULL
};

/* Reset all VTY status. */
void
vty_reset()
{
    unsigned int i;
    struct vty *vty;
    struct thread *vty_serv_thread;
    
    for (i = 0; i < vector_max(vtyvec); i++)
        if ((vty = vector_slot(vtyvec, i)) != NULL)
        {
            buffer_reset(vty->obuf);
            vty->status = VTY_CLOSE;
            vty_close(vty);
        }
        
    for (i = 0; i < vector_max(Vvty_serv_thread); i++)
        if ((vty_serv_thread = vector_slot(Vvty_serv_thread, i)) != NULL)
        {
            thread_cancel(vty_serv_thread);
            vector_slot(Vvty_serv_thread, i) = NULL;
            close(i);
        }
        
    vty_timeout_val = VTY_TIMEOUT_DEFAULT;
    
    if (vty_accesslist_name)
    {
        XFREE(MTYPE_VTY, vty_accesslist_name);
        vty_accesslist_name = NULL;
    }
    
    if (vty_ipv6_accesslist_name)
    {
        XFREE(MTYPE_VTY, vty_ipv6_accesslist_name);
        vty_ipv6_accesslist_name = NULL;
    }
}

/* for ospf6d easy temprary reload function */
/* vty_reset + close accept socket */
void
vty_finish()
{
    unsigned int i;
    struct vty *vty;
    struct thread *vty_serv_thread;
    
    for (i = 0; i < vector_max(vtyvec); i++)
        if ((vty = vector_slot(vtyvec, i)) != NULL)
        {
            buffer_reset(vty->obuf);
            vty->status = VTY_CLOSE;
            vty_close(vty);
        }
        
    for (i = 0; i < vector_max(Vvty_serv_thread); i++)
        if ((vty_serv_thread = vector_slot(Vvty_serv_thread, i)) != NULL)
        {
            thread_cancel(vty_serv_thread);
            vector_slot(Vvty_serv_thread, i) = NULL;
            close(i);
        }
        
    vty_timeout_val = VTY_TIMEOUT_DEFAULT;
    
    if (vty_accesslist_name)
    {
        XFREE(MTYPE_VTY, vty_accesslist_name);
        vty_accesslist_name = NULL;
    }
    
    if (vty_ipv6_accesslist_name)
    {
        XFREE(MTYPE_VTY, vty_ipv6_accesslist_name);
        vty_ipv6_accesslist_name = NULL;
    }
}


int
vty_shell(struct vty *vty)
{
    return vty->type == VTY_SHELL ? 1 : 0;
}

int
vty_shell_serv(struct vty *vty)
{
    return vty->type == VTY_SHELL_SERV ? 1 : 0;
}


void
vty_init_vtysh()
{
    vtyvec = vector_init(VECTOR_MIN_SIZE);
}

/* called by main CLI thread */
void vty_kill_all()
{
    unsigned int i;
    struct vty *vty;
    kill_all_vty = 0;
    
    for (i = 0; i < vector_max(vtyvec); i++)
    {
        if ((vty = vector_slot(vtyvec, i)) != NULL)
        {
            vty->status = VTY_CLOSE;
            vty_close(vty);
        }
    }
    
    multi_tasking_mode = new_multi_tasking_mode;
}

void vty_set_multi_tasking_mode(char enable)
{
    new_multi_tasking_mode = enable;
    /* flag for main CLI thread to call vty_kill_all function*/
    kill_all_vty = 1;
}


void vty_get_multi_tasking_mode(char *enable)
{
    *enable = multi_tasking_mode;
}

/* Install vty's own commands like `who' command. */
void
vty_init(struct thread_master *master_thread)
{
    /* For further configuration read, preserve current directory. */
    vtyvec = vector_init(VECTOR_MIN_SIZE);
    master = master_thread;
    /* Initilize server thread vector. */
    Vvty_serv_thread = vector_init(VECTOR_MIN_SIZE);
    install_node(&vty_node, vty_config_write);
    install_element(CONFIG_NODE,     &line_vty_cmd);
    install_element(VIEW_NODE,       &config_who_cmd);
    install_element(VIEW_NODE,       &show_history_cmd);
    install_element(ENABLE_NODE,     &config_who_cmd);
    install_element(CONFIG_NODE,     &show_history_cmd);
    install_element(ENABLE_NODE,     &show_history_cmd);
    install_default(VTY_NODE);
    install_element(CONFIG_NODE,     &exec_timeout_min_cmd);
    install_element(CONFIG_NODE,     &exec_timeout_sec_cmd);
    install_element(CONFIG_NODE,     &no_exec_timeout_cmd);
    install_element(CONFIG_NODE,     &show_timeout_cmd);
    install_element(VTY_NODE,        &vty_access_class_cmd);
    install_element(VTY_NODE,        &no_vty_access_class_cmd);
    install_element(VTY_NODE,        &vty_login_cmd);
    install_element(VTY_NODE,        &no_vty_login_cmd);
}

