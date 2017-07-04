/* Zebra common header.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002 Kunihiro Ishiguro

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

#ifndef _ZEBRA_H
#define _ZEBRA_H

#define HAVE_CONFIG_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define __EXTENSIONS__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/uio.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#define CLI_MAJOR_VER       2
#define CLI_MINOR_VER       0

#define MANUFACTURE     "SEMPTIAN"


/* machine dependent includes */
#ifdef HAVE_LINUX_VERSION_H
#include <linux/version.h>
#endif /* HAVE_LINUX_VERSION_H */

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif /* HAVE_ASM_TYPES_H */

/* misc include group */
#include <stdarg.h>
#if !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
/* Not C99; do we need to define va_copy? */
#ifndef va_copy
#ifdef __va_copy
#define va_copy(DST,SRC) __va_copy(DST,SRC)
#else
/* Now we are desperate; this should work on many typical platforms.
   But this is slightly dangerous, because the standard does not require
   va_copy to be a macro. */
#define va_copy(DST,SRC) memcpy(&(DST), &(SRC), sizeof(va_list))

#endif /* __va_copy */
#endif /* !va_copy */
#endif /* !C99 */


#ifdef HAVE_LCAPS
#include <sys/capability.h>
#include <sys/prctl.h>
#endif /* HAVE_LCAPS */

/* Some systems do not define UINT32_MAX */
#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFFU
#endif /* UINT32_MAX */

/* Local includes: */
#if !(defined(__GNUC__) || defined(VTYSH_EXTRACT_PL))
#define __attribute__(x)
#endif  /* !__GNUC__ || VTYSH_EXTRACT_PL */

#include "zassert.h"

/* MAX / MIN are not commonly defined, but useful */
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* For old definition. */
#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL IN6_IS_ADDR_EQUAL
#endif /* IN6_ARE_ADDR_EQUAL */

/* Zebra message types. */
#define ZEBRA_INTERFACE_ADD                1
#define ZEBRA_INTERFACE_DELETE             2
#define ZEBRA_INTERFACE_ADDRESS_ADD        3
#define ZEBRA_INTERFACE_ADDRESS_DELETE     4
#define ZEBRA_INTERFACE_UP                 5
#define ZEBRA_INTERFACE_DOWN               6
#define ZEBRA_IPV4_ROUTE_ADD               7
#define ZEBRA_IPV4_ROUTE_DELETE            8
#define ZEBRA_IPV6_ROUTE_ADD               9
#define ZEBRA_IPV6_ROUTE_DELETE           10
#define ZEBRA_REDISTRIBUTE_ADD            11
#define ZEBRA_REDISTRIBUTE_DELETE         12
#define ZEBRA_REDISTRIBUTE_DEFAULT_ADD    13
#define ZEBRA_REDISTRIBUTE_DEFAULT_DELETE 14
#define ZEBRA_IPV4_NEXTHOP_LOOKUP         15
#define ZEBRA_IPV6_NEXTHOP_LOOKUP         16
#define ZEBRA_IPV4_IMPORT_LOOKUP          17
#define ZEBRA_IPV6_IMPORT_LOOKUP          18
#define ZEBRA_INTERFACE_RENAME            19
#define ZEBRA_ROUTER_ID_ADD               20
#define ZEBRA_ROUTER_ID_DELETE            21
#define ZEBRA_ROUTER_ID_UPDATE            22
#define ZEBRA_MESSAGE_MAX                 23

/* Zebra route's types. */
#define ZEBRA_ROUTE_SYSTEM               0
#define ZEBRA_ROUTE_KERNEL               1
#define ZEBRA_ROUTE_CONNECT              2
#define ZEBRA_ROUTE_STATIC               3
#define ZEBRA_ROUTE_RIP                  4
#define ZEBRA_ROUTE_RIPNG                5
#define ZEBRA_ROUTE_OSPF                 6
#define ZEBRA_ROUTE_OSPF6                7
#define ZEBRA_ROUTE_ISIS                 8
#define ZEBRA_ROUTE_BGP                  9
#define ZEBRA_ROUTE_HSLS         10
#define ZEBRA_ROUTE_MAX                  11

/* Zebra's family types. */
#define ZEBRA_FAMILY_IPV4                1
#define ZEBRA_FAMILY_IPV6                2
#define ZEBRA_FAMILY_MAX                 3

/* Error codes of zebra. */
#define ZEBRA_ERR_RTEXIST               -1
#define ZEBRA_ERR_RTUNREACH             -2
#define ZEBRA_ERR_EPERM                 -3
#define ZEBRA_ERR_RTNOEXIST             -4

/* Zebra message flags */
#define ZEBRA_FLAG_INTERNAL           0x01
#define ZEBRA_FLAG_SELFROUTE          0x02
#define ZEBRA_FLAG_BLACKHOLE          0x04
#define ZEBRA_FLAG_IBGP               0x08
#define ZEBRA_FLAG_SELECTED           0x10
#define ZEBRA_FLAG_CHANGED            0x20
#define ZEBRA_FLAG_STATIC             0x40
#define ZEBRA_FLAG_REJECT             0x80

/* Zebra nexthop flags. */
#define ZEBRA_NEXTHOP_IFINDEX            1
#define ZEBRA_NEXTHOP_IFNAME             2
#define ZEBRA_NEXTHOP_IPV4               3
#define ZEBRA_NEXTHOP_IPV4_IFINDEX       4
#define ZEBRA_NEXTHOP_IPV4_IFNAME        5
#define ZEBRA_NEXTHOP_IPV6               6
#define ZEBRA_NEXTHOP_IPV6_IFINDEX       7
#define ZEBRA_NEXTHOP_IPV6_IFNAME        8
#define ZEBRA_NEXTHOP_BLACKHOLE          9

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001  /* Internet address 127.0.0.1.  */
#endif

/* Address family numbers from RFC1700. */
#define AFI_IP                    1
#define AFI_IP6                   2
#define AFI_MAX                   3

/* Subsequent Address Family Identifier. */
#define SAFI_UNICAST              1
#define SAFI_MULTICAST            2
#define SAFI_UNICAST_MULTICAST    3
#define SAFI_MPLS_VPN             4
#define SAFI_MAX                  5

/* Filter direction.  */
#define FILTER_IN                 0
#define FILTER_OUT                1
#define FILTER_MAX                2

/* Default Administrative Distance of each protocol. */
#define ZEBRA_KERNEL_DISTANCE_DEFAULT      0
#define ZEBRA_CONNECT_DISTANCE_DEFAULT     0
#define ZEBRA_STATIC_DISTANCE_DEFAULT      1
#define ZEBRA_RIP_DISTANCE_DEFAULT       120
#define ZEBRA_RIPNG_DISTANCE_DEFAULT     120
#define ZEBRA_OSPF_DISTANCE_DEFAULT      110
#define ZEBRA_OSPF6_DISTANCE_DEFAULT     110
#define ZEBRA_ISIS_DISTANCE_DEFAULT      115
#define ZEBRA_IBGP_DISTANCE_DEFAULT      200
#define ZEBRA_EBGP_DISTANCE_DEFAULT       20

/* Flag manipulation macros. */
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V,F)        (V) = (V) | (F)
#define UNSET_FLAG(V,F)      (V) = (V) & ~(F)

/* Changed by Tsihang, Semptian.inc. In order to enable compilation in Windows and LINUX */
/* AFI and SAFI type. */
typedef unsigned short afi_t;
/* typedef u_int8_t  safi_t; */

/* Zebra types. */
/* typedef u_int16_t zebra_size_t; */
/* typedef u_int8_t  zebra_command_t; */

/* FIFO -- first in first out structure and macros.  */
struct fifo
{
    struct fifo *next;
    struct fifo *prev;
};

#define FIFO_INIT(F)                                  \
    do {                                                \
        struct fifo *Xfifo = (struct fifo *)(F);          \
        Xfifo->next = Xfifo->prev = Xfifo;                \
    } while (0)

#define FIFO_ADD(F,N)                                 \
    do {                                                \
        struct fifo *Xfifo = (struct fifo *)(F);          \
        struct fifo *Xnode = (struct fifo *)(N);          \
        Xnode->next = Xfifo;                              \
        Xnode->prev = Xfifo->prev;                        \
        Xfifo->prev = Xfifo->prev->next = Xnode;          \
    } while (0)

#define FIFO_DEL(N)                                   \
    do {                                                \
        struct fifo *Xnode = (struct fifo *)(N);          \
        Xnode->prev->next = Xnode->next;                  \
        Xnode->next->prev = Xnode->prev;                  \
    } while (0)

#define FIFO_HEAD(F)                                  \
    ((((struct fifo *)(F))->next == (struct fifo *)(F)) \
     ? NULL : (F)->next)

#define FIFO_EMPTY(F)                                 \
    (((struct fifo *)(F))->next == (struct fifo *)(F))

#define FIFO_TOP(F)                                   \
    (FIFO_EMPTY(F) ? NULL : ((struct fifo *)(F))->next)



#define __USER_AUTH__       /* add by tsihang for auth user */
#define __VTY_NO_CLOSE__    /* add by tsihang for without exitting vty when execut command "exit" */
//#define __LOG_MODE_OPEN__ /* add by tsihang for running with zlog */
#define __EXTERNAL_LOGIN__  /* add by tsihang for external login */
//#undef __EXTERNAL_LOGIN__
#endif /* _ZEBRA_H */
