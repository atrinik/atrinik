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
 * Implements the /tpto command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_tpto(object *op, const char *command, char *params)
{
	char path[MAX_BUF], word[MAX_BUF];
	size_t pos;
	int x, y;
	object *dummy;

	params = player_sanitize_input(params);
	pos = 0;

	if (!params || string_get_word(params, &pos, path, sizeof(path)))
	{
		return;
	}

	x = y = -1;

	if (string_get_word(params, &pos, word, sizeof(word)) && string_isdigit(word))
	{
		x = atoi(word);
	}

	if (string_get_word(params, &pos, word, sizeof(word)) && string_isdigit(word))
	{
		y = atoi(word);
	}

	dummy = get_object();
	dummy->map = op->map;
	dummy->stats.hp = x;
	dummy->stats.sp = y;
	FREE_AND_COPY_HASH(EXIT_PATH(dummy), path);
	FREE_AND_COPY_HASH(dummy->name, path);

	enter_exit(op, dummy);

	object_destroy(dummy);
}
