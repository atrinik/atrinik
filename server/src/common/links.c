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

/**
 * @file
 * Object link related functions. */

#include <global.h>

/**
 * Allocate a new objectlink structure and initialize it.
 * @return Pointer to the new objectlink */
objectlink *get_objectlink()
{
	objectlink *ol = (objectlink *)CALLOC(1, sizeof(objectlink));

	ol->ob = NULL;
	ol->next = NULL;
	ol->id = 0;

	return ol;
}

/**
 * Allocate a new oblinkpt structure and initialize it.
 * @return Pointer to the new oblinkpt */
oblinkpt *get_objectlinkpt()
{
	oblinkpt *obp = (oblinkpt *) malloc(sizeof(oblinkpt));

	obp->link = NULL;
	obp->next = NULL;
	obp->value = 0;

	return obp;
}

/**
 * Recursively free all objectlinks.
 *
 * @warning Only call for lists with FLAG_IS_LINKED - friendly lists
 * and some others handle their own objectlink malloc/free.
 * @param ol The objectlink */
void free_objectlink(objectlink *ol)
{
	if (ol->next)
	{
		free_objectlink(ol->next);
	}

	if (OBJECT_VALID(ol->ob, ol->id))
	{
		CLEAR_FLAG(ol->ob, FLAG_IS_LINKED);
	}

	free(ol);
}

/**
 * Recursively free all linked lists of objectlink pointers
 * @warning Only call for lists with FLAG_IS_LINKED - friendly lists
 * and some others handle their own objectlink malloc/free.
 * @param obp The oblinkpt */
void free_objectlinkpt(oblinkpt *obp)
{
	if (obp->next)
	{
		free_objectlinkpt(obp->next);
	}

	if (obp->link)
	{
		free_objectlink(obp->link);
	}

	free(obp);
}
