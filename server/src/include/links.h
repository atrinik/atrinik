/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

#ifndef LINKS_H
#define LINKS_H

#include <decls.h>

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/server/links.c. */

extern mempool_struct *pool_objectlink;

extern void objectlink_init(void);

extern void objectlink_deinit(void);

extern objectlink *get_objectlink(void);

extern void free_objectlink(objectlink *ol);

extern void free_objectlinkpt(objectlink *obp);

extern objectlink *objectlink_link(objectlink **startptr,
                                   objectlink **endptr,
                                   objectlink *afterptr,
                                   objectlink *beforeptr,
                                   objectlink *objptr);

extern objectlink *
objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr);

#endif
