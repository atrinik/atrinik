/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

#ifndef ARCH_H
#define ARCH_H

/**
 * @file arch.h
 * Arch related structures */

/** The archetype structure is a set of rules on how to generate and manipulate
 * objects which point to archetypes.
 * This structure should get removed, and just replaced
 * by the object structure */
typedef struct archt {
	/** More definite name, like "generate_kobold" */
    const char *name;

	/** Next archetype in a linked list */
    struct archt *next;

	/** The main part of a linked object */
    struct archt *head;

	/** Next part of a linked object */
    struct archt *more;

	/** used by artifacts list: if != NULL,
	 * this object is the base object and clone is
	 * the modified artifacts object.
	 * we use base_clone for unidentified objects
	 * (to get unified "non identified" values),
	 * or it is used to get a base object when we
	 * remove the artifacts changes (cancellation, dispel...) */
	object *base_clone;

	/** An object from which to do copy_object() */
    object clone;
} archetype;

EXTERN archetype *first_archetype;

#endif
