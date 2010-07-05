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
 * @param type Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename Name of the sound to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume)
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
	uint16 sound_num;

	/* No reason to send anything other than sound effects to older
	 * clients, they don't support it anyway... */
	if (type != CMD_SOUND_EFFECT)
	{
		return;
	}

	if (!strcmp(filename, "event01.ogg") || !strcmp(filename, "magic_default.ogg"))
	{
		sound_num = 0;
	}
	else if (!strcmp(filename, "bow1.ogg") || !strcmp(filename, "magic_acid.ogg"))
	{
		sound_num = 1;
	}
	else if (!strcmp(filename, "learnspell.ogg") || !strcmp(filename, "magic_animate.ogg"))
	{
		sound_num = 2;
	}
	else if (!strcmp(filename, "missspell.ogg") || !strcmp(filename, "magic_avatar.ogg"))
	{
		sound_num = 3;
	}
	else if (!strcmp(filename, "rod.ogg") || !strcmp(filename, "magic_bomb.ogg"))
	{
		sound_num = 4;
	}
	else if (!strcmp(filename, "door.ogg") || !strcmp(filename, "magic_bullet1.ogg"))
	{
		sound_num = 5;
	}
	else if (!strcmp(filename, "push.ogg") || !strcmp(filename, "magic_bullet2.ogg"))
	{
		sound_num = 6;
	}
	else if (!strcmp(filename, "hit_impact.ogg") || !strcmp(filename, "magic_cancel.ogg"))
	{
		sound_num = 7;
	}
	else if (!strcmp(filename, "hit_cleave.ogg") || !strcmp(filename, "magic_comet.ogg"))
	{
		sound_num = 8;
	}
	else if (!strcmp(filename, "hit_slash.ogg") || !strcmp(filename, "magic_confusion.ogg"))
	{
		sound_num = 9;
	}
	else if (!strcmp(filename, "hit_pierce.ogg") || !strcmp(filename, "magic_create.ogg"))
	{
		sound_num = 10;
	}
	else if (!strcmp(filename, "hit_block.ogg") || !strcmp(filename, "magic_dark.ogg"))
	{
		sound_num = 11;
	}
	else if (!strcmp(filename, "hit_hand.ogg") || !strcmp(filename, "magic_death.ogg"))
	{
		sound_num = 12;
	}
	else if (!strcmp(filename, "miss_mob1.ogg") || !strcmp(filename, "magic_destruction.ogg"))
	{
		sound_num = 13;
	}
	else if (!strcmp(filename, "miss_player1.ogg") || !strcmp(filename, "magic_elec.ogg"))
	{
		sound_num = 14;
	}
	else if (!strcmp(filename, "petdead.ogg") || !strcmp(filename, "magic_fear.ogg"))
	{
		sound_num = 15;
	}
	else if (!strcmp(filename, "playerdead.ogg") || !strcmp(filename, "magic_fire.ogg"))
	{
		sound_num = 16;
	}
	else if (!strcmp(filename, "explosion.ogg") || !strcmp(filename, "magic_fireball1.ogg"))
	{
		sound_num = 17;
	}
	else if (!strcmp(filename, "magic_fireball2.ogg"))
	{
		sound_num = 18;
	}
	else if (!strcmp(filename, "kill.ogg") || !strcmp(filename, "magic_hword.ogg"))
	{
		sound_num = 19;
	}
	else if (!strcmp(filename, "pull.ogg") || !strcmp(filename, "magic_ice.ogg"))
	{
		sound_num = 20;
	}
	else if (!strcmp(filename, "fallhole.ogg") || !strcmp(filename, "magic_invisible.ogg"))
	{
		sound_num = 21;
	}
	else if (!strcmp(filename, "poison.ogg") || !strcmp(filename, "magic_invoke.ogg"))
	{
		sound_num = 22;
	}
	else if (!strcmp(filename, "drop.ogg") || !strcmp(filename, "magic_invoke2.ogg"))
	{
		sound_num = 23;
	}
	else if (!strcmp(filename, "lose_some.ogg") || !strcmp(filename, "magic_magic.ogg"))
	{
		sound_num = 24;
	}
	else if (!strcmp(filename, "throw.ogg") || !strcmp(filename, "magic_manaball.ogg"))
	{
		sound_num = 25;
	}
	else if (!strcmp(filename, "gate_open.ogg") || !strcmp(filename, "magic_missile.ogg"))
	{
		sound_num = 26;
	}
	else if (!strcmp(filename, "gate_close.ogg") || !strcmp(filename, "magic_mmap.ogg"))
	{
		sound_num = 27;
	}
	else if (!strcmp(filename, "open_container.ogg") || !strcmp(filename, "magic_orb.ogg"))
	{
		sound_num = 28;
	}
	else if (!strcmp(filename, "growl.ogg") || !strcmp(filename, "magic_paralyze.ogg"))
	{
		sound_num = 29;
	}
	else if (!strcmp(filename, "arrow_hit.ogg") || !strcmp(filename, "magic_poison.ogg"))
	{
		sound_num = 30;
	}
	else if (!strcmp(filename, "door_close.ogg") || !strcmp(filename, "magic_protection.ogg"))
	{
		sound_num = 31;
	}
	else if (!strcmp(filename, "teleport.ogg") || !strcmp(filename, "magic_rstrike.ogg"))
	{
		sound_num = 32;
	}
	else if (!strcmp(filename, "scroll.ogg") || !strcmp(filename, "magic_rune.ogg"))
	{
		sound_num = 33;
	}
	else if (!strcmp(filename, "magic_sball.ogg"))
	{
		sound_num = 34;
	}
	else if (!strcmp(filename, "magic_slow.ogg"))
	{
		sound_num = 35;
	}
	else if (!strcmp(filename, "magic_snowstorm.ogg"))
	{
		sound_num = 36;
	}
	else if (!strcmp(filename, "magic_stat.ogg"))
	{
		sound_num = 37;
	}
	else if (!strcmp(filename, "magic_steambolt.ogg"))
	{
		sound_num = 38;
	}
	else if (!strcmp(filename, "magic_summon1.ogg"))
	{
		sound_num = 39;
	}
	else if (!strcmp(filename, "magic_summon2.ogg"))
	{
		sound_num = 40;
	}
	else if (!strcmp(filename, "magic_summon3.ogg"))
	{
		sound_num = 41;
	}
	else if (!strcmp(filename, "magic_teleport.ogg"))
	{
		sound_num = 42;
	}
	else if (!strcmp(filename, "magic_turn.ogg"))
	{
		sound_num = 43;
	}
	else if (!strcmp(filename, "magic_wall.ogg"))
	{
		sound_num = 44;
	}
	else if (!strcmp(filename, "magic_walls.ogg"))
	{
		sound_num = 45;
	}
	else if (!strcmp(filename, "magic_wound.ogg"))
	{
		sound_num = 46;
	}
	else
	{
		return;
	}

	SockList_AddChar(&sl, (char) x);
	SockList_AddChar(&sl, (char) y);
	SockList_AddShort(&sl, sound_num);

	if (!strncmp(filename, "magic_", 6))
	{
		SockList_AddChar(&sl, 1);
	}
	else
	{
		SockList_AddChar(&sl, 0);
	}
}
else
{
	SockList_AddChar(&sl, (char) type);
	SockList_AddString(&sl, (char *) filename);
	SockList_AddChar(&sl, loop);
	SockList_AddChar(&sl, volume);

	/* Add X/Y offset for sound effects. */
	if (type == CMD_SOUND_EFFECT)
	{
		SockList_AddChar(&sl, (char) x);
		SockList_AddChar(&sl, (char) y);
	}
}

	Send_With_Handling(&pl->socket, &sl);
}

/**
 * Plays a sound on a map.
 *
 * Considers tiled maps.
 * @param map Map to play the sound on.
 * @param type Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename Name of the sound to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume)
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
				play_sound_player_only(CONTR(tmp), type, filename, x - tmp->x, y - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, x - tmp->x, yt - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, y - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, x - tmp->x, yt - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, y - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, yt - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, yt - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, yt - tmp->y, loop, volume);
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
				play_sound_player_only(CONTR(tmp), type, filename, xt - tmp->x, yt - tmp->y, loop, volume);
			}
		}
	}
}
