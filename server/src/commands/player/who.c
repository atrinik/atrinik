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
 * Implements the /who command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_who(object *op, const char *command, char *params)
{
	player *pl;
	int ip = 0, il = 0;
	char buf[MAX_BUF], race[MAX_BUF];

	draw_info(COLOR_WHITE, op, " ");

	for (pl = first_player; pl; pl = pl->next)
	{
		if (!pl->ob->map)
		{
			il++;
			continue;
		}

		ip++;

		if (pl->state == ST_PLAYING)
		{
			snprintf(buf, sizeof(buf), "%s the %s %s (lvl %d)", pl->ob->name, gender_noun[object_get_gender(pl->ob)], player_get_race_class(pl->ob, race, sizeof(race)), pl->ob->level);

			if (pl->afk)
			{
				strncat(buf, " [AFK]", sizeof(buf) - strlen(buf) - 1);
			}

			if (pl->socket.is_bot)
			{
				strncat(buf, " [BOT]", sizeof(buf) - strlen(buf) - 1);
			}

			if (pl->class_ob && pl->class_ob->title)
			{
				strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
				strncat(buf, pl->class_ob->title, sizeof(buf) - strlen(buf) - 1);
			}

			draw_info(COLOR_WHITE, op, buf);
		}
	}

	draw_info_format(COLOR_WHITE, op, "There %s %d player%s online (%d in login).", ip + il > 1 ? "are" : "is", ip + il, ip + il > 1 ? "s" : "", il);
}
