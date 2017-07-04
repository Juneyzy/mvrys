/*
 * $Id: log.c,v 1.20.2.3 2005/01/23 20:45:17 hasso Exp $
 *
 * Logging of zebra
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

#include "zebra.h"

#include "log.h"
#include "memory.h"
#include "command.h"
/* BSD_HANAN
#ifdef LINUX
#include <sys/un.h>
#endif
*/



/* Message lookup function. */
const char *
lookup(struct message *mes, int key)
{
    struct message *pnt;
    
    for (pnt = mes; pnt->key != 0; pnt++)
        if (pnt->key == key)
            return pnt->str;
            
    return "";
}

/* Wrapper around strerror to handle case where it returns NULL. */
const char *
safe_strerror(int errnum)
{
    const char *s = strerror(errnum);
    return (s != NULL) ? s : "Unknown error";
}
