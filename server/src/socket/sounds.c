/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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

#include <global.h>
#include <sproto.h>
#include <sounds.h>
#include <sockproto.h>

/* This is only used for new client/server sound.  If the sound source
 * on the map is farther away than this, we don't sent it to the client. */
#define MAX_SOUND_DISTANCE 12
#define MAX_SOUND_DISTANCE_SQUARED POW2(MAX_SOUND_DISTANCE)

void play_sound_player_only(player *pl, int soundnum, int soundtype, int x, int y)
{
    SockList sl;
	char buf[32];

	/* player has disabled sound */
    if (!pl->socket.sound)
		return;

    sl.buf = (unsigned char *) buf;

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_SOUND);

#if 0
    strcpy((char*)sl.buf, "sound ");
    sl.len = strlen((char*)sl.buf);
#endif

    SockList_AddChar(&sl, (char)x);
    SockList_AddChar(&sl, (char)y);
    SockList_AddShort(&sl, (uint16)soundnum);
    SockList_AddChar(&sl, (char)soundtype);
    Send_With_Handling(&pl->socket, &sl);
}

/* Plays some sound on map at x, y using a distance counter.
 * This is now nicely optimized - we use the player map list
 * and testing only the main map and the possible 8 attached maps.
 * Now, we don't must fear about increasing performance lose with
 * high player numbers. mt - 04.02.04
 * the function looks a bit bloated, but for speed reasons, we just
 * cloned all the 8 loops with native settings for each direction. */
void play_sound_map(mapstruct *map, int x, int y, int sound_num, int sound_type)
{
	int xt, yt;
	object *tmp;

	if (!map || map->in_memory != MAP_IN_MEMORY)
		return;

	/* any player on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, x - tmp->x, y - tmp->y);
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);
		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, x - tmp->x, yt - tmp->y);
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, y - tmp->y);
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, x - tmp->x, yt - tmp->y);
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[3]);
		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, y - tmp->y);
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[4]->player_first;tmp;tmp=CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, yt - tmp->y);
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, yt - tmp->y);
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, yt - tmp->y);
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);
		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= MAX_SOUND_DISTANCE_SQUARED)
				play_sound_player_only(CONTR(tmp), sound_num, sound_type, xt - tmp->x, yt - tmp->y);
		}
	}
}
