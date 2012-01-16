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
 * Implements the /drop command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_drop(object *op, const char *command, char *params)
{
	object *tmp, *next;
	int did_one = 0, missed = 0, ival;

	if (!params)
	{
		draw_info(COLOR_WHITE, op, "Drop what?");
		return;
	}

	if (op->map && op->map->events && trigger_map_event(MEVENT_CMD_DROP, op->map, op, NULL, NULL, params, 0))
	{
		return;
	}

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);

	for (tmp = op->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (QUERY_FLAG(tmp, FLAG_NO_DROP) || QUERY_FLAG(tmp, FLAG_STARTEQUIP) || IS_INVISIBLE(tmp, op))
		{
			continue;
		}

		ival = item_matched_string(op, tmp, params);

		if (ival > 0)
		{
			if (ival <= 2 && QUERY_FLAG(tmp, FLAG_INV_LOCKED))
			{
				missed++;
			}
			else
			{
				drop(op, tmp, 1);
				did_one = 1;
			}
		}
	}

	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

	if (did_one)
	{
		fix_player(op);
	}
	else if (!missed)
	{
		draw_info(COLOR_WHITE, op, "Nothing to drop.");
	}

	if (missed == 1)
	{
		draw_info(COLOR_WHITE, op, "One item couldn't be dropped because it was locked.");
	}
	else if (missed > 1)
	{
		draw_info_format(COLOR_WHITE, op, "%d items couldn't be dropped because they were locked.", missed);
	}

	CONTR(op)->count = 0;
}
