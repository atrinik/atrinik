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

/**
 * @file
 * This file is the one and only DRAWINFO output module.
 *
 * All player communication using drawinfo is handled here - except the
 * few messages we send to the client using DRAWINFO before we had setup
 * any player structure - for example when an outdated client logs in and
 * we send "update your client" directly to the info windows.
 *
 * But if the player is logged in, all DRAWINFO are generated here. */

#include <global.h>
#include <sproto.h>
#include <stdarg.h>

/**
 * Draw a message in the text windows for player's client.
 * @param flags Various flags. Mostly color, but also some others.
 * @param pri Priority. It is a little odd - the lower the value, the
 * more important it is. Thus, 0 gets sent no matter what. Otherwise, the
 * value must be less than the @ref player::listening "listening" level
 * that the player has set. Unfortunately, there is no clear guideline on
 * what each level does what.
 * @param pl The player object to write the information to - if flags has
 * @ref NDI_ALL, this is unused and can be NULL.
 * @param buf The message to draw.
 * @see NDI_xxx*/
void new_draw_info(int flags, int pri, object *pl, const char *buf)
{
	char info_string[HUGE_BUF];
	SockList sl;

	/* Should not happen - generate safe string and LOG it */
	if (!buf)
	{
		buf = "[NULL]";
		LOG(llevBug, "BUG: new_draw_info: NULL string send! %s (%x - %d)\n", query_name(pl, NULL), flags, pri);
	}

	/* Here we handle global messages. */
	if (flags & NDI_ALL)
	{
		player *tmppl;

		for (tmppl = first_player; tmppl != NULL; tmppl = tmppl->next)
		{
			new_draw_info((flags & ~NDI_ALL), pri, tmppl->ob, buf);
		}

		return;
	}

	/* Handle some security stuff... a bit overhead for max secure */
	if (!pl || pl->type != PLAYER)
	{
		LOG(llevBug, "BUG: new_draw_info: called for object != PLAYER! %s (%x - %d) msg: %s\n", query_name(pl, NULL), flags, pri, buf);
		return;
	}

	if (CONTR(pl) == NULL)
	{
		LOG(llevBug, "BUG: new_draw_info: called for player with contr == NULL! %s (%x - %d) msg: %s\n", query_name(pl, NULL), flags, pri, buf);
		return;
	}

	if (CONTR(pl)->state != ST_PLAYING)
	{
		return;
	}

	/* Player doesn't want this */
	if (pri >= CONTR(pl)->listening)
	{
		return;
	}

	sl.buf = (unsigned char *) info_string;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_DRAWINFO2);
	SockList_AddShort(&sl, flags & NDI_FLAG_MASK);
	strcpy((char *) sl.buf + sl.len, buf);
	sl.len += strlen(buf);
	Send_With_Handling(&CONTR(pl)->socket, &sl);

#if 0
	sprintf(info_string, "X%d %s", flags & NDI_FLAG_MASK, buf);
	Write_String_To_Socket(&CONTR(pl)->socket, BINARY_CMD_DRAWINFO, info_string, strlen(info_string));
#endif
}

/**
 * Similar to new_draw_info() but allows to use printf style
 * formatting. */
void new_draw_info_format(int flags, int pri, object *pl, char *format, ...)
{
	char buf[HUGE_BUF];

	va_list ap;
	va_start(ap, format);

	vsprintf(buf, format, ap);

	va_end(ap);

	new_draw_info(flags, pri, pl, buf);
}

/* we want give msg to all people on one, specific map */
static void new_info_map_all_except(int color, mapstruct *map, object *op1, object *op, const char *str)
{
	object *tmp;

	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1)
			{
				new_draw_info(color, 0, tmp, str);
			}
		}
	}
}

/* write to everyone on the current map */
void new_info_map(int color, mapstruct *map, int x, int y, int dist, const char *str)
{
	int xt, yt, d;
	object *tmp;

	if (!map || map->in_memory != MAP_IN_MEMORY)
	{
		return;
	}

	if (dist != MAP_INFO_ALL)
	{
		d = POW2(dist);
	}
	else
	{
		/* we want all on this map */
		new_info_map_all_except(color, map, NULL, NULL, str);
		return;
	}

	/* any player on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);
		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt =x + MAP_WIDTH(map->tile_map[3]);
		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[4]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);
		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}
}

/* write to everyone on the map *except* op.  This is useful for emotions. */
void new_info_map_except(int color, mapstruct *map, int x, int y, int dist, object *op1, object *op, const char *str)
{
	int xt, yt, d;
	object *tmp;

	if (!map || map->in_memory != MAP_IN_MEMORY)
		return;

	if (dist != MAP_INFO_ALL)
		d = POW2(dist);
	else
	{
		/* we want all on this map */
		new_info_map_all_except(color, map, op1, op, str);
		return;
	}

	/* any player on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);
		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[3]);
		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);
		for (tmp = map->tile_map[4]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);
		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);
		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
				new_draw_info(color, 0, tmp, str);
		}
	}
}
