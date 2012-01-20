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
 * Implements the /tp command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_tp(object *op, const char *command, char *params)
{
	char word[MAX_BUF];
	size_t pos;
	player *dst, *who;

	pos = 0;

	if (!string_get_word(params, &pos, word, sizeof(word)))
	{
		draw_info(COLOR_WHITE, op, "Usage: /tp <dst> [who]");
		return;
	}

	dst = find_player(word);

	if (string_get_word(params, &pos, word, sizeof(word)))
	{
		who = find_player(word);
	}
	else
	{
		who = CONTR(op);
	}

	if (!dst || !who)
	{
		draw_info(COLOR_WHITE, op, "No such player.");
		return;
	}

	if (dst == who)
	{
		draw_info(COLOR_WHITE, op, "You cannot teleport yourself to yourself.");
		return;
	}

	draw_info_format(COLOR_WHITE, op, "Teleporting %s to %s.", who->ob->name, dst->ob->name);

	object_remove(who->ob, 0);
	who->ob->x = dst->ob->x;
	who->ob->y = dst->ob->y;
	insert_ob_in_map(who->ob, dst->ob->map, NULL, INS_NO_MERGE);
}
