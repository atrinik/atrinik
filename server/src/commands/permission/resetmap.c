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
 * Implements the /resetmap command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_resetmap(object *op, const char *command, char *params)
{
	mapstruct *m;
	shstr *path;
	object *tmp, *next, **players;
	size_t players_num, i;
	int flags;

	if (!params)
	{
		m = has_been_loaded_sh(op->map->path);
	}
	else
	{
		shstr *mapfile_sh;

		mapfile_sh = add_string(params);
		m = has_been_loaded_sh(mapfile_sh);
		free_string_shared(mapfile_sh);
	}

	if (!m)
	{
		draw_info(COLOR_WHITE, op, "No such map.");
		return;
	}

	if (MAP_UNIQUE(m) && MAP_NOSAVE(m))
	{
		draw_info(COLOR_WHITE, op, "Cannot reset unique no-save map.");
		return;
	}

	if (!strncmp(m->path, "/random/", 8))
	{
		draw_info(COLOR_WHITE, op, "Cannot reset a random map.");
		return;
	}

	if (m->in_memory != MAP_IN_MEMORY)
	{
		logger_print(LOG(BUG), "Tried to swap out map which was not in memory.");
		return;
	}

	draw_info_format(COLOR_WHITE, op, "Start resetting map %s.", m->path);

	players = NULL;
	players_num = 0;

	for (tmp = m->player_first; tmp; tmp = next)
	{
		next = CONTR(tmp)->map_above;

		leave_map(tmp);
		players = realloc(players, sizeof(*players) * (players_num + 1));
		players[players_num] = tmp;
		players_num++;
	}

	draw_info_format(COLOR_WHITE, op, "Removed %"FMT64U" players from map. Reset map.", (uint64) players_num);
	m->reset_time = seconds();
	m->map_flags |= MAP_FLAG_FIXED_RTIME;
	/* Store the path, so we can load it after swapping is done. */
	path = add_refcount(m->path);
	flags = MAP_NAME_SHARED | (MAP_UNIQUE(m) ? MAP_PLAYER_UNIQUE : 0);
	swap_map(m, 1);

	m = ready_map_name(path, flags);
	free_string_shared(path);
	draw_info(COLOR_WHITE, op, "Resetmap done.");

	for (i = 0; i < players_num; i++)
	{
		insert_ob_in_map(players[i], m, NULL, INS_NO_MERGE);

		if (players[i] != op)
		{
			draw_info(COLOR_WHITE, players[i], "Your surroundings seem different but still familiar. Haven't you been here before?");
		}
	}

	if (players)
	{
		free(players);
	}
}
