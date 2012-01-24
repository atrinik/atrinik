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
 * Implements the /t_tell command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_t_tell(object *op, const char *command, char *params)
{
	int i, x, y;
	mapstruct *m;
	object *tmp;

	params = player_sanitize_input(params);

	if (!params)
	{
		return;
	}

	if (OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && OBJECT_CAN_TALK(CONTR(op)->target_object))
	{
		for (i = 0; i <= SIZEOFFREE2; i++)
		{
			x = op->x + freearr_x[i];
			y = op->y + freearr_y[i];

			if (!(m = get_map_from_coord(op->map, &x, &y)))
			{
				continue;
			}

			if (m == CONTR(op)->target_object->map && x == CONTR(op)->target_object->x && y == CONTR(op)->target_object->y)
			{
				logger_print(LOG(CHAT), "[TALKTO] [%s] [%s] %s", op->name, CONTR(op)->target_object->name, params);
				talk_to_npc(op, CONTR(op)->target_object, params);
				return;
			}
		}
	}

	for (i = 0; i <= SIZEOFFREE2; i++)
	{
		x = op->x + freearr_x[i];
		y = op->y + freearr_y[i];

		if (!(m = get_map_from_coord(op->map, &x, &y)))
		{
			continue;
		}

		for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above)
		{
			if (OBJECT_CAN_TALK(tmp))
			{
				logger_print(LOG(CHAT), "[TALKTO] [%s] [%s] %s", op->name, tmp->name, params);

				if (talk_to_npc(op, tmp, params))
				{
					if (CONTR(op)->target_object != tmp || CONTR(op)->target_object_count != tmp->count)
					{
						CONTR(op)->target_object = tmp;
						CONTR(op)->target_object_count = tmp->count;
						send_target_command(CONTR(op));
					}
				}

				return;
			}
		}
	}

	if (OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && OBJECT_CAN_TALK(CONTR(op)->target_object))
	{
		draw_info_format(COLOR_WHITE, op, "You are too far away from %s.", CONTR(op)->target_object->name);
	}
	else
	{
		draw_info(COLOR_WHITE, op, "There are no NPCs that you can talk to nearby.");
	}
}
