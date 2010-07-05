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
 * Sound related functions. */

#include <global.h>
#include <sounds.h>

/**
 * Maximum distance a player may hear a sound from.
 *
 * This is only used for new client/server sound.  If the sound source
 * on the map is farther away than this, we don't sent it to the client. */
#define MAX_SOUND_DISTANCE 12
/** Squared maximum sound distance, using POW2. */
#define MAX_SOUND_DISTANCE_SQUARED POW2(MAX_SOUND_DISTANCE)

/**
 * Plays a sound for specified player only.
 * @param pl Player to play sound to.
 * @param sound_num ID of the sound, one of ::sound_ids_enum.
 * @param filename If passed, play this background music instead of the
 * above 'sound_num'.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
void play_sound_player_only(player *pl, int sound_num, const char *filename, int x, int y, int loop, int volume)
{
	SockList sl;
	unsigned char buf[MAX_BUF];

	/* Player has disabled sound */
	if (!pl->socket.sound)
	{
		return;
	}

	sl.buf = buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_SOUND);

	if (pl->socket.socket_version < 1038)
	{
		SockList_AddChar(&sl, (char) x);
		SockList_AddChar(&sl, (char) y);

		if (sound_num < SOUND_MAGIC_DEFAULT)
		{
			SockList_AddShort(&sl, (uint16) sound_num);
			SockList_AddChar(&sl, 0);
		}
		else
		{
			SockList_AddShort(&sl, (uint16) sound_num - SOUND_MAGIC_DEFAULT);
			SockList_AddChar(&sl, 1);
		}
	}
	else
	{
	if (!filename || *filename == '\0')
	{
		SockList_AddChar(&sl, (char) CMD_SOUND_EFFECT);
		SockList_AddChar(&sl, (char) x);
		SockList_AddChar(&sl, (char) y);
		SockList_AddShort(&sl, (uint16) sound_num);
	}
	else
	{
		SockList_AddChar(&sl, (char) CMD_SOUND_BACKGROUND);
		SockList_AddString(&sl, (char *) filename);
	}

	SockList_AddChar(&sl, loop);
	SockList_AddChar(&sl, volume);
	}

	Send_With_Handling(&pl->socket, &sl);
}

/**
 * Plays a sound on a map.
 *
 * Considers tiled maps.
 * @param map Map to play the sound on.
 * @param sound_num ID of the sound, one of ::sound_ids_enum.
 * @param filename If passed, play this background music instead of the
 * above 'sound_num'.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
void play_sound_map(mapstruct *map, int sound_num, const char *filename, int x, int y, int loop, int volume)
{
	int xt, yt;
	object *tmp;

	if (!map || map->in_memory != MAP_IN_MEMORY)
	{
		return;
	}

	/* any player on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, x - tmp->x, y - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);

		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, x - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, y - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, x - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[3]);

		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, y - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[4]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);

		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
			{
				play_sound_player_only(CONTR(tmp), sound_num, filename, xt - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}
}
