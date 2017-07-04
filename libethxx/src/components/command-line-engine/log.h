/*
 * $Id: log.h,v 1.14.2.3 2005/01/23 20:45:17 hasso Exp $
 *
 * Zebra logging funcions.
 * Copyright (C) 1997, 1998, 1999 Kunihiro Ishiguro
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

#ifndef _ZEBRA_LOG_H
#define _ZEBRA_LOG_H


/* Here is some guidance on logging levels to use:
 *
 * LOG_DEBUG    - For all messages that are enabled by optional debugging
 *        features, typically preceded by "if (IS...DEBUG...)"
 * LOG_INFO - Information that may be of interest, but everything seems
 *        to be working properly.
 * LOG_NOTICE   - Only for message pertaining to daemon startup or shutdown.
 * LOG_WARNING  - Warning conditions: unexpected events, but the daemon believes
 *        it can continue to operate correctly.
 * LOG_ERR  - Error situations indicating malfunctions.  Probably require
 *        attention.
 *
 * Note: LOG_CRIT, LOG_ALERT, and LOG_EMERG are currently not used anywhere,
 * please use LOG_ERR instead.
 */
/* Message structure. */
struct message
{
    int key;
    const char *str;
};

#include <stdio.h>

const char *lookup(struct message *, int);

/* Safe version of strerror -- never returns NULL. */
extern const char *safe_strerror(int errnum);

#endif /* _ZEBRA_LOG_H */
