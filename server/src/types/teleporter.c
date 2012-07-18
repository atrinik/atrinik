/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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

/**
 * @file
 * Handles code used by @ref TELEPORTER "teleporters".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
	object *tmp, *next;

	if (!op->map)
	{
		return;
	}

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = next)
	{
		next = tmp->above;

		if (QUERY_FLAG(tmp, FLAG_NO_TELEPORT))
		{
			continue;
		}

		if (HAS_EVENT(op, EVENT_TRIGGER))
		{
			int ret;

			ret = trigger_event(EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);

			if (ret == 1)
			{
				return;
			}
			else if (ret == 2)
			{
				continue;
			}
		}

		if (EXIT_PATH(op))
		{
			object_enter_map(tmp, op, NULL, 0, 0, 0);
		}
		else if (EXIT_X(op) != -1 && EXIT_Y(op) != -1)
		{
			if (OUT_OF_MAP(op->map, EXIT_X(op), EXIT_Y(op)))
			{
				return;
			}

			transfer_ob(tmp, EXIT_X(op), EXIT_Y(op), 0, op, NULL);
		}
		else
		{
			teleport(op, TELEPORTER, tmp);
		}
	}
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
	process_func(op);

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the teleporter type object methods. */
void object_type_init_teleporter(void)
{
	object_type_methods[TELEPORTER].process_func = process_func;
	object_type_methods[TELEPORTER].trigger_func = trigger_func;
}
