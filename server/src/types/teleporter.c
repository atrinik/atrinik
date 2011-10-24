/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Handles code used by @ref TELEPORTER "teleporters". */

#include <global.h>

/**
 * Teleporter does its tick.
 *
 * A teleporter will teleport any object on the tile node it is on,
 * including multi arch objects which are with one part on the
 * teleporter, unless the object has @ref FLAG_NO_TELEPORT set.
 * @param op The teleporter. */
void move_teleporter(object *op)
{
	object *tmp, *next;

	/* Sanity check */
	if (!op->map)
	{
		return;
	}

	/* get first object of this map node */
	for (tmp = GET_BOTTOM_MAP_OB(op); tmp != NULL; tmp = next)
	{
		next = tmp->above;

		if (QUERY_FLAG(tmp, FLAG_NO_TELEPORT))
		{
			continue;
		}

		/* teleport to different map */
		if (EXIT_PATH(op))
		{
			/* Trigger the TRIGGER event */
			if (trigger_event(EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
			{
				return;
			}

			enter_exit(tmp, op);
		}
		/* teleport inside this map */
		else if (EXIT_X(op) != -1 && EXIT_Y(op) != -1)
		{
			/* use OUT_OF_REAL_MAP() - we want be truly on THIS map */
			if (OUT_OF_REAL_MAP(op->map, EXIT_X(op), EXIT_Y(op)))
			{
				LOG(llevBug, "Removed illegal teleporter (map: %s (%d,%d)) -> (%d,%d)\n", op->map->name, op->x, op->y, EXIT_X(op), EXIT_Y(op));
				remove_ob(op);
				object_destroy(op);
				return;
			}

			/* Trigger the TRIGGER event */
			if (trigger_event(EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
			{
				return;
			}

			transfer_ob(tmp, EXIT_X(op), EXIT_Y(op), 0, op, NULL);
		}
		/* Random teleporter */
		else
		{
			/* Trigger the TRIGGER event */
			if (trigger_event(EVENT_TRIGGER, op, tmp, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
			{
				return;
			}

			teleport(op, TELEPORTER, tmp);
		}
	}
}
