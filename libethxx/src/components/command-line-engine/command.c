/*
   $Id: command.c,v 1.31.2.3 2005/03/07 13:39:20 hasso Exp $

   Command interpreter routine for virtual terminal [aka TeletYpe]
   Copyright (C) 1997, 98, 99 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2, or (at your
option) any later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "zebra.h"

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "log.h"
#include "version.h"
#include "vector.h"

#include "vty.h"
#include "command.h"
#include "config.h"
#include "zserv.h"

#define NUMBER_OF_SYSLOG_SERVER 4

extern int g_ix_system_timezone;
extern unsigned char log_enable;
extern int syslog_enable;
extern unsigned long syslog_servaddr[NUMBER_OF_SYSLOG_SERVER];

#if 0
extern void system_config_write(struct vty *vty);
extern void interface_config_write(struct vty *vty);
extern void ifgrp_config_write(struct vty *vty);
extern void rule_fivetuple_config_write(struct vty *vty);
extern void rule_ipmask_config_write(struct vty *vty);
extern void rule_string_config_write(struct vty *vty);
extern void rule_singleip_config_write(struct vty *vty);
extern void session_config_write(struct vty *vty);
extern void rule_dll_config_write(struct vty *vty);

#ifdef BYPASS_ENA
extern void bypass_config_write(struct vty *vty);
extern void watchdog_config_write(struct vty *vty);
extern void opb_config_write(struct vty *vty);
#endif

extern void system_config_restore(struct vty *vty);
extern void interface_config_restore(struct vty *vty);
extern void ifgrp_config_restore(struct vty *vty);
extern void ipmask_config_restore(struct vty *vty);
extern void singleip_config_restore(struct vty *vty);
extern void rule_fivetuple_config_restore(struct vty *vty);
extern void string_config_restore(struct vty *vty);
extern void dll_config_restore(struct vty *vty);

#ifdef UMTS_ENA
extern void umts_config_write(struct vty *vty);
extern void umts_config_restore(struct vty *vty);
#endif
extern void snmp_config_write(struct vty *vty);
extern void snmp_config_restore(struct vty *vty);
extern void rule_vlan_config_write(struct vty *vty);
extern void rule_vlan_config_restore(struct vty *vty);

#ifdef BYPASS_ENA
extern void bypass_config_restore(struct vty *vty);
extern void watchdog_config_restore(struct vty *vty);
extern void opb_config_restore(struct vty *vty);
#endif
#endif

/* Command vector which includes some level of command lists. Normally
   each daemon maintains each own cmdvec. */
vector cmdvec;

/* Host information structure. */
struct host host;
/* Language define */
int language;

struct zebra_t cmdline_interface = {0, 0, 0};

/* Configuration strings taken from configuration file */
#define NUM_OF_CONFIG_STR  2
#define MAX_CONFIG_STR     50

#ifdef __USER_AUTH__
char *cmd_config_str[] =
{
    "root",
    "root",
    "enable"
};
#else
char *cmd_config_str[] =
{
    "root",
    "root"
};
#endif

#define CONFIGURATION_FILE_NAME                         "Cli.config"
#define CONFIGURATION_FILE_CLI_PASS_GROUP               "cli_password"
#define CONFIGURATION_FILE_ENABLE_PASS_GROUP            "enable_password"
#define CONFIGURATION_FILE_CLI_PASS_NAME                    "CLI_PASSWORD"
#define CONFIGURATION_FILE_ENABLE_PASS_NAME             "ENABLE_PASSWORD"
#define UNAME   "root"
#define PASSWD  "root"
#define EPASSWD "enable"

/* Default motd string. */
const char *default_motd =
    "\r\n\
Hello, this is " QUAGGA_PROGNAME " (version " QUAGGA_VERSION ").\r\n\
" QUAGGA_COPYRIGHT "\r\n\
\r\n";

#ifdef __USER_AUTH__
struct cmd_node auth_user_node =
{
    AUTH_USER_NODE,
    "Username: ",
    0,
    NULL,
    NULL
};
#endif

/* Standard command node structures. */
struct cmd_node auth_node =
{
    AUTH_NODE,
    "Password: ",
    0,
    NULL,
    NULL
};

struct cmd_node view_node =
{
    VIEW_NODE,
    "%s> ",
    0,
    NULL,
    NULL
};

struct cmd_node auth_enable_node =
{
    AUTH_ENABLE_NODE,
    "Password: ",
    0,
    NULL,
    NULL
};

struct cmd_node enable_node =
{
    ENABLE_NODE,
    "%s# ",
    0,
    NULL,
    NULL
};

struct cmd_node config_node =
{
    CONFIG_NODE,
    "%s(config)# ",
    1,
    NULL,
    NULL
};

struct cmd_node access_list_node =
{
    ACL_NODE,
    "%s(access-list)# ",
    1,
    NULL,
    NULL
};

#if 0
void common_config_write(struct vty *vty)
{
    return;
}
void common_config_restore(struct vty *vty)
{
#if 0
#define MAX_CMD_STRING_SIZE 511
    char vty_cmd_buffer[MAX_CMD_STRING_SIZE + 1] = {0};
    int vty_cmd_size = 0;
#define VTY_RUN_COMMAND(command)\
    {\
        vty_cmd_size    =   snprintf(vty_cmd_buffer, MAX_CMD_STRING_SIZE, "%s %s", command, VTY_NEWLINE);\
        run_command(vty, command, vty_cmd_size);\
        memset((void *)&vty_cmd_buffer[0], 0, MAX_CMD_STRING_SIZE);\
    }
#endif
    return;
}
#endif


/* Show version. */
DEFUN(show_version,
      show_version_cmd,
      "show cli version",
      SHOW_STR
      SHOW_CSTR
      "Displays version information\n"
      "显示版本信息\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty_out(vty, "Running CLI V%d.%02d %s%s",
            CLI_MAJOR_VER, CLI_MINOR_VER, VTY_NEWLINE, VTY_NEWLINE);
    return CMD_SUCCESS;
}

DEFUN(copy_file,
      copy_file_cmd,
      "copy-file tftp IP_OR_HOST port TFTP_PORT filename TFTP_FILENAME local_path_file_name LOCAL_PATH_FILE_NAME",
      "Download a file from a TFTP server to the host\n"
      "TFTP server configuration\n"
      "Setting record, IP address in dotted-decimal notation or in host-name format\n"
      "TFTP port number\n"
      "Setting TFTP port number\n"
      "The name for the file on the TFTP server\n"
      "Setting the file name on the server\n"
      "Path and name of the file on the host file system\n"
      "Setting path and file name on te host\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty; 
    return CMD_SUCCESS;
}

ALIAS(copy_file,
      copy_file_no_dest_cmd,
      "copy-file tftp IP_OR_HOST port TFTP_PORT filename TFTP_FILENAME",
      "Download a file from a TFTP server to the host\n"
      "TFTP server configuration\n"
      "Setting record, IP address in dotted-decimal notation or in host-name format\n"
      "TFTP port number\n"
      "Setting TFTP port number\n"
      "The name for the file on the TFTP server\n"
      "Setting the file name on the server\n")

/* Enable command */
DEFUN(enable,
      config_enable_cmd,
      "enable",
      "Turn on privileged mode command\n"
      "进入特权用户模式\n")
{
    self = self;
    argc = argc;
    argv = argv;

    /* If enable password is NULL, change to ENABLE_NODE */
    if ((host.enable == NULL && host.enable_encrypt == NULL) ||
            vty->type == VTY_SHELL_SERV)
        vty->node = ENABLE_NODE;
        
    else
        vty->node = AUTH_ENABLE_NODE;
        
    return CMD_SUCCESS;
}


/* Disable command */
DEFUN(disable,
      config_disable_cmd,
      "disable",
      "Turn off privileged mode command\n"
      "退出特权用户模式\n")
{
    self = self;
    argc = argc;
    argv = argv;
   
    if (vty->node == ENABLE_NODE)
        vty->node = VIEW_NODE;
        
    return CMD_SUCCESS;
}

/* Configration from terminal */
DEFUN(config,
      config_cmd,
      "configure",
      "CLI gloable configuration\n"
      "进入全局配置模式\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    /*  Changed in order to enable more than one session to enter config mode */
    /*  if (vty_config_lock (vty)) */
    vty->node = CONFIG_NODE;
    /*  else
        {
          vty_out (vty, "VTY configuration is locked by other VTY%s", VTY_NEWLINE);
          return CMD_WARNING;
        }  */
    return CMD_SUCCESS;
}


/* VTY interface password set. */
DEFUN(config_password, password_cmd,
      "password WORD",
      "Assign the terminal connection password\n"
      "The CLI password string\n")
{
    self = self;
 
    /* Argument check. */
    if (argc == 0)
    {
        vty_out(vty, "Please specify password.%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    if (!isalnum((int) *argv[0]))
    {
        vty_out(vty,
                "Please specify string starting with alphanumeric%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    if (host.password)
        XFREE(0, host.password);
        
    host.password = NULL;
    host.password = XSTRDUP(0, argv[0]);
    return CMD_SUCCESS;
}

/* VTY enable password set. */
DEFUN(config_enablepass_password, enablepass_password_cmd,
      "enablepass password WORD",
      "Modify enable password parameters\n"
      "Assign the privileged level password\n"
      "The 'enable' password string\n")
{
    self = self;

    /* Argument check. */
    if (argc == 0)
    {
        vty_out(vty, "Please specify password.%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    if (!isalnum((int) *argv[0]))
    {
        vty_out(vty,
                "Please specify string starting with alphanumeric%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    if (host.enable)
        XFREE(0, host.enable);
        
    host.enable = NULL;
    host.enable = XSTRDUP(0, argv[0]);
    return CMD_SUCCESS;
}

/* VTY enable password delete. */
DEFUN(no_config_enablepass_password, no_enablepass_password_cmd,
      "no enablepass password",
      NO_STR
      NO_CSTR
      "Modify enable password parameters\n"
      "Assign the privileged level password\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty; 
    
    if (host.enable)
        XFREE(0, host.enable);
        
    host.enable = NULL;
    
    if (host.enable_encrypt)
        XFREE(0, host.enable_encrypt);
        
    host.enable_encrypt = NULL;
    return CMD_SUCCESS;
}


DEFUN(set_system_sampling, set_system_sampling_cmd,
      "set system statistics-cycle (<10-3600000> | continuous)",
      SET_STR
      SET_CSTR
      SYSTEM_STR
      "Frequency of statistics sampling into statistics table\n"
      "Statistics sampling cycle, stated in Milliseconds\n"
      "Continuous statistics sampling")
{
    self = self;
    argc = argc;
    argv = argv;
  
    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "set_system_sampling_cmd %s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(no_system_sampling, no_system_sampling_cmd,
      "no system statistics-cycle",
      NO_STR
      SYSTEM_STR
      "Stop statistics sampling into statistics table\n")
{
    self = self;
    argc = argc;
    argv = argv;
 
    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "no_system_sampling_cmd %s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(set_system_monitoring, set_system_monitoring_cmd,
      "set system monitoring-cycle (<10-3600000> | continuous)",
      SET_STR
      SYSTEM_STR
      "Frequency of alarm thresholds checking\n"
      "Monitoring cycle, stated in Milliseconds\n"
      "Continuous monitoring")
{
    self = self;
    argc = argc;
    argv = argv;

    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "set_system_monitoring_cmd %s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(no_system_monitoring, no_system_monitoring_cmd,
      "no system monitoring-cycle",
      NO_STR
      SYSTEM_STR
      "stop of alarm thresholds checking\n")
{
    self = self;
    argc = argc;
    argv = argv;

    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "no_system_monitoring_cmd %s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(set_system_host_msg, set_system_host_msg_cmd,
      "set system host-msg-timeout <1-10000>",
      SET_STR
      SYSTEM_STR
      "Timeout waiting to a response from firmware after sending it a msg\n"
      "Timeout Value stated in milliseconds.\n")
{
    self = self;
    argc = argc;
    argv = argv;
 
    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, " set_system_host_msg_cmd%s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(set_system_reset_to, set_system_reset_to_cmd,
      "set system reset-timeouts <1-32767>",
      SET_STR
      SYSTEM_STR
      "Number of msgs sending timeouts after which is reset\n"
      "Number of msg Value\n")
{
    self = self;
    argc = argc;
    argv = argv;

    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "set_system_reset_to_cmd %s", VTY_NEWLINE);
    return (return_result);
}

DEFUN(no_system_reset_to, no_system_reset_to_cmd,
      "no system reset-timeouts",
      NO_STR
      SYSTEM_STR
      "Never reset the device\n")
{
    self = self;
    argc = argc;
    argv = argv;
 
    short int                 return_result = CMD_SUCCESS;
    vty_out(vty, "no_system_reset_to_cmd %s", VTY_NEWLINE);
    return (return_result);
}


/* This is called from main when a daemon is invoked with -v or --version. */
void
print_version(const char *progname)
{
    printf("%s version %s\n", progname, QUAGGA_VERSION);
    printf("%s\n", QUAGGA_COPYRIGHT);
}

/* Utility function to concatenate argv argument into a single string
   with inserting ' ' character between each argument.  */
char *
argv_concat(const char **argv, int argc, int shift)
{
    int i;
    int len;
    int index;
    char *str;
    str = NULL;
    index = 0;
    
    for (i = shift; i < argc; i++)
    {
        len = strlen(argv[i]);
        
        if (i == shift)
        {
            str = XSTRDUP(MTYPE_TMP, argv[i]);
            index = len;
        }
        
        else
        {
            str = XREALLOC(MTYPE_TMP, str, (index + len + 2));
            str[index++] = ' ';
            memcpy(str + index, argv[i], len);
            index += len;
            str[index] = '\0';
        }
    }
    
    return str;
}

/* Install top node of command vector. */
void
install_node(struct cmd_node *node,
             int (*func)(struct vty *))
{
    vector_set_index(cmdvec, node->node, node);
    node->func = func;
    node->cmd_vector = vector_init(VECTOR_MIN_SIZE);
}

/* Compare two command's string.  Used in sort_node (). */
static int
cmp_node(const void *p, const void *q)
{
    struct cmd_element *a = * (struct cmd_element **) p;
    struct cmd_element *b = * (struct cmd_element **) q;
    return strcmp(a->string, b->string);
}

static int
cmp_desc(const void *p, const void *q)
{
    struct desc *a = * (struct desc **) p;
    struct desc *b = * (struct desc **) q;
    return strcmp(a->cmd, b->cmd);
}

/* Sort each node's command element according to command string. */
void
sort_node()
{
    unsigned int i, j;
    struct cmd_node *cnode;
    vector descvec;
    struct cmd_element *cmd_element;
    
    for (i = 0; i < vector_max(cmdvec); i++)
        if ((cnode = vector_slot(cmdvec, i)) != NULL)
        {
            vector cmd_vector = cnode->cmd_vector;
            qsort(cmd_vector->index, cmd_vector->max, sizeof(void *), cmp_node);
            
            for (j = 0; j < vector_max(cmd_vector); j++)
                if ((cmd_element = vector_slot(cmd_vector, j)) != NULL)
                {
                    descvec = vector_slot(cmd_element->strvec,
                                          vector_max(cmd_element->strvec) - 1);
                    qsort(descvec->index, descvec->max, sizeof(void *), cmp_desc);
                }
        }
}

/* Breaking up string into each command piece. I assume given
   character is separated by a space character. Return value is a
   vector which includes char ** data element. */
vector
cmd_make_strvec(const char *string)
{
    const char *cp, *start;
    char *token;
    int strlen;
    vector strvec;
    
    if (string == NULL)
        return NULL;
        
    cp = string;
    
    /* Skip white spaces. */
    while (isspace((int) *cp) && *cp != '\0')
        cp++;
        
    /* Return if there is only white spaces */
    if (*cp == '\0')
        return NULL;
        
    if (*cp == '!' || *cp == '#')
        return NULL;
        
    /* Prepare return vector. */
    strvec = vector_init(VECTOR_MIN_SIZE);
    
    /* Copy each command piece and set into vector. */
    while (1)
    {
        start = cp;
        
        while (!(isspace((int) *cp) || *cp == '\r' || *cp == '\n') &&
                *cp != '\0')
            cp++;
            
        strlen = cp - start;
        token = XMALLOC(MTYPE_STRVEC, strlen + 1);
        memcpy(token, start, strlen);
        * (token + strlen) = '\0';
        vector_set(strvec, token);
        
        while ((isspace((int) *cp) || *cp == '\n' || *cp == '\r') &&
                *cp != '\0')
            cp++;
            
        if (*cp == '\0')
            return strvec;
    }
}

/* Free allocated string vector. */
void
cmd_free_strvec(vector v)
{
    unsigned int i;
    char *cp;
    
    if (!v)
        return;
        
    for (i = 0; i < vector_max(v); i++)
        if ((cp = vector_slot(v, i)) != NULL)
            XFREE(MTYPE_STRVEC, cp);
            
    vector_free(v);
}

/* Fetch next description.  Used in cmd_make_descvec(). */
static char *
cmd_desc_str(const char **string)
{
    const char *cp, *start;
    char *token;
    int strlen;
    cp = *string;
    
    if (cp == NULL)
        return NULL;
        
    /* Skip white spaces. */
    while (isspace((int) *cp) && *cp != '\0')
        cp++;
        
    /* Return if there is only white spaces */
    if (*cp == '\0')
        return NULL;
        
    start = cp;
    
    while (!(*cp == '\r' || *cp == '\n') && *cp != '\0')
        cp++;
        
    strlen = cp - start;
    token = XMALLOC(MTYPE_STRVEC, strlen + 1);
    memcpy(token, start, strlen);
    * (token + strlen) = '\0';
    *string = cp;
    return token;
}

/* Add by tsihang for Chinese parse */
char *
cmd_desc_cstr(const char **string)
{
    char *cp, *start, *token;
    int strlen;
    cp = (char *) *string;
    
    if (cp == NULL)
        return NULL;
        
    /* Skip white spaces. */
    while (isspace((int) * ((unsigned char *) cp)) && *cp != '\0')
        cp++;
        
    /* Return if there is only white spaces */
    if (*cp == '\0')
        return NULL;
        
    start = cp;
    
    while (!(*cp == '\r' || *cp == '\n') && *cp != '\0')
        cp++;
        
    strlen = cp - start;
    token = XMALLOC(MTYPE_STRVEC, strlen + 1);
    memcpy(token, start, strlen);
    * (token + strlen) = '\0';
    *string = cp;
    return token;
}


/* New string vector. */
static vector
cmd_make_descvec(const char *string, const char *descstr)
{
    int multiple = 0;
    const char *sp;
    char *token;
    int len;
    const char *cp;
    const char *dp;
    vector allvec;
    vector strvec = NULL;
    struct desc *desc;
    cp = string;
    dp = descstr;
    
    if (cp == NULL)
        return NULL;
        
    allvec = vector_init(VECTOR_MIN_SIZE);
    
    while (1)
    {
        while (isspace((int) *cp) && *cp != '\0')
            cp++;
            
        if (*cp == '(')
        {
            multiple = 1;
            cp++;
        }
        
        if (*cp == ')')
        {
            multiple = 0;
            cp++;
        }
        
        if (*cp == '|')
        {
            if (! multiple)
            {
                fprintf(stderr, "Command parse error!: %s\n", string);
                exit(1);
            }
            
            cp++;
        }
        
        while (isspace((int) *cp) && *cp != '\0')
            cp++;
            
        if (*cp == '(')
        {
            multiple = 1;
            cp++;
        }
        
        if (*cp == '\0')
            return allvec;
            
        sp = cp;
        
        while (!(isspace((int) *cp) || *cp == '\r' || *cp == '\n' || *cp == ')' || *cp == '|') && *cp != '\0')
            cp++;
            
        len = cp - sp;
        token = XMALLOC(MTYPE_STRVEC, len + 1);
        memcpy(token, sp, len);
        * (token + len) = '\0';
        desc = XCALLOC(MTYPE_DESC, sizeof(struct desc));
        desc->cmd = token;
        desc->str = cmd_desc_str(&dp);
        desc->cstr = cmd_desc_cstr(&dp);
        
        if (multiple)
        {
            if (multiple == 1)
            {
                strvec = vector_init(VECTOR_MIN_SIZE);
                vector_set(allvec, strvec);
            }
            
            multiple++;
        }
        
        else
        {
            strvec = vector_init(VECTOR_MIN_SIZE);
            vector_set(allvec, strvec);
        }
        
        vector_set(strvec, desc);
    }
}

/* Count mandantory string vector size.  This is to determine inputed
   command has enough command length. */
static int
cmd_cmdsize(vector strvec)
{
    unsigned int i;
    int size = 0;
    vector descvec;
    
    for (i = 0; i < vector_max(strvec); i++)
    {
        descvec = vector_slot(strvec, i);
        
        if (vector_max(descvec) == 1)
        {
            struct desc *desc = vector_slot(descvec, 0);
            
            if (desc->cmd == NULL || CMD_OPTION(desc->cmd))
                return size;
                
            else
                size++;
        }
        
        else
            size++;
    }
    
    return size;
}

/* Return prompt character of specified node. */
const char *
cmd_prompt(enum node_type node)
{
    struct cmd_node *cnode;
    cnode = vector_slot(cmdvec, node);
    return cnode->prompt;
}

/* Install a command into a node. */
void
install_element(enum node_type ntype, struct cmd_element *cmd)
{
    struct cmd_node *cnode;
    cnode = vector_slot(cmdvec, ntype);
    
    if (cnode == NULL)
    {
        fprintf(stderr, "Command node %d doesn't exist, please check it\n",
                ntype);
        exit(1);
    }
    
    vector_set(cnode->cmd_vector, cmd);
    cmd->strvec = cmd_make_descvec(cmd->string, cmd->doc);
    cmd->cmdsize = cmd_cmdsize(cmd->strvec);
    cmd->languageflag = 0;
}

/**
static unsigned char itoa64[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static void
to64(char *s, long v, int n)
{
    while (--n >= 0)
    {
        *s++ = itoa64[v & 0x3f];
        v >>= 6;
    }
}
*/

/* This function write configuration of this host. */
static int
config_write_host(struct vty *vty)
{
    vty = vty;
#if 0

    if (host.name)
        vty_out(vty, "hostname %s%s", host.name, VTY_NEWLINE);
        
    if (host.encrypt)
    {
        if (host.password_encrypt)
            vty_out(vty, "password 8 %s%s", host.password_encrypt, VTY_NEWLINE);
            
        if (host.enable_encrypt)
            vty_out(vty, "enable password 8 %s%s", host.enable_encrypt, VTY_NEWLINE);
    }
    
    else
    {
        if (host.password)
            vty_out(vty, "password %s%s", host.password, VTY_NEWLINE);
            
        if (host.enable)
            vty_out(vty, "enable password %s%s", host.enable, VTY_NEWLINE);
    }
    
    vty_out(vty, "configure %s", VTY_NEWLINE);
    
    if (host.advanced)
        vty_out(vty, "service advanced-vty%s", VTY_NEWLINE);
        
    if (host.encrypt)
        vty_out(vty, "service password-encryption%s", VTY_NEWLINE);
        
    if (host.lines >= 0)
        vty_out(vty, "service terminal-length %d%s", host.lines, VTY_NEWLINE);
        
#endif
#if 0
    interface_config_write(vty);
    ifgrp_config_write(vty);
    rule_fivetuple_config_write(vty);
    rule_ipmask_config_write(vty);
#ifdef UMTS_ENA 
    umts_config_write(vty);
#endif
    rule_string_config_write(vty);
    rule_singleip_config_write(vty);
    session_config_write(vty);
    snmp_config_write(vty);
    rule_vlan_config_write(vty);
    rule_ffwd_config_write(vty);
	rule_dll_config_write(vty);
	rule_url_config_write(vty);
	
#ifdef BYPASS_ENA
	bypass_config_write(vty);
	watchdog_config_write(vty);    
	opb_config_write(vty);
#endif

	/** System configuration */
    common_config_write(vty);
    system_config_write(vty);
#endif
    /** vty_out(vty, "!%s", VTY_NEWLINE); */
    return 1;
}

/* Utility function for getting command vector. */
static vector
cmd_node_vector(vector v, enum node_type ntype)
{
    struct cmd_node *cnode = vector_slot(v, ntype);
    return cnode->cmd_vector;
}


/* Completion match types. */
enum match_type
{
    no_match,
    extend_match,
    ipv4_prefix_match,
    ipv4_match,
    ipv6_prefix_match,
    ipv6_match,
    range_match,
    vararg_match,
    partly_match,
    exact_match
};

static enum match_type
cmd_ipv4_match(const char *str)
{
    const char *sp;
    int dots = 0, nums = 0;
    char buf[4];
    
    if (str == NULL)
        return partly_match;
        
    for (;;)
    {
        memset(buf, 0, sizeof(buf));
        sp = str;
        
        while (*str != '\0')
        {
            if (*str == '.')
            {
                if (dots >= 3)
                    return no_match;
                    
                if (* (str + 1) == '.')
                    return no_match;
                    
                if (* (str + 1) == '\0')
                    return partly_match;
                    
                dots++;
                break;
            }
            
            if (!isdigit((int) *str))
                return no_match;
                
            str++;
        }
        
        if (str - sp > 3)
            return no_match;
            
        strncpy(buf, sp, str - sp);
        
        if (atoi(buf) > 255)
            return no_match;
            
        nums++;
        
        if (*str == '\0')
            break;
            
        str++;
    }
    
    if (nums < 4)
        return partly_match;
        
    return exact_match;
}

static enum match_type
cmd_ipv4_prefix_match(const char *str)
{
    const char *sp;
    int dots = 0;
    char buf[4];
    
    if (str == NULL)
        return partly_match;
        
    for (;;)
    {
        memset(buf, 0, sizeof(buf));
        sp = str;
        
        while (*str != '\0' && *str != '/')
        {
            if (*str == '.')
            {
                if (dots == 3)
                    return no_match;
                    
                if (* (str + 1) == '.' || * (str + 1) == '/')
                    return no_match;
                    
                if (* (str + 1) == '\0')
                    return partly_match;
                    
                dots++;
                break;
            }
            
            if (!isdigit((int) *str))
                return no_match;
                
            str++;
        }
        
        if (str - sp > 3)
            return no_match;
            
        strncpy(buf, sp, str - sp);
        
        if (atoi(buf) > 255)
            return no_match;
            
        if (dots == 3)
        {
            if (*str == '/')
            {
                if (* (str + 1) == '\0')
                    return partly_match;
                    
                str++;
                break;
            }
            
            else if (*str == '\0')
                return partly_match;
        }
        
        if (*str == '\0')
            return partly_match;
            
        str++;
    }
    
    sp = str;
    
    while (*str != '\0')
    {
        if (!isdigit((int) *str))
            return no_match;
            
        str++;
    }
    
    if (atoi(sp) > 32)
        return no_match;
        
    return exact_match;
}

#define IPV6_ADDR_STR       "0123456789abcdefABCDEF:.%"
#define IPV6_PREFIX_STR     "0123456789abcdefABCDEF:.%/"
#define STATE_START     1
#define STATE_COLON     2
#define STATE_DOUBLE    3
#define STATE_ADDR      4
#define STATE_DOT       5
#define STATE_SLASH     6
#define STATE_MASK      7

#ifdef HAVE_IPV6

static enum match_type
cmd_ipv6_match(const char *str)
{
    int state = STATE_START;
    int colons = 0, nums = 0, double_colon = 0;
    const char *sp = NULL;
    struct sockaddr_in6 sin6_dummy;
    int ret;
    
    if (str == NULL)
        return partly_match;
        
    if (strspn(str, IPV6_ADDR_STR) != strlen(str))
        return no_match;
        
    /* use inet_pton that has a better support,
     * for example inet_pton can support the automatic addresses:
     *  ::1.2.3.4
     */
    ret = inet_pton(AF_INET6, str, &sin6_dummy.sin6_addr);
    
    if (ret == 1)
        return exact_match;
        
    while (*str != '\0')
    {
        switch (state)
        {
            case STATE_START:
                if (*str == ':')
                {
                    if (* (str + 1) != ':' && * (str + 1) != '\0')
                        return no_match;
                        
                    colons--;
                    state = STATE_COLON;
                }
                
                else
                {
                    sp = str;
                    state = STATE_ADDR;
                }
                
                continue;
                
            case STATE_COLON:
                colons++;
                
                if (* (str + 1) == ':')
                    state = STATE_DOUBLE;
                    
                else
                {
                    sp = str + 1;
                    state = STATE_ADDR;
                }
                
                break;
                
            case STATE_DOUBLE:
                if (double_colon)
                    return no_match;
                    
                if (* (str + 1) == ':')
                    return no_match;
                    
                else
                {
                    if (* (str + 1) != '\0')
                        colons++;
                        
                    sp = str + 1;
                    state = STATE_ADDR;
                }
                
                double_colon++;
                nums++;
                break;
                
            case STATE_ADDR:
                if (* (str + 1) == ':' || * (str + 1) == '\0')
                {
                    if (str - sp > 3)
                        return no_match;
                        
                    nums++;
                    state = STATE_COLON;
                }
                
                if (* (str + 1) == '.')
                    state = STATE_DOT;
                    
                break;
                
            case STATE_DOT:
                state = STATE_ADDR;
                break;
                
            default:
                break;
        }
        
        if (nums > 8)
            return no_match;
            
        if (colons > 7)
            return no_match;
            
        str++;
    }
    
#if 0
    
    if (nums < 11)
        return partly_match;
        
#endif /* 0 */
    return exact_match;
}

static enum match_type
cmd_ipv6_prefix_match(const char *str)
{
    int state = STATE_START;
    int colons = 0, nums = 0, double_colon = 0;
    int mask;
    const char *sp = NULL;
    char *endptr = NULL;
    
    if (str == NULL)
        return partly_match;
        
    if (strspn(str, IPV6_PREFIX_STR) != strlen(str))
        return no_match;
        
    while (*str != '\0' && state != STATE_MASK)
    {
        switch (state)
        {
            case STATE_START:
                if (*str == ':')
                {
                    if (* (str + 1) != ':' && * (str + 1) != '\0')
                        return no_match;
                        
                    colons--;
                    state = STATE_COLON;
                }
                
                else
                {
                    sp = str;
                    state = STATE_ADDR;
                }
                
                continue;
                
            case STATE_COLON:
                colons++;
                
                if (* (str + 1) == '/')
                    return no_match;
                    
                else if (* (str + 1) == ':')
                    state = STATE_DOUBLE;
                    
                else
                {
                    sp = str + 1;
                    state = STATE_ADDR;
                }
                
                break;
                
            case STATE_DOUBLE:
                if (double_colon)
                    return no_match;
                    
                if (* (str + 1) == ':')
                    return no_match;
                    
                else
                {
                    if (* (str + 1) != '\0' && * (str + 1) != '/')
                        colons++;
                        
                    sp = str + 1;
                    
                    if (* (str + 1) == '/')
                        state = STATE_SLASH;
                        
                    else
                        state = STATE_ADDR;
                }
                
                double_colon++;
                nums += 1;
                break;
                
            case STATE_ADDR:
                if (* (str + 1) == ':' || * (str + 1) == '.'
                        || * (str + 1) == '\0' || * (str + 1) == '/')
                {
                    if (str - sp > 3)
                        return no_match;
                        
                    for (; sp <= str; sp++)
                        if (*sp == '/')
                            return no_match;
                            
                    nums++;
                    
                    if (* (str + 1) == ':')
                        state = STATE_COLON;
                        
                    else if (* (str + 1) == '.')
                        state = STATE_DOT;
                        
                    else if (* (str + 1) == '/')
                        state = STATE_SLASH;
                }
                
                break;
                
            case STATE_DOT:
                state = STATE_ADDR;
                break;
                
            case STATE_SLASH:
                if (* (str + 1) == '\0')
                    return partly_match;
                    
                state = STATE_MASK;
                break;
                
            default:
                break;
        }
        
        if (nums > 11)
            return no_match;
            
        if (colons > 7)
            return no_match;
            
        str++;
    }
    
    if (state < STATE_MASK)
        return partly_match;
        
    mask = strtol(str, &endptr, 10);
    
    if (*endptr != '\0')
        return no_match;
        
    if (mask < 0 || mask > 128)
        return no_match;
        
    /* I don't know why mask < 13 makes command match partly.
       Forgive me to make this comments. I Want to set static default route
       because of lack of function to originate default in ospf6d; sorry
           yasu
      if (mask < 13)
        return partly_match;
    */
    return exact_match;
}

#endif /* HAVE_IPV6  */

#define DECIMAL_STRLEN_MAX 10

static int
cmd_range_match(const char *range, const char *str)
{
    char *p;
    char buf[DECIMAL_STRLEN_MAX + 1];
    char *endptr = NULL;
    unsigned long min, max, val;
    char      range_in_deciaml;
    
    if (str == NULL)
        return 1;
        
    val = strtoul(str, &endptr, 10);
    
    if (*endptr != '\0')
        return 0;
        
    range++;
    p = strchr(range, '-');
    
    if (p == NULL)
        return 0;
        
    if (p - range > DECIMAL_STRLEN_MAX)
        return 0;
        
    strncpy(buf, range, p - range);
    buf[p - range] = '\0';
    
    if (memcmp(range, "0x", 2) != 0)
    {
        range_in_deciaml = 1;
    }
    
    else
    {
        range_in_deciaml = 0;
    }
    
    if (range_in_deciaml == 1)
    {
        min = strtoul(buf, &endptr, 10);
        
        if (*endptr != '\0')
            return 0;
    }
    
    else
    {
        min = strtoul(buf, &endptr, 16);
        
        if (*endptr != '\0')
            return 0;
    }
    
    range = p + 1;
    p = strchr(range, '>');
    
    if (p == NULL)
        return 0;
        
    if (p - range > DECIMAL_STRLEN_MAX)
        return 0;
        
    strncpy(buf, range, p - range);
    buf[p - range] = '\0';
    
    if (range_in_deciaml == 1)
    {
        max = strtoul(buf, &endptr, 10);
        
        if (*endptr != '\0')
            return 0;
    }
    
    else
    {
        max = strtoul(buf, &endptr, 16);
        
        if (*endptr != '\0')
            return 0;
    }
    
    if (val < min || val > max)
        return 0;
        
    return 1;
}


#define HEXADECIMAL_STRLEN_MAX 12

static int
cmd_hex_range_match(const char *range, const char *str)
{
    char *p;
    char buf[HEXADECIMAL_STRLEN_MAX + 1];
    char *endptr = NULL;
    unsigned long min, max, val;
    char      range_in_deciaml;
    
    if (str == NULL)
        return 1;
        
    val = strtoul(str, &endptr, 16);
    
    if (*endptr != '\0')
        return 0;
        
    range++;
    p = strchr(range, '-');
    
    if (p == NULL)
        return 0;
        
    if (p - range > HEXADECIMAL_STRLEN_MAX)
        return 0;
        
    strncpy(buf, range, p - range);
    buf[p - range] = '\0';
    
    if (memcmp(range, "0x", 2) != 0)
    {
        range_in_deciaml = 1;
    }
    
    else
    {
        range_in_deciaml = 0;
    }
    
    if (range_in_deciaml == 1)
    {
        min = strtoul(buf, &endptr, 10);
        
        if (*endptr != '\0')
            return 0;
    }
    
    else
    {
        min = strtoul(buf, &endptr, 16);
        
        if (*endptr != '\0')
            return 0;
    }
    
    range = p + 1;
    p = strchr(range, '>');
    
    if (p == NULL)
        return 0;
        
    if (p - range > HEXADECIMAL_STRLEN_MAX)
        return 0;
        
    strncpy(buf, range, p - range);
    buf[p - range] = '\0';
    
    if (range_in_deciaml == 1)
    {
        max = strtoul(buf, &endptr, 10);
        
        if (*endptr != '\0')
            return 0;
    }
    
    else
    {
        max = strtoul(buf, &endptr, 16);
        
        if (*endptr != '\0')
            return 0;
    }
    
    if (val < min || val > max)
        return 0;
        
    return 1;
}



static int
cmd_range_general_match(const char *range, const char *str)
{
    if (str == NULL)
        return 1;
        
    if (memcmp(str, "0x", 2) != 0)
        return cmd_range_match(range, str);
        
    else
        return cmd_hex_range_match(range, str);
}

/* Make completion match and return match type flag. */
static enum match_type
cmd_filter_by_completion(char *command, vector v, unsigned int index)
{
    unsigned int i;
    const char *str;
    struct cmd_element *cmd_element;
    enum match_type match_type;
    vector descvec;
    struct desc *desc;
    match_type = no_match;
    
    /* If command and cmd_element string does not match set NULL to vector */
    for (i = 0; i < vector_max(v); i++)
        if ((cmd_element = vector_slot(v, i)) != NULL)
        {
            if (index >= vector_max(cmd_element->strvec))
                vector_slot(v, i) = NULL;
                
            else
            {
                unsigned int j;
                int matched = 0;
                descvec = vector_slot(cmd_element->strvec, index);
                
                for (j = 0; j < vector_max(descvec); j++)
                {
                    desc = vector_slot(descvec, j);
                    str = desc->cmd;
                    
                    if (CMD_VARARG(str))
                    {
                        if (match_type < vararg_match)
                            match_type = vararg_match;
                            
                        matched++;
                    }
                    
                    else if (CMD_RANGE_GENERAL(str))
                    {
                        if (cmd_range_general_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_HEX_RANGE(str))
                    {
                        if (cmd_hex_range_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_RANGE(str))
                    {
                        if (cmd_range_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
#ifdef HAVE_IPV6
                    
                    else if (CMD_IPV6(str))
                    {
                        if (cmd_ipv6_match(command))
                        {
                            if (match_type < ipv6_match)
                                match_type = ipv6_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_IPV6_PREFIX(str))
                    {
                        if (cmd_ipv6_prefix_match(command))
                        {
                            if (match_type < ipv6_prefix_match)
                                match_type = ipv6_prefix_match;
                                
                            matched++;
                        }
                    }
                    
#endif   /* HAVE_IPV6  */
                    
                    else if (CMD_IPV4(str))
                    {
                        if (cmd_ipv4_match(command))
                        {
                            if (match_type < ipv4_match)
                                match_type = ipv4_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_IPV4_PREFIX(str))
                    {
                        if (cmd_ipv4_prefix_match(command))
                        {
                            if (match_type < ipv4_prefix_match)
                                match_type = ipv4_prefix_match;
                                
                            matched++;
                        }
                    }
                    
                    else
                    
                        /* Check is this point's argument optional ? */
                        if (CMD_OPTION(str) || CMD_VARIABLE(str))
                        {
                            if (match_type < extend_match)
                                match_type = extend_match;
                                
                            matched++;
                        }
                        
                        else if (strncmp(command, str, strlen(command)) == 0)
                        {
                            if (strcmp(command, str) == 0)
                                match_type = exact_match;
                                
                            else
                            {
                                if (match_type < partly_match)
                                    match_type = partly_match;
                            }
                            
                            matched++;
                        }
                }
                
                if (! matched)
                    vector_slot(v, i) = NULL;
            }
        }
        
    return match_type;
}

/* Filter vector by command character with index. */
static enum match_type
cmd_filter_by_string(char *command, vector v, unsigned int index)
{
    unsigned int i;
    const char *str;
    struct cmd_element *cmd_element;
    enum match_type match_type;
    vector descvec;
    struct desc *desc;
    match_type = no_match;
    
    /* If command and cmd_element string does not match set NULL to vector */
    for (i = 0; i < vector_max(v); i++)
        if ((cmd_element = vector_slot(v, i)) != NULL)
        {
            /* If given index is bigger than max string vector of command,
                   set NULL*/
            if (index >= vector_max(cmd_element->strvec))
                vector_slot(v, i) = NULL;
                
            else
            {
                unsigned int j;
                int matched = 0;
                descvec = vector_slot(cmd_element->strvec, index);
                
                for (j = 0; j < vector_max(descvec); j++)
                {
                    desc = vector_slot(descvec, j);
                    str = desc->cmd;
                    
                    if (CMD_VARARG(str))
                    {
                        if (match_type < vararg_match)
                            match_type = vararg_match;
                            
                        matched++;
                    }
                    
                    else if (CMD_RANGE_GENERAL(str))
                    {
                        if (cmd_range_general_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_HEX_RANGE(str))
                    {
                        if (cmd_hex_range_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_RANGE(str))
                    {
                        if (cmd_range_match(str, command))
                        {
                            if (match_type < range_match)
                                match_type = range_match;
                                
                            matched++;
                        }
                    }
                    
#ifdef HAVE_IPV6
                    
                    else if (CMD_IPV6(str))
                    {
                        if (cmd_ipv6_match(command) == exact_match)
                        {
                            if (match_type < ipv6_match)
                                match_type = ipv6_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_IPV6_PREFIX(str))
                    {
                        if (cmd_ipv6_prefix_match(command) == exact_match)
                        {
                            if (match_type < ipv6_prefix_match)
                                match_type = ipv6_prefix_match;
                                
                            matched++;
                        }
                    }
                    
#endif /* HAVE_IPV6  */
                    
                    else if (CMD_IPV4(str))
                    {
                        if (cmd_ipv4_match(command) == exact_match)
                        {
                            if (match_type < ipv4_match)
                                match_type = ipv4_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_IPV4_PREFIX(str))
                    {
                        if (cmd_ipv4_prefix_match(command) == exact_match)
                        {
                            if (match_type < ipv4_prefix_match)
                                match_type = ipv4_prefix_match;
                                
                            matched++;
                        }
                    }
                    
                    else if (CMD_OPTION(str) || CMD_VARIABLE(str))
                    {
                        if (match_type < extend_match)
                            match_type = extend_match;
                            
                        matched++;
                    }
                    
                    else
                    {
                        if (strcmp(command, str) == 0)
                        {
                            match_type = exact_match;
                            matched++;
                        }
                    }
                }
                
                if (! matched)
                    vector_slot(v, i) = NULL;
            }
        }
        
    return match_type;
}

/* Check ambiguous match */
static int
is_cmd_ambiguous(char *command, vector v, int index, enum match_type type)
{
    unsigned int i;
    unsigned int j;
    const char *str = NULL;
    struct cmd_element *cmd_element;
    const char *matched = NULL;
    vector descvec;
    struct desc *desc;
    
    for (i = 0; i < vector_max(v); i++)
        if ((cmd_element = vector_slot(v, i)) != NULL)
        {
            int match = 0;
            descvec = vector_slot(cmd_element->strvec, index);
            
            for (j = 0; j < vector_max(descvec); j++)
            {
                enum match_type ret;
                desc = vector_slot(descvec, j);
                str = desc->cmd;
                
                switch (type)
                {
                    case exact_match:
                        if (!(CMD_OPTION(str) || CMD_VARIABLE(str))
                                && strcmp(command, str) == 0)
                            match++;
                            
                        break;
                        
                    case partly_match:
                        if (!(CMD_OPTION(str) || CMD_VARIABLE(str))
                                && strncmp(command, str, strlen(command)) == 0)
                        {
                            if (matched && strcmp(matched, str) != 0)
                                return 1; /* There is ambiguous match. */
                                
                            else
                                matched = str;
                                
                            match++;
                        }
                        
                        break;
                        
                    case range_match:
                        if ((cmd_range_match(str, command)) || (cmd_hex_range_match(str, command)))
                        {
                            if (matched && strcmp(matched, str) != 0)
                                return 1;
                                
                            else
                                matched = str;
                                
                            match++;
                        }
                        
                        break;
#ifdef HAVE_IPV6
                        
                    case ipv6_match:
                        if (CMD_IPV6(str))
                            match++;
                            
                        break;
                        
                    case ipv6_prefix_match:
                        if ((ret = cmd_ipv6_prefix_match(command)) != no_match)
                        {
                            if (ret == partly_match)
                                return 2; /* There is incomplete match. */
                                
                            match++;
                        }
                        
                        break;
#endif  /* HAVE_IPV6 */
                        
                    case ipv4_match:
                        if (CMD_IPV4(str))
                            match++;
                            
                        break;
                        
                    case ipv4_prefix_match:
                        if ((ret = cmd_ipv4_prefix_match(command)) != no_match)
                        {
                            if (ret == partly_match)
                                return 2; /* There is incomplete match. */
                                
                            match++;
                        }
                        
                        break;
                        
                    case extend_match:
                        if (CMD_OPTION(str) || CMD_VARIABLE(str))
                            match++;
                            
                        break;
                        
                    case no_match:
                    default:
                        break;
                }
            }
            
            if (! match)
                vector_slot(v, i) = NULL;
        }
        
    return 0;
}

/* If src matches dst return dst string, otherwise return NULL */
static const char *
cmd_entry_function(const char *src, const char *dst)
{
    /* Skip variable arguments. */
    if (CMD_OPTION(dst) || CMD_VARIABLE(dst) || CMD_VARARG(dst) ||
            CMD_IPV4(dst) || CMD_IPV4_PREFIX(dst) || CMD_RANGE(dst) || CMD_HEX_RANGE(dst) || CMD_RANGE_GENERAL(dst))
        return NULL;
        
    /* In case of 'command \t', given src is NULL string. */
    if (src == NULL)
        return dst;
        
    /* Matched with input string. */
    if (strncmp(src, dst, strlen(src)) == 0)
        return dst;
        
    return NULL;
}

/* If src matches dst return dst string, otherwise return NULL */
/* This version will return the dst string always if it is
   CMD_VARIABLE for '?' key processing */
static const char *
cmd_entry_function_desc(const char *src, const char *dst)
{
    if (CMD_VARARG(dst))
        return dst;
        
    if (CMD_RANGE_GENERAL(dst))
    {
        if (cmd_range_general_match(dst, src))
            return dst;
            
        else
            return NULL;
    }
    
    if (CMD_HEX_RANGE(dst))
    {
        if (cmd_hex_range_match(dst, src))
            return dst;
            
        else
            return NULL;
    }
    
    if (CMD_RANGE(dst))
    {
        if (cmd_range_match(dst, src))
            return dst;
            
        else
            return NULL;
    }
    
#ifdef HAVE_IPV6
    
    if (CMD_IPV6(dst))
    {
        if (cmd_ipv6_match(src))
            return dst;
            
        else
            return NULL;
    }
    
    if (CMD_IPV6_PREFIX(dst))
    {
        if (cmd_ipv6_prefix_match(src))
            return dst;
            
        else
            return NULL;
    }
    
#endif /* HAVE_IPV6 */
    
    if (CMD_IPV4(dst))
    {
        if (cmd_ipv4_match(src))
            return dst;
            
        else
            return NULL;
    }
    
    if (CMD_IPV4_PREFIX(dst))
    {
        if (cmd_ipv4_prefix_match(src))
            return dst;
            
        else
            return NULL;
    }
    
    /* Optional or variable commands always match on '?' */
    if (CMD_OPTION(dst) || CMD_VARIABLE(dst))
        return dst;
        
    /* In case of 'command \t', given src is NULL string. */
    if (src == NULL)
        return dst;
        
    if (strncmp(src, dst, strlen(src)) == 0)
        return dst;
        
    else
        return NULL;
}

/* Check same string element existence.  If it isn't there return
    1. */
static int
cmd_unique_string(vector v, const char *str)
{
    unsigned int i;
    char *match;
    
    for (i = 0; i < vector_max(v); i++)
        if ((match = vector_slot(v, i)) != NULL)
            if (strcmp(match, str) == 0)
                return 0;
                
    return 1;
}

/* Compare string to description vector.  If there is same string
   return 1 else return 0. */
static int
desc_unique_string(vector v, const char *str)
{
    unsigned int i;
    struct desc *desc;
    
    for (i = 0; i < vector_max(v); i++)
        if ((desc = vector_slot(v, i)) != NULL)
            if (strcmp(desc->cmd, str) == 0)
                return 1;
                
    return 0;
}

static int
cmd_try_do_shortcut(enum node_type node, char *first_word)
{
    if (first_word != NULL &&
            node != AUTH_NODE &&
            node != VIEW_NODE &&
            node != AUTH_ENABLE_NODE &&
            node != ENABLE_NODE &&
            0 == strcmp("do", first_word))
        return 1;
        
    return 0;
}

/* '?' describe command support. */
static vector
cmd_describe_command_real(vector vline, struct vty *vty, int *status)
{
    int i;
    vector cmd_vector;
#define INIT_MATCHVEC_SIZE 10
    vector matchvec;
    struct cmd_element *cmd_element;
    int index;
    int ret;
    enum match_type match;
    char *command;
    static struct desc desc_cr = { "<cr>", "", ""};
    /* Set index. */
    index = vector_max(vline) - 1;
    /* Make copy vector of current node's command vector. */
    cmd_vector = vector_copy(cmd_node_vector(cmdvec, vty->node));
    /* Prepare match vector */
    matchvec = vector_init(INIT_MATCHVEC_SIZE);
    
    /* Filter commands. */
    /* Only words precedes current word will be checked in this loop. */
    for (i = 0; i < index; i++)
    {
        command = vector_slot(vline, i);
        match = cmd_filter_by_completion(command, cmd_vector, i);
        
        if (match == vararg_match)
        {
            struct cmd_element *cmd_element;
            vector descvec;
            int j, k;
            
            for (j = 0; j < (int)(vector_max(cmd_vector)); j++)
                if ((cmd_element = vector_slot(cmd_vector, j)) != NULL)
                {
                    descvec = vector_slot(cmd_element->strvec,
                                          vector_max(cmd_element->strvec) - 1);
                                          
                    for (k = 0; k < (int) vector_max(descvec); k++)
                    {
                        struct desc *desc = vector_slot(descvec, k);
                        vector_set(matchvec, desc);
                    }
                }
                
            vector_set(matchvec, &desc_cr);
            vector_free(cmd_vector);
            return matchvec;
        }
        
        if ((ret = is_cmd_ambiguous(command, cmd_vector, i, match)) == 1)
        {
            vector_free(cmd_vector);
            *status = CMD_ERR_AMBIGUOUS;
            return NULL;
        }
        
        else if (ret == 2)
        {
            vector_free(cmd_vector);
            *status = CMD_ERR_NO_MATCH;
            return NULL;
        }
    }
    
    /* Prepare match vector */
    /*  matchvec = vector_init (INIT_MATCHVEC_SIZE); */
    /* Make sure that cmd_vector is filtered based on current word */
    command = vector_slot(vline, index);
    
    if (command)
        match = cmd_filter_by_completion(command, cmd_vector, index);
        
    /* Make description vector. */
    for (i = 0; i < (int)(vector_max(cmd_vector)); i++)
        if ((cmd_element = vector_slot(cmd_vector, i)) != NULL)
        {
            const char *string = NULL;
            vector strvec = cmd_element->strvec;
            
            /* if command is NULL, index may be equal to vector_max */
            if (command && index >= (int)(vector_max(strvec)))
                vector_slot(cmd_vector, i) = NULL;
                
            else
            {
                /* Check if command is completed. */
                if (command == NULL && index == (int)(vector_max(strvec)))
                {
                    string = "<cr>";
                    
                    if (! desc_unique_string(matchvec, string))
                        vector_set(matchvec, &desc_cr);
                }
                
                else
                {
                    unsigned int j;
                    vector descvec = vector_slot(strvec, index);
                    struct desc *desc;
                    
                    for (j = 0; j < vector_max(descvec); j++)
                    {
                        desc = vector_slot(descvec, j);
                        string = cmd_entry_function_desc(command, desc->cmd);
                        
                        if (string)
                        {
                            /* Uniqueness check */
                            if (! desc_unique_string(matchvec, string))
                                vector_set(matchvec, desc);
                        }
                    }
                }
            }
        }
        
    vector_free(cmd_vector);
    
    if (vector_slot(matchvec, 0) == NULL)
    {
        vector_free(matchvec);
        *status = CMD_ERR_NO_MATCH;
    }
    
    else
        *status = CMD_SUCCESS;
        
    return matchvec;
}

vector
cmd_describe_command(vector vline, struct vty *vty, int *status)
{
#if 0 /* - remove the "do" command */
    vector ret;
    
    if (cmd_try_do_shortcut(vty->node, vector_slot(vline, 0)))
    {
        enum node_type onode;
        vector shifted_vline;
        unsigned int index;
        int num;
        onode = vty->node;
        vty->node = ENABLE_NODE;
        /* We can try it on enable node, cos' the vty is authenticated */
        shifted_vline = vector_init(vector_count(vline));
        /* use memcpy? */
        num = vector_max(vline);
        
        for (index = 1; index < vector_max(vline); index++)
        {
            vector_set_index(shifted_vline, index - 1, vector_lookup(vline, index));
        }
        
        ret = cmd_describe_command_real(shifted_vline, vty, status);
        vector_free(shifted_vline);
        vty->node = onode;
        return ret;
    }
    
#endif
    return cmd_describe_command_real(vline, vty, status);
}


/* Check LCD of matched command. */
static int
cmd_lcd(char **matched)
{
    int i;
    int j;
    int lcd = -1;
    char *s1, *s2;
    char c1, c2;
    
    if (matched[0] == NULL || matched[1] == NULL)
        return 0;
        
    for (i = 1; matched[i] != NULL; i++)
    {
        s1 = matched[i - 1];
        s2 = matched[i];
        
        for (j = 0; (c1 = s1[j]) && (c2 = s2[j]); j++)
            if (c1 != c2)
                break;
                
        if (lcd < 0)
            lcd = j;
            
        else
        {
            if (lcd > j)
                lcd = j;
        }
    }
    
    return lcd;
}

/* Command line completion support. */
static char **
cmd_complete_command_real(vector vline, struct vty *vty, int *status)
{
    int i;
    vector cmd_vector = vector_copy(cmd_node_vector(cmdvec, vty->node));
#define INIT_MATCHVEC_SIZE 10
    vector matchvec;
    struct cmd_element *cmd_element;
    int index = vector_max(vline) - 1;
    char **match_str;
    struct desc *desc;
    vector descvec;
    char *command;
    int lcd;
    
    /* First, filter by preceeding command string */
    for (i = 0; i < index; i++)
    {
        enum match_type match;
        int ret;
        command = vector_slot(vline, i);
        /* First try completion match, if there is exactly match return 1 */
        match = cmd_filter_by_completion(command, cmd_vector, i);
        
        /* If there is exact match then filter ambiguous match else check
        ambiguousness. */
        if ((ret = is_cmd_ambiguous(command, cmd_vector, i, match)) == 1)
        {
            vector_free(cmd_vector);
            *status = CMD_ERR_AMBIGUOUS;
            return NULL;
        }
        
        /*
        else if (ret == 2)
        {
          vector_free (cmd_vector);
          *status = CMD_ERR_NO_MATCH;
          return NULL;
        }
             */
    }
    
    /* Prepare match vector. */
    matchvec = vector_init(INIT_MATCHVEC_SIZE);
    
    /* Now we got into completion */
    for (i = 0; i < (int)(vector_max(cmd_vector)); i++)
        if ((cmd_element = vector_slot(cmd_vector, i)) != NULL)
        {
            const char *string;
            vector strvec = cmd_element->strvec;
            
            /* Check field length */
            if (index >= (int)(vector_max(strvec)))
                vector_slot(cmd_vector, i) = NULL;
                
            else
            {
                unsigned int j;
                descvec = vector_slot(strvec, index);
                
                for (j = 0; j < vector_max(descvec); j++)
                {
                    desc = vector_slot(descvec, j);
                    
                    if ((string = cmd_entry_function(vector_slot(vline, index),
                                                     desc->cmd)))
                        if (cmd_unique_string(matchvec, string))
                            vector_set(matchvec, XSTRDUP(MTYPE_TMP, string));
                }
            }
        }
        
    /* We don't need cmd_vector any more. */
    vector_free(cmd_vector);
    
    /* No matched command */
    if (vector_slot(matchvec, 0) == NULL)
    {
        vector_free(matchvec);
        
        /* In case of 'command \t' pattern.  Do you need '?' command at
           the end of the line. */
        if (vector_slot(vline, index) == '\0')
            *status = CMD_ERR_NOTHING_TODO;
            
        else
            *status = CMD_ERR_NO_MATCH;
            
        return NULL;
    }
    
    /* Only one matched */
    if (vector_slot(matchvec, 1) == NULL)
    {
        match_str = (char **) matchvec->index;
        vector_only_wrapper_free(matchvec);
        *status = CMD_COMPLETE_FULL_MATCH;
        return match_str;
    }
    
    /* Make it sure last element is NULL. */
    vector_set(matchvec, NULL);
    
    /* Check LCD of matched strings. */
    if (vector_slot(vline, index) != NULL)
    {
        lcd = cmd_lcd((char **) matchvec->index);
        
        if (lcd)
        {
            int len = strlen(vector_slot(vline, index));
            
            if (len < lcd)
            {
                char *lcdstr;
                lcdstr = XMALLOC(MTYPE_TMP, lcd + 1);
                memcpy(lcdstr, matchvec->index[0], lcd);
                lcdstr[lcd] = '\0';
                
                /* match_str = (char **) &lcdstr; */
                
                /* Free matchvec. */
                for (i = 0; i < (int)(vector_max(matchvec)); i++)
                {
                    if (vector_slot(matchvec, i))
                        XFREE(MTYPE_TMP, vector_slot(matchvec, i));
                }
                
                vector_free(matchvec);
                /* Make new matchvec. */
                matchvec = vector_init(INIT_MATCHVEC_SIZE);
                vector_set(matchvec, lcdstr);
                match_str = (char **) matchvec->index;
                vector_only_wrapper_free(matchvec);
                *status = CMD_COMPLETE_MATCH;
                return match_str;
            }
        }
    }
    
    match_str = (char **) matchvec->index;
    vector_only_wrapper_free(matchvec);
    *status = CMD_COMPLETE_LIST_MATCH;
    return match_str;
}

char **
cmd_complete_command(vector vline, struct vty *vty, int *status)
{
#if 0
    char **ret;
    
    if (cmd_try_do_shortcut(vty->node, vector_slot(vline, 0)))
    {
        enum node_type onode;
        vector shifted_vline;
        unsigned int index;
        int num;
        onode = vty->node;
        vty->node = ENABLE_NODE;
        /* We can try it on enable node, cos' the vty is authenticated */
        shifted_vline = vector_init(vector_count(vline));
        /* use memcpy? */
        num = vector_max(vline);
        
        for (index = 1; index < vector_max(vline); index++)
        {
            vector_set_index(shifted_vline, index - 1, vector_lookup(vline, index));
        }
        
        ret = cmd_complete_command_real(shifted_vline, vty, status);
        vector_free(shifted_vline);
        vty->node = onode;
        return ret;
    }
    
#endif
    return cmd_complete_command_real(vline, vty, status);
}

/* return parent node */
/* MUST eventually converge on CONFIG_NODE */
static enum node_type
node_parent(enum node_type node)
{
    enum node_type ret;
    assert(node > CONFIG_NODE);
    
    switch (node)
    {
        default:
            ret = CONFIG_NODE;
    }
    
    return ret;
}

/* Execute command by argument vline vector. */
static int
cmd_execute_command_real(vector vline, struct vty *vty, struct cmd_element **cmd)
{
    unsigned int i;
    unsigned int index;
    vector cmd_vector;
    struct cmd_element *cmd_element;
    struct cmd_element *matched_element;
    unsigned int matched_count, incomplete_count;
    int argc;
    const char *argv[CMD_ARGC_MAX];
    enum match_type match = 0;
    int varflag;
    char *command;
    /* Make copy of command elements. */
    cmd_vector = vector_copy(cmd_node_vector(cmdvec, vty->node));
    
    for (index = 0; index < vector_max(vline); index++)
    {
        int ret;
        command = vector_slot(vline, index);
        match = cmd_filter_by_completion(command, cmd_vector, index);
        
        if (match == vararg_match)
            break;
            
        ret = is_cmd_ambiguous(command, cmd_vector, index, match);
        
        if (ret == 1)
        {
            vector_free(cmd_vector);
            return CMD_ERR_AMBIGUOUS;
        }
        
        else if (ret == 2)
        {
            vector_free(cmd_vector);
            return CMD_ERR_NO_MATCH;
        }
    }
    
    /* Check matched count. */
    matched_element = NULL;
    matched_count = 0;
    incomplete_count = 0;
    
    for (i = 0; i < vector_max(cmd_vector); i++)
        if (vector_slot(cmd_vector, i) != NULL)
        {
            cmd_element = vector_slot(cmd_vector, i);
            
            if (match == vararg_match || index >= cmd_element->cmdsize)
            {
                matched_element = cmd_element;
#if 0
                printf("DEBUG: %s\n", cmd_element->string);
#endif
                matched_count++;
            }
            
            else
            {
                incomplete_count++;
            }
        }
        
    /* Finish of using cmd_vector. */
    vector_free(cmd_vector);
    
    /* To execute command, matched_count must be 1.*/
    if (matched_count == 0)
    {
        if (incomplete_count)
            return CMD_ERR_INCOMPLETE;
            
        else
            return CMD_ERR_NO_MATCH;
    }
    
    if (matched_count > 1)
        return CMD_ERR_AMBIGUOUS;
        
    /* Argument treatment */
    varflag = 0;
    argc = 0;
    
    for (i = 0; i < vector_max(vline); i++)
    {
        if (varflag)
            argv[argc++] = vector_slot(vline, i);
            
        else
        {
            vector descvec = vector_slot(matched_element->strvec, i);
            
            if (vector_max(descvec) == 1)
            {
                struct desc *desc = vector_slot(descvec, 0);
                
                if (CMD_VARARG(desc->cmd))
                    varflag = 1;
                    
                if (varflag || CMD_VARIABLE(desc->cmd) || CMD_OPTION(desc->cmd))
                    argv[argc++] = vector_slot(vline, i);
            }
            
            else
                argv[argc++] = vector_slot(vline, i);
        }
        
        if (argc >= CMD_ARGC_MAX)
            return CMD_ERR_EXEED_ARGC_MAX;
    }
    
    /* For vtysh execution. */
    if (cmd)
        *cmd = matched_element;
        
    if (matched_element->daemon)
        return CMD_SUCCESS_DAEMON;
        
    /* Execute matched command. */
    return (*matched_element->func)(matched_element, vty, argc, argv);
}


int
cmd_execute_command(vector vline, struct vty *vty, struct cmd_element **cmd,
                    int vtysh)
{
    int ret, saved_ret, tried = 0;
    enum node_type onode, try_node;
    onode = try_node = vty->node;
    
    if (cmd_try_do_shortcut(vty->node, vector_slot(vline, 0)))
    {
        vector shifted_vline;
        unsigned int index;
        vty->node = ENABLE_NODE;
        /* We can try it on enable node, cos' the vty is authenticated */
        shifted_vline = vector_init(vector_count(vline));
        
        /* use memcpy? */
        for (index = 1; index < vector_max(vline); index++)
        {
            vector_set_index(shifted_vline, index - 1, vector_lookup(vline, index));
        }
        
        ret = cmd_execute_command_real(shifted_vline, vty, cmd);
        vector_free(shifted_vline);
        vty->node = onode;
        return ret;
    }
    
    saved_ret = ret = cmd_execute_command_real(vline, vty, cmd);
    
    if (vtysh)
        return saved_ret;
        
    /* This assumes all nodes above CONFIG_NODE are childs of CONFIG_NODE */
    while (ret != CMD_SUCCESS && ret != CMD_WARNING
            && vty->node > CONFIG_NODE)
    {
        try_node = node_parent(try_node);
        vty->node = try_node;
        ret = cmd_execute_command_real(vline, vty, cmd);
        tried = 1;
        
        if (ret == CMD_SUCCESS || ret == CMD_WARNING)
        {
            /* succesfull command, leave the node as is */
            return ret;
        }
    }
    
    /* no command succeeded, reset the vty to the original node and
       return the error for this node */
    if (tried)
        vty->node = onode;
        
    return saved_ret;
}

/* Execute command by argument readline. */
int
cmd_execute_command_strict(vector vline, struct vty *vty,
                           struct cmd_element **cmd)
{
    unsigned int i;
    unsigned int index;
    vector cmd_vector;
    struct cmd_element *cmd_element;
    struct cmd_element *matched_element;
    unsigned int matched_count, incomplete_count;
    int argc;
    const char *argv[CMD_ARGC_MAX];
    int varflag;
    enum match_type match = 0;
    char *command;
    /* Make copy of command element */
    cmd_vector = vector_copy(cmd_node_vector(cmdvec, vty->node));
    
    for (index = 0; index < vector_max(vline); index++)
    {
        int ret;
        command = vector_slot(vline, index);
        match = cmd_filter_by_string(vector_slot(vline, index),
                                     cmd_vector, index);
                                     
        /* If command meets '.VARARG' then finish matching. */
        if (match == vararg_match)
            break;
            
        ret = is_cmd_ambiguous(command, cmd_vector, index, match);
        
        if (ret == 1)
        {
            vector_free(cmd_vector);
            return CMD_ERR_AMBIGUOUS;
        }
        
        if (ret == 2)
        {
            vector_free(cmd_vector);
            return CMD_ERR_NO_MATCH;
        }
    }
    
    /* Check matched count. */
    matched_element = NULL;
    matched_count = 0;
    incomplete_count = 0;
    
    for (i = 0; i < vector_max(cmd_vector); i++)
        if (vector_slot(cmd_vector, i) != NULL)
        {
            cmd_element = vector_slot(cmd_vector, i);
            
            if (match == vararg_match || index >= cmd_element->cmdsize)
            {
                matched_element = cmd_element;
                matched_count++;
            }
            
            else
                incomplete_count++;
        }
        
    /* Finish of using cmd_vector. */
    vector_free(cmd_vector);
    
    /* To execute command, matched_count must be 1.*/
    if (matched_count == 0)
    {
        if (incomplete_count)
            return CMD_ERR_INCOMPLETE;
            
        else
            return CMD_ERR_NO_MATCH;
    }
    
    if (matched_count > 1)
        return CMD_ERR_AMBIGUOUS;
        
    /* Argument treatment */
    varflag = 0;
    argc = 0;
    
    for (i = 0; i < vector_max(vline); i++)
    {
        if (varflag)
            argv[argc++] = vector_slot(vline, i);
            
        else
        {
            vector descvec = vector_slot(matched_element->strvec, i);
            
            if (vector_max(descvec) == 1)
            {
                struct desc *desc = vector_slot(descvec, 0);
                
                if (CMD_VARARG(desc->cmd))
                    varflag = 1;
                    
                if (varflag || CMD_VARIABLE(desc->cmd) || CMD_OPTION(desc->cmd))
                    argv[argc++] = vector_slot(vline, i);
            }
            
            else
                argv[argc++] = vector_slot(vline, i);
        }
        
        if (argc >= CMD_ARGC_MAX)
            return CMD_ERR_EXEED_ARGC_MAX;
    }
    
    /* For vtysh execution. */
    if (cmd)
        *cmd = matched_element;
        
    if (matched_element->daemon)
        return CMD_SUCCESS_DAEMON;
        
    /* Now execute matched command */
    return (*matched_element->func)(matched_element, vty, argc, argv);
}

/** Added by tsihang, 4 Nov, 2014. */
int
run_command(struct vty *vty, char *command_str, uint32_t command_str_len)
{
    int ret;
    vector vline;
    command_str_len = command_str_len;
    
    if (command_str)
    {
        vline = cmd_make_strvec(command_str);
        
        /* In case of comment line */
        if (vline == NULL)
            return CMD_SUCCESS;
            
        /* Execute configuration command : this is strict match */
        ret = cmd_execute_command_strict(vline, vty, NULL);
        
        /* Try again with setting node to CONFIG_NODE */
        while (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO && vty->node != CONFIG_NODE)
        {
            vty->node = node_parent(vty->node);
            ret = cmd_execute_command_strict(vline, vty, NULL);
        }
        
        cmd_free_strvec(vline);
        
        if (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO)
            return ret;
    }
    
    return CMD_SUCCESS;
}

/* Configration make from file. */
int
config_from_file(struct vty *vty, FILE *fp)
{
    int ret = 0;
    vector vline;
    vty->restore_flag = 1;
    
    while (fgets(vty->buf, VTY_BUFSIZ, fp))
    {
        vline = cmd_make_strvec(vty->buf);
        
        /* In case of comment line */
        if (vline == NULL)
            continue;
            
        /* Execute configuration command : this is strict match */
        ret = cmd_execute_command_strict(vline, vty, NULL);
        
        /* Try again with setting node to CONFIG_NODE */
        while (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO && vty->node != CONFIG_NODE)
        {
            vty->node = node_parent(vty->node);
            ret = cmd_execute_command_strict(vline, vty, NULL);
        }
        
        cmd_free_strvec(vline);
        
        if (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO)
            break;
    }
    
    vty->restore_flag = 0;
    return ret;
}

int external_config_password()
{
#if 0
    short int  parameter_read_result;
    char       pass_str[30];
    char       enable_pass_str[30];
    char       password_cmd_str[NUM_OF_CONFIG_STR][MAX_CONFIG_STR] = {"password ", "enablepass password "};
    /* changed by tsihang, Semptian. Configure passwords isn't by calling command it is done here */
    READ_CLI_CONFIGURATION_PARAMETER(CONFIGURATION_FILE_CLI_PASS_GROUP, CONFIGURATION_FILE_CLI_PASS_NAME, pass_str)
    
    if (parameter_read_result == EXIT_OK)
    {
        if (host.password)
            XFREE(0, host.password);
            
        host.password = NULL;
        
        if (strcmp(pass_str, "") != 0)
            host.password = XSTRDUP(0, pass_str);
    }
    
    else
        return (parameter_read_result);
        
    READ_CLI_CONFIGURATION_PARAMETER(CONFIGURATION_FILE_ENABLE_PASS_GROUP, CONFIGURATION_FILE_ENABLE_PASS_NAME, enable_pass_str)
    
    if (parameter_read_result == EXIT_OK)
    {
        if (host.enable)
            XFREE(0, host.enable);
            
        host.enable = NULL;
        
        if (strcmp(enable_pass_str, "") != 0)
            host.enable = XSTRDUP(0, enable_pass_str);
    }
    
    else
        return (parameter_read_result);
        
#if 0
    strcat(password_cmd_str[0], pass_str);
    strcat(password_cmd_str[1], enable_pass_str);
    
    for (counter = 0; counter < NUM_OF_CONFIG_STR ; counter ++)
    {
        strcpy(vty->buf, password_cmd_str[counter]);
        vline = cmd_make_strvec(vty->buf);
        
        /* In case of comment line */
        if (vline == NULL)
            continue;
            
        /* Execute configuration command : this is strict match */
        ret = cmd_execute_command_strict(vline, vty, NULL);
        
        /* Try again with setting node to CONFIG_NODE */
        while (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO && vty->node != CONFIG_NODE)
        {
            vty->node = node_parent(vty->node);
            ret = cmd_execute_command_strict(vline, vty, NULL);
        }
        
        cmd_free_strvec(vline);
        
        if (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO)
            return ret;
    }
    
#endif
#endif
    return (CMD_SUCCESS);
}

int internal_config_password()
{
#ifdef __USER_AUTH__

    /* Username of CLI */
    if (host.name)
        XFREE(0, host.name);
        
    host.name = NULL;
    
    if (strcmp(cmd_config_str[0], "") != 0)
        host.name = XSTRDUP(0, cmd_config_str[0]);
        
    /* Password of CLI */
    if (host.password)
        XFREE(0, host.password);
        
    host.password = NULL;
    
    if (strcmp(cmd_config_str[1], "") != 0)
        host.password = XSTRDUP(0, cmd_config_str[1]);
        
    /* Enable mode password of CLI */
    if (host.enable)
        XFREE(0, host.enable);
        
    host.enable = NULL;
    
    if (strcmp(cmd_config_str[2], "") != 0)
        host.enable = XSTRDUP(0, cmd_config_str[2]);
        
#else
        
    if (host.password)
        XFREE(0, host.password);
        
    host.password = NULL;
        
    if (strcmp(cmd_config_str[0], "") != 0)
        host.password = XSTRDUP(0, cmd_config_str[0]);
        
    if (host.enable)
        XFREE(0, host.enable);
        
    host.enable = NULL;
        
    if (strcmp(cmd_config_str[1], "") != 0)
        host.enable = XSTRDUP(0, cmd_config_str[1]);
        
#endif
#if 0
        
    for (counter = 0; counter < NUM_OF_CONFIG_STR ; counter ++)
    {
        strcpy(vty->buf, cmd_config_str[counter]);
        vline = cmd_make_strvec(vty->buf);
        
        /* In case of comment line */
        if (vline == NULL)
            continue;
            
        /* Execute configuration command : this is strict match */
        ret = cmd_execute_command_strict(vline, vty, NULL);
        
        /* Try again with setting node to CONFIG_NODE */
        while (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO && vty->node != CONFIG_NODE)
        {
            vty->node = node_parent(vty->node);
            ret = cmd_execute_command_strict(vline, vty, NULL);
        }
        
        cmd_free_strvec(vline);
        
        if (ret != CMD_SUCCESS && ret != CMD_WARNING
                && ret != CMD_ERR_NOTHING_TODO)
            return ret;
    }
    
#endif
    return CMD_SUCCESS;
}

/** Analyzer a range of uint32_t type keyword which division with '-' */
int continuous_keyword_analyzer(struct vty *vty, char *keyword_range_str, uint32_t *keyword_start, uint32_t *keyword_end)
{
    vty = vty;
    uint32_t tfrom;
    uint32_t tto;
    
    if (strchr(keyword_range_str, '-'))
    {
        sscanf(keyword_range_str, "%d-%d", &tfrom, &tto);
    }
    
    else
    {
        sscanf(keyword_range_str, "%d", &tfrom);
        tto = tfrom;
    }
    
    *keyword_start = tfrom;
    *keyword_end = tto;
    return CMD_SUCCESS;
}
/** Analyzer a range of uint32_t type keyword which division with ',' */
int uncontinuous_keyword_analyzer(struct vty *vty, char *keyword_range_str, uint32_t *keyword_array)
{
    int i, index = 0;
    char *p, *p1, *p2 ;
    uint32_t keyword[MAX_UNCONTINUOUS_KEYWORD_COUNT] = { 0 };
    
    if (strlen(keyword_range_str) > MAX_UNCONTINUOUS_KEYWORD_COUNT)
    {
        vty_out(vty, " Error: The keyword string is too long (1-%d) ! %s", MAX_UNCONTINUOUS_KEYWORD_STR_LEN, VTY_NEWLINE);
        return -1;
    }
    
    for (i = 0; i < (int) strlen(keyword_range_str); i++)
    {
        if ((keyword_range_str[i] >= '0' && keyword_range_str[i] <= '9') || keyword_range_str[i] == ',')
            continue;
            
        else
        {
            vty_out(vty, " Error: Invalid keyword number! %s",  VTY_NEWLINE);
            return -1;
        }
    }
    
    if (strstr(keyword_range_str, ",,"))
    {
        vty_out(vty, " Error: Invalid keyword number! %s",  VTY_NEWLINE);
        return -1;
    }
    
    if (keyword_range_str[strlen(keyword_range_str) - 1] == ',')
    {
        vty_out(vty, " Error: Invalid keyword number! %s",  VTY_NEWLINE);
        return -1;
    }
    
    if (keyword_range_str[0] == ',')
    {
        vty_out(vty, " Error: Invalid keyword number! %s",  VTY_NEWLINE);
        return -1;
    }
    
    p = p1 = keyword_range_str;
    
    while (*p != 0)
    {
#if 0
    
        if (atoi(p1) == 0 || atoi(p1) > 65535)
        {
            vty_out(vty, " Error: Invalid keyword number %d (1-65535)! %s",  atoi(p1), VTY_NEWLINE);
            return -1;
        }
        
#endif
        p2 = strchr(p1, ',');
        
        if (p2 != NULL)
        {
            keyword[index++] = atoi(p1);
            p = p1 = p2 + 1;
        }
        
        else
        {
            keyword[index++] = atoi(p1);
            break;
        }
    }
    
    for (i = 0; i < MAX_UNCONTINUOUS_KEYWORD_COUNT; i++)
    {
        if (keyword[i] != 0)
        {
            *keyword_array++ = keyword[i];
        }
    }
    
    return CMD_SUCCESS;
}
/* Set config filename.  Called from vty.c */
void
host_config_set(char *filename)
{
    host.config = strdup(filename);
}
/* Down vty node level. */
DEFUN(config_exit,
      config_exit_cmd,
      "exit",
      "Exit current mode and down to previous mode\n"
      "退出当前模式\n")
{
    self = self;
    argc = argc;
    argv = argv;

    switch (vty->node)
    {
        case VIEW_NODE:
        case ENABLE_NODE:
            if (vty_shell(vty))
                exit(0);
                
            else
#ifdef __VTY_NO_CLOSE__
                vty->node = AUTH_USER_NODE;
                
#else
                vty->status = VTY_CLOSE;
#endif
            break;
            
        case CONFIG_NODE:
            vty->node = ENABLE_NODE;
            vty_config_unlock(vty);
            break;
            
        case INTERFACE_NODE:
        case RIPNG_NODE:
        case OSPF_NODE:
        case OSPF6_NODE:
        case ISIS_NODE:
        case MASC_NODE:
        case RMAP_NODE:
        case VTY_NODE:
        case SESSION_NODE:
        case APP_NODE:
        case ACL_NODE:
        case STRING_NODE:
        case IFGRP_NODE:
            vty->node = CONFIG_NODE;
            break;
            
        default:
            break;
    }
    
    return CMD_SUCCESS;
}

/* quit is alias of exit. */
ALIAS(config_exit,
      config_quit_cmd,
      "quit",
      "Exit current mode and down to previous mode\n"
      "退出当前模式\n")

/* End of configuration. */
DEFUN(config_end, config_end_cmd,
      "end",
      "End current mode and change to enable mode.\n"
      "退出到特权用户模式\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
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
            break;
    }
    
    return CMD_SUCCESS;
}

/* Help display function for all node. */
DEFUN(config_help, config_help_cmd,
      "help",
      "Description of the interactive help system\n"
      "显示交互帮助系统\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    vty_out(vty,
            "CLI provides advanced help feature.  When you need help,%s\
anytime at the command line please press '?'.%s\
%s\
If nothing matches, the help list will be empty and you must backup%s\
 until entering a '?' shows the available options.%s\
Two styles of help are provided:%s\
1. Full help is available when you are ready to enter a%s\
command argument (e.g. 'show ?') and describes each possible%s\
argument.%s\
2. Partial help is provided when an abbreviated argument is entered%s\
   and you want to know what arguments match the input%s\
   (e.g. 'show me?'.)%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE,
            VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE,
            VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
    return CMD_SUCCESS;
}

/* Help display function for all node. */
DEFUN(config_list, config_list_cmd,
      "list",
      "List all commands of the present mode\n"
      "显示当前模式下所有命令\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    unsigned int i;
    struct cmd_node *cnode = vector_slot(cmdvec, vty->node);
    struct cmd_element *cmd;
    
    for (i = 0; i < vector_max(cnode->cmd_vector); i++)
        if ((cmd = vector_slot(cnode->cmd_vector, i)) != NULL)
            vty_out(vty, "  %s%s", cmd->string,
                    VTY_NEWLINE);
                    
    return CMD_SUCCESS;
}

#define __configuration_restore__
/** Restore current configuration, by tsihang, 4 Nov, 2014. */
DEFUN(config_restore, config_restore_cmd,
      "restore [FILE-NAME]",
      "Restore default configuration\n"
      "恢复默认配置\n"
      "File of the configuration used to restore\n"
      "用以恢复配置文件\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
#if 0
    /** Restore system with a known configuraion file. */
    if (argc == 1)
    {
        FILE *confp = NULL;
        confp = fopen(argv[0], "r");
        
        if (confp == NULL)
        {
            vty_out(vty, "Can't open configuration file [%s] ! %s", argv[0], VTY_NEWLINE);
            return CMD_WARNING;
        }
        
        config_from_file(vty, confp);
    }
    
    else
    {
        vty->restore_flag = 1;
        common_config_restore(vty);
        system_config_restore(vty);
        ifgrp_config_restore(vty);
        interface_config_restore(vty);
        ipmask_config_restore(vty);
#ifdef UMTS_ENA
        umts_config_restore(vty);
#endif
        singleip_config_restore(vty);
        rule_fivetuple_config_restore(vty);
        string_config_restore(vty);
        rvlan_config_restore(vty);
#ifdef FFWD_ENA        
        ffwd_config_restore(vty);
#endif
        dll_config_restore(vty);
        snmp_config_restore(vty);
#ifdef BYPASS_ENA
        bypass_config_restore(vty);
        watchdog_config_restore(vty);
        opb_config_restore(vty);
#endif
        url_config_restore(vty);

        vty->restore_flag = 0;
    }
#endif
    
    return CMD_SUCCESS;
}
#define __configuration_save__
/** Save current configuration into the terminal, by tsihang, 4 Nov, 2014. */
DEFUN(config_write_file, config_save_cmd,
      "save [FILE-NAME]",
      "Save running configuration to memory, network, or terminal\n"
      "记录运行配置\n"
      "Save confiuration to file\n"
      "保存配置到指定的文件\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int i;
    int fd;
    FILE *fp = NULL;
    struct cmd_node *node;
    char config_file[128];
    struct vty *file_vty;
    
    if (argc == 1)
        strcpy(config_file, argv[0]);
        
    else
        strcpy(config_file, VTY_CURRENT_CONFIG);
        
    fp = fopen(config_file, "w");
    fd = fileno(fp);
    
    if (fd < 0)
    {
        vty_out(vty, "Can't open configuration file %s.%s", config_file, VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    /* Make vty for configuration file. */
    file_vty = vty_new();
    file_vty->fd = fd;
    file_vty->type = VTY_FILE;
    
    for (i = 0; i < (int) vector_max(cmdvec); i++)
    {
        if ((node = vector_slot(cmdvec, i)) && node->func)
        {
            if ((*node->func)(file_vty))
                vty_out(file_vty, "!\n");
        }
    }
    
    vty_close(file_vty);
    fclose(fp);
    int ret = system ("sync");
    if ((-1 == ret) || (!WIFEXITED(ret)) || (0 != WEXITSTATUS(ret)))
    {
        vty_out (vty, "sync faiulre! %s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    return CMD_SUCCESS;
}

ALIAS(config_write_file, config_write_cmd,
      "write",
      "Write running configuration to memory, network, or terminal\n"
      "记录运行配置\n")

ALIAS(config_write_file, config_write_memory_cmd,
      "write memory",
      "Write running configuration to memory, network, or terminal\n"
      "记录运行配置\n"
      "Write configuration to the file (same as write file)\n"
      "把运行配置写到文件\n")

ALIAS(config_write_file,  copy_runningconfig_startupconfig_cmd,
      "copy running-config startup-config",
      "Copy configuration\n"
      "拷贝配置\n"
      "Copy running config to... \n"
      "拷贝运行配置\n"
      "Copy running config to startup config (same as write file)\n"
      "拷贝运行配置到启动配置文件\n")

/** Write current configuration into the terminal, by tsihang 4 Nov, 2014. */
DEFUN(config_write_terminal, config_write_terminal_cmd,
      "write terminal",
      "Write running configuration to memory, network, or terminal\n"
      "记录运行配置\n"
      "Write to terminal\n"
      "把运行配置写到终端设备\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    unsigned int i;
    struct cmd_node *node;
    
    if (vty->type == VTY_SHELL_SERV)
    {
        for (i = 0; i < vector_max(cmdvec); i++)
            if ((node = vector_slot(cmdvec, i)) && node->func && node->vtysh)
            {
                if ((*node->func)(vty))
                {
                    ;/* vty_out(vty, "!%s", VTY_NEWLINE);*/
                }
            }
    }
    
    else
    {
        vty_out(vty, "%sCurrent configuration:%s", VTY_NEWLINE, VTY_NEWLINE);
        /* vty_out(vty, "!%s", VTY_NEWLINE);*/
        
        for (i = 0; i < vector_max(cmdvec); i++)
        {
            if ((node = vector_slot(cmdvec, i)) && node->func)
            {
                if ((*node->func)(vty))
                {
                    ;/* vty_out(vty, "!%s", VTY_NEWLINE);*/
                }
            }
        }
        
        vty_out(vty, "end%s", VTY_NEWLINE);
    }
    
    return CMD_SUCCESS;
}

/** Write current configuration into the terminal, by tsihang, 4 Nov, 2014. */
ALIAS(config_write_terminal, show_running_config_cmd,
      "show running-config",
      SHOW_STR
      SHOW_CSTR
      "Running configuration\n"
      "当前系统配置\n")

/** Write current configuration into the terminal, by tsihang, 4 Nov, 2014. */
DEFUN(show_startup_config, show_startup_config_cmd,
      "show startup-config",
      SHOW_STR
      SHOW_CSTR
      "Contentes of startup configuration\n"
      "显示重启动配置信息\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    char buf[512];
    FILE *confp;
    confp = fopen(VTY_CURRENT_CONFIG, "r");
    
    if (confp == NULL)
    {
        vty_out(vty, " Error<362>: Can't open configuration file [%s] ! %s", VTY_CURRENT_CONFIG, VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    while (fgets(buf, 512, confp))
    {
        char *cp = buf;
        
        while (*cp != '\r' && *cp != '\n' && *cp != '\0')
            cp++;
            
        *cp = '\0';
        vty_out(vty, "%s%s", buf, VTY_NEWLINE);
    }
    
    fclose(confp);
    return CMD_SUCCESS;
}

DEFUN(config_terminal_length, config_terminal_length_cmd,
      "terminal length <0-512>",
      "Set terminal line parameters\n"
      "设置终端显示参数\n"
      "Set number of lines on a screen\n"
      "终端显示行数\n"
      "Number of lines on screen (0 for no pausing)\n"
      "终端显示行数取值范围\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int lines;
    char *endptr = NULL;
    lines = strtol(argv[0], &endptr, 10);
    
    if (lines < 0 || lines > 512 || *endptr != '\0')
    {
        vty_out(vty, "length is malformed%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    vty->lines = lines;
    return CMD_SUCCESS;
}

DEFUN(config_terminal_no_length, config_terminal_no_length_cmd,
      "no terminal length",
      NO_STR
      NO_CSTR
      "Set terminal line parameters\n"
      "设置终端显示参数\n"
      "Set number of lines on a screen\n"
      "终端显示行数\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    vty->lines = -1;
    return CMD_SUCCESS;
}

DEFUN(service_terminal_length, service_terminal_length_cmd,
      "service terminal-length <0-512>",
      "Set up miscellaneous service\n"
      "System wide terminal length configuration\n"
      "Number of lines of VTY (0 means no line control)\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int lines;
    char *endptr = NULL;
    lines = strtol(argv[0], &endptr, 10);
    
    if (lines < 0 || lines > 512 || *endptr != '\0')
    {
        vty_out(vty, "length is malformed%s", VTY_NEWLINE);
        return CMD_WARNING;
    }
    
    host.lines = lines;
    return CMD_SUCCESS;
}

DEFUN(no_service_terminal_length, no_service_terminal_length_cmd,
      "no service terminal-length [<0-512>]",
      NO_STR
      NO_CSTR
      "Set up miscellaneous service\n"
      "System wide terminal length configuration\n"
      "Number of lines of VTY (0 means no line control)\n")
{
    self = self;
    argc = argc;
    argv = argv;
    vty = vty;
    
    host.lines = -1;
    return CMD_SUCCESS;
}


DEFUN_HIDDEN(do_echo, echo_cmd,
             "echo .MESSAGE",
             "Echo a message back to the CLI screen\n"
             "向命令行终端输出信息\n"
             "The message to echo\n"
             "信息\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    char *message;
    vty_out(vty, "%s%s", (message = argv_concat(argv, argc, 0)), VTY_NEWLINE);
    XFREE(MTYPE_TMP, message);
    return CMD_SUCCESS;
}

#define Tsihang
#ifdef Tsihang
/* added by Tsihang 2012-11-13*/
DEFUN(language_english, language_english_cmd,
      "english",
      "Description with English\n"
      "切换至英文描述状态\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int i;
    int j;
    struct cmd_node *cnode;
    struct cmd_element *cmdelement;
    
#if 0
    int k;
    int n;
    vector cmdvector;
    struct desc *cmddesc;
    char *temp;
    
    if (language == 1)
        language = 0;
#endif
        
    if (vty->language != LAUGUAGE_ENGLISH)
        vty->language = LAUGUAGE_ENGLISH;
        
    for (i = 0; i < (int) vector_max(cmdvec); i++)
    {
        if ((cnode = vector_slot(cmdvec, i)) != NULL)
        {
            for (j = 0; j < (int) vector_max(cnode->cmd_vector); j++)
            {
                if ((cmdelement = vector_slot(cnode->cmd_vector, j)) != NULL)
                {
                    if (cmdelement->languageflag == LAUGUAGE_CHINESE)
                    {
#if 0
                    
                        for (k = 0; k < (int) vector_max(cmdelement->strvec); k++)
                        {
                            if ((cmdvector = vector_slot(cmdelement->strvec, k)) != NULL)
                            {
                                for (n = 0; n < (int) vector_max(cmdvector); n++)
                                {
                                    if ((cmddesc = vector_slot(cmdvector, n)) != NULL)
                                    {
                                        //switch pointer of CSTR and STR
                                        temp = (char *) cmddesc->cstr;
                                        cmddesc->cstr = cmddesc->str;
                                        cmddesc->str = temp;
                                    }
                                }
                            }
                        }
                        
#endif
                        cmdelement->languageflag = LAUGUAGE_ENGLISH;
                    }
                    
                    else
                        continue;
                }
            }
        }
    }
    
    return CMD_SUCCESS;
}
DEFUN(language_chinese, language_chinese_cmd,
      "chinese",
      "Description with chinese\n"
      "切换至中文描述状态\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
    int i;
    int j;
    
    struct cmd_node *cnode;
    struct cmd_element *cmdelement;
    
    
#if 0
    int k;
    int n;
    vector cmdvector;
    struct desc *cmddesc;
    char *temp;
    
    if (language == 0)
        language = 1;
#endif
        
    if (vty->language != LAUGUAGE_CHINESE)
        vty->language = LAUGUAGE_CHINESE;
        
    for (i = 0; i < (int) vector_max(cmdvec); i++)
    {
        if ((cnode = vector_slot(cmdvec, i)) != NULL)
        {
            for (j = 0; j < (int) vector_max(cnode->cmd_vector); j++)
            {
                if ((cmdelement = vector_slot(cnode->cmd_vector, j)) != NULL)
                {
                    if (cmdelement->languageflag == LAUGUAGE_ENGLISH)
                    {
#if 0
                    
                        for (k = 0; k < (int) vector_max(cmdelement->strvec); k++)
                        {
                            if ((cmdvector = vector_slot(cmdelement->strvec, k)) != NULL)
                            {
                                for (n = 0; n < (int) vector_max(cmdvector); n++)
                                {
                                    if ((cmddesc = vector_slot(cmdvector, n)) != NULL)
                                    {
                                        temp = (char *) cmddesc->cstr;
                                        cmddesc->cstr = cmddesc->str;
                                        cmddesc->str = temp;
                                    }
                                }
                            }
                        }
                        
#endif
                        cmdelement->languageflag = LAUGUAGE_CHINESE;
                    }
                    
                    else
                        continue;
                }
            }
        }
    }
    
    return CMD_SUCCESS;
}

DEFUN(show_language, show_language_cmd,
      "show language",
      SHOW_STR
      SHOW_CSTR
      "language configuration\n"
      "显示命令描述语言状态\n")
{
    self = self;
    argc = argc;
    argv = argv;
    
#if 0

    if (language == 0)
    {
        vty_out(vty, " The description language used  at present is English %s", VTY_NEWLINE);
    }
    
    else
    {
        vty_out(vty, " 系统描述语言为中文 %s", VTY_NEWLINE);
    }
    
#else
    
    if (vty->language == LAUGUAGE_ENGLISH)
    {
        vty_out(vty, " The description language used  at present is English %s", VTY_NEWLINE);
    }
    
    else
    {
        vty_out(vty, " 系统描述语言为中文 %s", VTY_NEWLINE);
    }
    
#endif
    return CMD_SUCCESS;
}
/*end added*/
void
install_language(enum node_type node)
{
    install_element(node, &show_language_cmd);
    install_element(node, &language_english_cmd);
    install_element(node, &language_chinese_cmd);
}
#endif

void
install_default(enum node_type node)
{
    install_element(node, &config_exit_cmd);
    install_element(node, &config_quit_cmd);
    install_element(node, &config_end_cmd);
    install_element(node, &config_help_cmd);
    install_element(node, &config_list_cmd);
    install_element(node, &echo_cmd);
}


void common_cmd_initialize()
{
    /* Install top nodes. */
    install_node(&view_node, NULL);
    install_node(&enable_node, NULL);
#ifdef __USER_AUTH__
    install_node(&auth_user_node, NULL);
#endif
    install_node(&auth_node, NULL);
    install_node(&auth_enable_node, NULL);
    install_node(&config_node, config_write_host);
    install_node(&access_list_node, NULL);
    install_default(ENABLE_NODE);
    install_default(CONFIG_NODE);
    install_default(ACL_NODE);
    install_language(VIEW_NODE);
    install_language(ENABLE_NODE);
    install_language(CONFIG_NODE);
    install_language(ACL_NODE);
    install_element(VIEW_NODE,                         &config_list_cmd);
    install_element(VIEW_NODE,                         &config_exit_cmd);
    install_element(VIEW_NODE,                         &config_quit_cmd);
    install_element(VIEW_NODE,                         &config_help_cmd);
    install_element(VIEW_NODE,                         &show_version_cmd);
    install_element(VIEW_NODE,                         &config_enable_cmd);
    install_element(ENABLE_NODE,                       &config_cmd);
    install_element(CONFIG_NODE,                       &show_running_config_cmd);
    install_element(CONFIG_NODE,                       &show_startup_config_cmd);
    install_element(CONFIG_NODE,                       &config_save_cmd);
    install_element(CONFIG_NODE,                       &config_restore_cmd);
    install_element(CONFIG_NODE,                       &config_write_terminal_cmd);
    install_element(CONFIG_NODE,                       &copy_runningconfig_startupconfig_cmd);
    install_element(CONFIG_NODE,                       &config_terminal_length_cmd);
    install_element(CONFIG_NODE,                       &config_terminal_no_length_cmd);
    //install_element (CONFIG_NODE,                       &service_terminal_length_cmd);
    //install_element (CONFIG_NODE,                       &no_service_terminal_length_cmd);
}

/* Initialize command interface. Install basic nodes and commands. */
void
cmdline_runtime_env_initialize()
{
    /* Allocate initial top vector of commands. */
    cmdvec = vector_init(VECTOR_MIN_SIZE);
    /* Default host value settings. */
    host.name = NULL;
    host.password = NULL;
    host.enable = NULL;
    host.logfile = NULL;
    host.config = NULL;
    host.lines = -1;
    host.motd = default_motd;
    srand(time(NULL));
}
