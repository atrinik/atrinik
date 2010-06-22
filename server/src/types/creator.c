/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Handles code used for @ref CREATOR "creators". */

#include <global.h>

/**
 * Search for duplicate object on map at x, y.
 *
 * A duplicate object is an object that has same name, type and arch.
 * @param op Object we're checking against.
 * @param map Map to check on.
 * @param x X position.
 * @param y Y position.
 * @return 1 if there is a duplicate object, 0 otherwise. */
static int check_for_duplicate_ob(object *op, mapstruct *map, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB(map, x, y); tmp; tmp = tmp->above)
	{
		if (tmp != op && tmp->name == op->name && tmp->type == op->type && tmp->arch == op->arch)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Have a creator do its tick.
 * @param op The creator.
 * @todo Check if it works properly with multi arch objects.
 * @todo Perhaps if other_arch is set try to check for an object in its
 * inventory, and copy any modified values? */
void move_creator(object *op)
{
	if (op->stats.hp <= 0 && !QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		return;
	}

	/* Create from other_arch */
	if (op->other_arch)
	{
		object *tmp = arch_to_object(op->other_arch);

		if (op->slaying)
		{
			FREE_AND_ADD_REF_HASH(tmp->name, op->slaying);
			FREE_AND_ADD_REF_HASH(tmp->title, op->slaying);
		}

		tmp->x = op->x;
		tmp->y = op->y;
		tmp->level = op->level;

		if (QUERY_FLAG(op, FLAG_ONE_DROP) && check_for_duplicate_ob(tmp, op->map, op->x, op->y))
		{
			return;
		}

		op->stats.hp--;
		insert_ob_in_map(tmp, op->map, op, 0);
	}
	/* Clone from inventory. System objects won't be copied, with an exception for player movers. */
	else if (op->inv)
	{
		object *source, *tmp;
		int didit = 0, cloneindex = 0;

		/* Create single random item from inventory? */
		if (QUERY_FLAG(op, FLAG_SPLITTING))
		{
			int numobs = 0;

			/* Count applicable items */
			for (tmp = op->inv; tmp; tmp = tmp->below)
			{
				if (!QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || tmp->type == PLAYERMOVER)
				{
					numobs++;
				}
			}

			if (numobs == 0)
			{
				return;
			}

			cloneindex = RANDOM() % numobs;
		}

		for (source = op->inv; source && cloneindex >= 0; source = source->below)
		{
			/* Don't clone sys objects */
			if (QUERY_FLAG(source, FLAG_SYS_OBJECT) && source->type != PLAYERMOVER)
			{
				continue;
			}

			/* Count down to target if creating a single random item */
			if (QUERY_FLAG(op, FLAG_SPLITTING) && --cloneindex >= 0)
			{
				continue;
			}

			tmp = object_create_clone(source);
			tmp->x = op->x;
			tmp->y = op->y;

			if (QUERY_FLAG(op, FLAG_ONE_DROP) && check_for_duplicate_ob(tmp, op->map, op->x, op->y))
			{
				continue;
			}

			insert_ob_in_map(tmp, op->map, op, 0);
			didit = 1;
		}

		/* Reduce count if we cloned any object from inventory */
		if (didit)
		{
			op->stats.hp--;
		}
	}
	else
	{
		LOG(llevDebug, "DEBUG: Creator object with no other_arch/inventory: %s (%d, %d)\n", op->map->path, op->x, op->y);
	}
}
