/*
 * Zebra configuration command interface routine
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _ZEBRA_COMMAND_H
#define _ZEBRA_COMMAND_H

#include "vector.h"
#include "thread.h"
#include "vty.h"

/* Host configuration variable */
struct host
{
    /* Host name of this router. */
    char *name;
    
    /* Password for vty interface. */
    char *password;
    char *password_encrypt;
    
    /* Enable password */
    char *enable;
    char *enable_encrypt;
    
    /* System wide terminal lines. */
    int lines;
    
    /* Log filename. */
    char *logfile;
    
    /* config file name of this host */
    char *config;
    
    /* Flags for services */
    int advanced;
    int encrypt;
    
    /* Banner configuration. */
    const char *motd;
};


/* There are some command levels which called from command node. */
enum node_type
{
#ifdef __USER_AUTH__
    AUTH_USER_NODE,       /* User authentication */
#endif
    AUTH_NODE,            /* Authentication mode of vty interface. */
    VIEW_NODE,            /* View node. Default mode of vty interface. */
    AUTH_ENABLE_NODE, /* Authentication mode for change enable. */
    ENABLE_NODE,          /* Enable node. */
    CONFIG_NODE,          /* Config node. Default mode of config file. */
    INTERFACE_NODE,       /* Interface mode node. */
    TABLE_NODE,           /* rtm_table selection node. */
    RIPNG_NODE,           /* RIPng protocol mode node. */
    OSPF_NODE,            /* OSPF protocol mode */
    OSPF6_NODE,           /* OSPF protocol for IPv6 mode */
    ISIS_NODE,                /* ISIS protocol mode */
    MASC_NODE,            /* MASC for multicast.  */
    IRDP_NODE,            /* ICMP Router Discovery Protocol mode. */
    IP_NODE,              /* Static ip route node. */
    ACCESS_NODE,          /* Access list node. */
    PREFIX_NODE,          /* Prefix list node. */
    ACCESS_IPV6_NODE,     /* Access list node. */
    PREFIX_IPV6_NODE,     /* Prefix list node. */
    AS_LIST_NODE,         /* AS list node. */
    COMMUNITY_LIST_NODE,  /* Community list node. */
    RMAP_NODE,            /* Route map node. */
    SMUX_NODE,            /* SNMP configuration node. */
    DUMP_NODE,            /* Packet dump node. */
    FORWARDING_NODE,      /* IP forwarding node. */
    VTY_NODE,             /* Vty node. */
    FW_CRAFT_NODE,        /* FW CRAFT node */
    MARVELL_SWITCH_NODE,  /* Marvell switch node */
    BCM_SWITCH_NODE,
    SESSION_NODE,
    ACL_NODE,
    APP_NODE,
    IFGRP_NODE,           /* interface group node */
    STRING_NODE,          /* string group node */
    
};

/* Node which has some commands and prompt string and configuration
   function pointer . */
struct cmd_node
{
    /* Node index. */
    enum node_type node;
    
    /* Prompt character at vty interface. */
    const char *prompt;
    
    /* Is this node's configuration goes to vtysh ? */
    int vtysh;
    
    /* Node's configuration write function */
    int (*func)(struct vty *);
    
    /* Vector of this node's command list. */
    vector cmd_vector;
};

enum
{
    CMD_ATTR_DEPRECATED,
    CMD_ATTR_HIDDEN,
};
#define MAX_CMD_STRING_SIZE 511
/* Structure of command element. */
struct cmd_element
{
    const char *string;           /* Command specification by string. */
    //char string[MAX_CMD_STRING_SIZE + 1];
    int (*func)(struct cmd_element *, struct vty *, int, const char *[]);
    const char *doc;          /* Documentation of this command. */
    int daemon;                   /* Daemon to which this command belong. */
    vector strvec;        /* Pointing out each description vector. */
    unsigned int cmdsize;     /* Command index count. */
    char *config;         /* Configuration string */
    vector subconfig;     /* Sub configuration string */
    u_char attr;          /* Command attributes */
    //add by Tsihang for  language changing
    int languageflag;            /* language used in description of the command */
};

/* Command description structure. */
struct desc
{
    const char *cmd;          /* Command string. */
    const char *str;          /* Command's description. */
    //add by qihang
    const char *cstr;            /* Command's description in Chinese. */
};


extern int language;

/* Return value of the commands. */
#define CMD_SUCCESS                         0
#define CMD_WARNING                         1
#define CMD_ERR_NO_MATCH                    2
#define CMD_ERR_AMBIGUOUS               3
#define CMD_ERR_INCOMPLETE              4
#define CMD_ERR_EXEED_ARGC_MAX          5
#define CMD_ERR_NOTHING_TODO            6
#define CMD_COMPLETE_FULL_MATCH         7
#define CMD_COMPLETE_MATCH              8
#define CMD_COMPLETE_LIST_MATCH         9
#define CMD_SUCCESS_DAEMON              10

#define MAX_UNCONTINUOUS_KEYWORD_COUNT      32
#define MAX_UNCONTINUOUS_KEYWORD_STR_LEN   64
extern int continuous_keyword_analyzer(struct vty *vty, char *keyword_range_str, uint32_t *keyword_start, uint32_t *keyword_end);
extern int uncontinuous_keyword_analyzer(struct vty *vty, char *keyword_range_str, uint32_t *keyword_array);

/* Argc max counts. */
/* Changed by Tsihang, Semptian. :
   Increase the argc from 25 to 50 */
#define CMD_ARGC_MAX   50

/* Turn off these macros when uisng cpp with extract.pl */
#ifndef VTYSH_EXTRACT_PL

/* helper defines for end-user DEFUN* macros */
#if 0
#define DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
    struct cmd_element cmdname = \
    { \
        .string = cmdstr, \
                  .func = funcname, \
                          .doc = helpstr, \
                                 .attr = attrs, \
                                         .daemon = dnum, \
    };
#endif

#define DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
    struct cmd_element cmdname = { cmdstr, funcname, helpstr, dnum, 0, 0, 0, 0, attrs,0};

#define DEFUN_CMD_FUNC_DECL(funcname) \
    int funcname (struct cmd_element *, struct vty *, int, const char *[]); \
     
#define DEFUN_CMD_FUNC_TEXT(funcname) \
    int funcname \
    (struct cmd_element *self, struct vty *vty, int argc, const char *argv[])

/** Modified by tsihang, August 6, 2014 */
/* DEFUN for vty command interafce. Little bit hacky ;-). */
#define DEFUN(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_FUNC_DECL(funcname) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0) \
    DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
    DEFUN_CMD_FUNC_DECL(funcname) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0) \
    DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUN_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED) \
     
/* DEFUN_NOSH for commands that vtysh should ignore */
#define DEFUN_NOSH(funcname, cmdname, cmdstr, helpstr) \
    DEFUN(funcname, cmdname, cmdstr, helpstr)

/* DEFSH for vtysh. */
#define DEFSH(daemon, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(NULL, cmdname, cmdstr, helpstr, 0, daemon) \
     
/* DEFUN + DEFSH */
#define DEFUNSH(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_FUNC_DECL(funcname) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon) \
    DEFUN_CMD_FUNC_TEXT(funcname)

/* DEFUN + DEFSH with attributes */
#define DEFUNSH_ATTR(daemon, funcname, cmdname, cmdstr, helpstr, attr) \
    DEFUN_CMD_FUNC_DECL(funcname) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, daemon) \
    DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUNSH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUNSH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED)

/* ALIAS macro which define existing command's alias. */
#define ALIAS(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0)

#define ALIAS_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0)

#define ALIAS_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, 0)

#define ALIAS_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, 0)

#define ALIAS_SH(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon)

#define ALIAS_SH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, daemon)

#define ALIAS_SH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
    DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, daemon)

#endif /* VTYSH_EXTRACT_PL */

/* Some macroes */
#define CMD_OPTION(S)   ((S[0]) == '[')
#define CMD_VARIABLE(S) (((S[0]) >= 'A' && (S[0]) <= 'Z') || ((S[0]) == '<'))
#define CMD_VARARG(S)   ((S[0]) == '.')
#define CMD_RANGE(S)    ((S[0] == '<'))
#define CMD_HEX_RANGE(S)    ((S[0] == '<') && (S[1] == '0') && (S[2] == 'x'))
//#define CMD_RANGE_GENERAL(S)    ((S[0] == '<') && (S[1] == '0') && (S[2] == 'x') || (S[0] == '<'))    /** Removed by tsihang */
#define CMD_RANGE_GENERAL(S)    (((S[0] == '<') && (S[1] == '0') && (S[2] == 'x')) || (S[0] == '<'))   /** Replaced by tsihang */
#define CMD_DEV_INF(S) ((strcmp ((S), "<0-3>/<0-15>") == 0))

#define CMD_IPV4(S)    ((strcmp ((S), "A.B.C.D") == 0))
#define CMD_IPV4_PREFIX(S) ((strcmp ((S), "A.B.C.D/M") == 0))
#define CMD_IPV6(S)        ((strcmp ((S), "X:X::X:X") == 0))
#define CMD_IPV6_PREFIX(S) ((strcmp ((S), "X:X::X:X/M") == 0))

/* Common descriptions. */
#define SHOW_STR "Show Running System Information\n"
//add by qihang
#define SHOW_VERSION "Show version\n"
#define SHOW_CSTR "显示系统信息\n"
#define NO_STR "Negate a command or set it to its defaults\n"
#define NO_CSTR "恢复缺省设置\n"
#define ENABLE_STR  "Enable Control\n"
#define ENABLE_CSTR "使能控制\n"
#define DISABLE_STR "Disable Control\n"
#define DISABLE_CSTR    "去使能控制\n"
#define CLEAR_STR "Reset functions\n"
#define CLEAR_CSTR  "清除功能\n"
#define DEL_STR "Delete function\n"
#define DEL_CSTR "删除功能\n"
#define RIP_STR "RIP information\n"
#define BGP_STR "BGP information\n"
#define OSPF_STR "OSPF information\n"
#define NEIGHBOR_STR "Specify neighbor router\n"
#define DEBUG_STR "Debugging functions (see also 'undebug')\n"
#define UNDEBUG_STR "Disable debugging functions (see also 'debug')\n"
#define ROUTER_STR "Enable a routing process\n"
#define AS_STR "AS number\n"
#define MBGP_STR "MBGP information\n"
#define MATCH_STR "Match values from routing table\n"
#define SET_STR "Set configuration values\n"
#define SET_CSTR "配置\n"
#define OUT_STR "Filter outgoing routing updates\n"
#define IN_STR  "Filter incoming routing updates\n"
#define V4NOTATION_STR "specify by IPv4 address notation(e.g. 0.0.0.0)\n"
#define OSPF6_NUMBER_STR "Specify by number\n"
#define INTERFACE_STR "Interface Configuration\n"
#define INTERFACE_CSTR  "接口配置\n"
#define IFNAME_STR "Interface name(e.g. ep0)\n"
#define IP6_STR "IPv6 Information\n"
#define OSPF6_STR "Open Shortest Path First (OSPF) for IPv6\n"
#define OSPF6_ROUTER_STR "Enable a routing process\n"
#define OSPF6_INSTANCE_STR "<1-65535> Instance ID\n"
#define SECONDS_STR "<1-65535> Seconds\n"
#define ROUTE_STR "Routing Table\n"
#define PREFIX_LIST_STR "Build a prefix list\n"
#define OSPF6_DUMP_TYPE_LIST \
    "(neighbor|interface|area|lsa|zebra|config|dbex|spf|route|lsdb|redistribute|hook|asbr|prefix|abr)"
#define ISIS_STR "IS-IS information\n"
#define AREA_TAG_STR "[area tag]\n"

#define CONF_BACKUP_EXT ".sav"


#define INIT_STR            "Initalize operation\n"
#define TERM_STR            "Terminate operation\n"
#define SYSTEM_STR          "Set Semptian API system parameters during runtime\n"
#define ALARM_STR           "Alarm event generation thresholds\n"
#define LOG_STR             "Change the current logging configuration\n"
#define OPEN_ENCRYPT_STR    "Open encryption configuration\n"
#define MARVELL_SWITCH_STR  "Marvell switch configuration\n"
/* IPv4 only machine should not accept IPv6 address for peer's IP
   address.  So we replace VTY command string like below. */
#ifdef HAVE_IPV6
#define NEIGHBOR_CMD       "neighbor (A.B.C.D|X:X::X:X) "
#define NO_NEIGHBOR_CMD    "no neighbor (A.B.C.D|X:X::X:X) "
#define NEIGHBOR_ADDR_STR  "Neighbor address\nIPv6 address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor IPv6 address\nNeighbor tag\n"
#else
#define NEIGHBOR_CMD       "neighbor A.B.C.D "
#define NO_NEIGHBOR_CMD    "no neighbor A.B.C.D "
#define NEIGHBOR_ADDR_STR  "Neighbor address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor tag\n"
#endif /* HAVE_IPV6 */

#define PORT_STR           "TCP or UDP port (eg. 12,56,556; 12-556)\n"
#define PORT_CSTR          "TCP 或者UDP 的端口号(eg. 12,56,556; 12-556)\n"

#define SRC_IP_STR                  "Source ip address\n"
#define SRC_IP_CSTR                 "源IP 地址\n"
#define SRC_IP_RANGE_STR            "Source ip address range\n"
#define SRC_IP_RANGE_CSTR           "源IP 地址范围\n"
#define DST_IP_STR                  "Destination ip address\n"
#define DST_IP_CSTR                 "目的IP 地址\n"
#define DST_IP_RANGE_STR            "Destination ip address range\n"
#define DST_IP_RANGE_CSTR           "目的IP 地址范围\n"
#define SRC_PORT_STR                "Source port number\n"
#define SRC_PORT_CSTR               "配置源端口号\n"
#define SRC_PORT_RANGE_STR          "Source port number range\n"
#define SRC_PORT_RANGE_CSTR         "配置源端口号范围\n"
#define DST_PORT_STR                "Destination port number\n"
#define DST_PORT_CSTR               "配置目的端口号\n"
#define DST_PORT_RANGE_STR          "Destination port number range\n"
#define DST_PORT_RANGE_CSTR         "配置目的端口号范围\n"
#define DISCONTINUED_PORT_STR       "Discontinue port number(eg. 1,3-5,7)\n"
#define DISCONTINUED_PORT_CSTR      "不连续的端口号(eg. 1,3-5,7)\n"
#define DISCONTINUED_RULE_ID_STR    "Discontinue rule ID(eg. 1,3-10,16)\n"
#define DISCONTINUED_RULE_ID_CSTR   "不连续的规则ID(eg. 1,3-10,16)\n"
#define IP_STR                      "Configure IPv4 Address\n"
#define IP_CSTR                     "配置IPv4 地址\n"
#define IPV6_STR                    "Configure IPv6 Address\n"
#define IPV6_CSTR                   "配置IPv6 地址\n"
#define CHECK_STR                   "Check rule by configuration\n"
#define CHECK_CSTR                  "根据配置信息查询规则\n"
#define RULE_STR                    "Access control list\n"
#define RULE_CSTR                   "访问控制列表配置\n"

#define TYPE_GE_STR                 "Gigabit ethernet interface type\n"
#define TYPE_GE_CSTR                "千兆以太网接口\n"
#define TYPE_XE_STR                 "Ten gigabit ethernet interface type\n"
#define TYPE_XE_CSTR                "万兆以太网接口\n"

#define INTERFACE_ID_STR            "Interface id (eg. 0/0; 0/0,0/11; 0/0-0/11; all)\n"
#define INTERFACE_ID_CSTR           "接口索引(eg. 0/0; 0/0,0/11; 0/0-0/11; all)\n"

#define XAUI_ID_STR                 "Xaui id(eg. 0;1-2,4)\n"
#define XAUI_ID_CSTR                "端口索引(eg. 0;1-2,4)\n"

#define IFGRP_DEL_STR               "Interface group delete configuration\n"
#define IFGRP_DEL_CSTR              "删除接口组配置\n"

#define IIFGRP_STR                  "Ingress interface group configuration\n"
#define IIFGRP_CSTR                 "入接口组配置\n"
#define IIFGRP_INDEX_STR            "Ingress interface group ID\n"
#define IIFGRP_INDEX_CSTR           "入接口组ID\n"

#define OIFGRP_STR                  "Egress interface group configuration\n"
#define OIFGRP_CSTR                 "出接口组配置\n"
#define OIFGRP_INDEX_STR            "Egress interface group ID\n"
#define OIFGRP_INDEX_CSTR           "出接口组ID\n"

#define VIFGRP_STR                  "Virtual interface group configuration\n"
#define VIFGRP_CSTR                 "虚接口组配置\n"
#define VIFGRP_INDEX_STR            "Virtual interface group ID\n"
#define VIFGRP_INDEX_CSTR           "虚接口组ID\n"

#define STATISTICS_STR              "Statistics information\n"
#define STATISTICS_CSTR             "统计信息\n"

/* Prototypes. */
void install_node(struct cmd_node *, int ( *)(struct vty *));
void install_default(enum node_type);
void install_element(enum node_type, struct cmd_element *);
void sort_node();

char *argv_concat(const char **, int, int);
vector cmd_make_strvec(const char *);
void cmd_free_strvec(vector);
vector cmd_describe_command();
char **cmd_complete_command();
const char *cmd_prompt(enum node_type);
int config_from_file(struct vty *, FILE *);
int external_config_password();
int internal_config_password();
int cmd_execute_command(vector, struct vty *, struct cmd_element **, int);
int cmd_execute_command_strict(vector, struct vty *, struct cmd_element **);
void config_replace_string(struct cmd_element *, char *, ...);
void cmdline_runtime_env_initialize();
int run_command(struct vty *, char *, uint32_t);

/* Export typical functions. */
extern struct cmd_element config_end_cmd;
extern struct cmd_element config_exit_cmd;
extern struct cmd_element config_quit_cmd;
extern struct cmd_element config_help_cmd;
extern struct cmd_element config_list_cmd;
extern struct cmd_element echo_cmd;

char *host_config_file();
void host_config_set(char *);

void print_version(const char *);

void common_cmd_initialize();

#endif /* _ZEBRA_COMMAND_H */
