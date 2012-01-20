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
 * Implements the /no_shout command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_no_shout(object *op, const char *command, char *params)
{
	player *pl;

	if (!params)
	{
		draw_info(COLOR_WHITE, op, "Usage: /no_shout <player>");
		return;
	}

	pl = find_player(params);

	if (!pl)
	{
		draw_info(COLOR_WHITE, op, "No such player.");
		return;
	}

	if (pl->no_shout)
	{
		draw_info_format(COLOR_WHITE, op, "%s is able to shout again.", pl->ob->name);
		pl->no_shout = 0;
	}
	else
	{
		draw_info_format(COLOR_WHITE, op, "%s is now not able to shout.", pl->ob->name);
		pl->no_shout = 1;
	}
}
