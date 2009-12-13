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
 * Functions related to relationship management. */

#include <global.h>

static int is_friendly(const object *op);
static int is_valid_friendly(object *op);

/**
 * Add a new friendly object to the linked list of friendly objects.
 * @param op Object to add to the list. */
void add_friendly_object(object *op)
{
	objectlink *ol;

	if (!is_valid_friendly(op))
	{
		return;
	}

	/* Add some error checking. This shouldn't happen, but the friendly
	 * object list usually isn't very long, and remove_friendly_object
	 * won't remove it either. Plus, it is easier to put a breakpoint in
	 * the debugger here and see where the problem is happening. */
	if (is_friendly(op))
	{
		LOG(llevBug, "BUG: add_friendly_object(): Trying to add object already on list (%s)\n", query_name(op, NULL));
		return;
	}

	ol = get_objectlink();

	ol->objlink.ob = op;
	ol->id = op->count;
	objectlink_link(&first_friendly_object, NULL, NULL, first_friendly_object, ol);
}

/**
 * Removes the specified object from the linked list of friendly objects.
 * @param op Object to remove from list. */
void remove_friendly_object(object *op)
{
	objectlink *ol;

	if (!is_valid_friendly(op))
	{
		return;
	}

	CLEAR_FLAG(op, FLAG_FRIENDLY);

	if (!first_friendly_object)
	{
		LOG(llevBug, "BUG: remove_friendly_object called with empty friendly list, remove ob=%s\n", query_name(op, NULL));
		return;
	}

	for (ol = first_friendly_object; ol; ol = ol->next)
	{
		if (ol->objlink.ob == op)
		{
			objectlink_unlink(&first_friendly_object, NULL, ol);
			return_poolchunk(ol, pool_objectlink);
			break;
		}
	}
}

/**
 * Dumps all friendly objects. Invoked in DM-mode with
 * /dumpfriendlyobjects command. */
void dump_friendly_objects()
{
	objectlink *ol;

	for (ol = first_friendly_object; ol != NULL; ol = ol->next)
	{
		LOG(llevInfo, "%s (count: %d)\n", query_name(ol->objlink.ob, NULL), ol->objlink.ob->count);
	}
}

/**
 * Checks if the given object is already in the friendly list or not.
 * @param op Object to check.
 * @return 1 if on friendly list, 0 otherwise. */
static int is_friendly(const object *op)
{
	objectlink *ol;

	for (ol = first_friendly_object; ol != NULL; ol = ol->next)
	{
		if (ol->objlink.ob == op)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Check whether the passed object is a valid object that can be in the
 * friendly list.
 * @param op The object to check.
 * @return 1 if it is valid, 0 otherwise. */
static int is_valid_friendly(object *op)
{
	/* Only players/monsters can be added to the friendly list. */
	if (op->type != PLAYER && op->type != MONSTER && !QUERY_FLAG(op, FLAG_ALIVE) && !QUERY_FLAG(op, FLAG_MONSTER))
	{
		LOG(llevDebug, "DEBUG: add_friendly_object(): Called for non monster/player (%s - %s)\n", query_name(op, NULL), op->arch->name);
		return 0;
	}

	/* Special case is that player can or not be friendly. If in the
	 * login process they haven't been marked as friendly yet. */
	if (op->type == PLAYER && !QUERY_FLAG(op, FLAG_FRIENDLY))
	{
		return 0;
	}

	return 1;
}
