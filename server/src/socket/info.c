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
 * This file is the one and only DRAWINFO output module.
 *
 * All player communication using drawinfo is handled here - except the
 * few messages we send to the client using DRAWINFO before we had setup
 * any player structure - for example when an outdated client logs in and
 * we send "update your client" directly to the info windows.
 *
 * But if the player is logged in, all DRAWINFO are generated here. */

#include <global.h>
#include <stdarg.h>

/**
 * Draw a message in the text windows for player's client.
 * @param flags Various @ref NDI_xxx "flags". Mostly color, but also some others.
 * @param pl The player object to write the information to - if flags has
 * @ref NDI_ALL, this is unused and can be NULL.
 * @param buf The message to draw. */
void new_draw_info(int flags, object *pl, const char *buf)
{
	unsigned char info_string[HUGE_BUF];
	size_t len;
	SockList sl;

	/* Handle global messages. */
	if (flags & NDI_ALL)
	{
		player *tmppl;

		for (tmppl = first_player; tmppl; tmppl = tmppl->next)
		{
			new_draw_info((flags & ~NDI_ALL), tmppl->ob, buf);
		}

		return;
	}

	if (!pl || pl->type != PLAYER)
	{
		return;
	}

	if (CONTR(pl) == NULL)
	{
		LOG(llevBug, "BUG: new_draw_info(): Called for player with contr == NULL! %s (%x) msg: %s\n", query_name(pl, NULL), flags, buf);
		return;
	}

	if (CONTR(pl)->state != ST_PLAYING)
	{
		return;
	}

	sl.buf = info_string;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_DRAWINFO2);
	SockList_AddShort(&sl, flags & NDI_FLAG_MASK);
	/* Make sure we don't copy more bytes than available space in the buffer. */
	len = MIN(strlen(buf), sizeof(info_string) - sl.len - 1);
	memcpy((char *) sl.buf + sl.len, buf, len);
	sl.len += len;
	/* Terminate the string. */
	SockList_AddChar(&sl, '\0');
	Send_With_Handling(&CONTR(pl)->socket, &sl);
}

/**
 * Similar to new_draw_info() but allows to use printf style
 * formatting.
 * @param flags Flags.
 * @param pl Player.
 * @param format Format.
 * @see new_draw_info() */
void new_draw_info_format(int flags, object *pl, char *format, ...)
{
	char buf[HUGE_BUF];

	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	new_draw_info(flags, pl, buf);
}

/**
 * Give message to all people on one, specified map.
 * @param color Flags.
 * @param map The map.
 * @param op1 Will not write to this object.
 * @param op Will not write to this object.
 * @param str What to write. */
static void new_info_map_all_except(int color, mapstruct *map, object *op1, object *op, const char *str)
{
	object *tmp;

	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}
}

/**
 * Writes to everyone on the specified map.
 *
 * Tiled maps will be considered.
 * @param color Flags.
 * @param map Map to write on.
 * @param x X position.
 * @param y Y position.
 * @param dist Distance.
 * @param str What to write. */
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
		/* We want all on this map */
		new_info_map_all_except(color, map, NULL, NULL, str);
		return;
	}

	/* Any players on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);

		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[3]);

		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[4]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);

		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if ((POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}
}

/**
 * Writes to everyone on the map *except* op and op1. This is useful for emotions.
 *
 * Tiled maps will be considered.
 * @param color Flags.
 * @param map Map to write on.
 * @param x X position.
 * @param y Y position.
 * @param dist Distance.
 * @param op1 Will not write to this object.
 * @param op Will not write to this object.
 * @param str What to write. */
void new_info_map_except(int color, mapstruct *map, int x, int y, int dist, object *op1, object *op, const char *str)
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
		/* We want all on this map */
		new_info_map_all_except(color, map, op1, op, str);
		return;
	}

	/* Any players on this map? */
	if (map->player_first)
	{
		for (tmp = map->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[0] && map->tile_map[0]->in_memory == MAP_IN_MEMORY && map->tile_map[0]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[0]);

		for (tmp = map->tile_map[0]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[1] && map->tile_map[1]->in_memory == MAP_IN_MEMORY && map->tile_map[1]->player_first)
	{
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[1]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[2] && map->tile_map[2]->in_memory == MAP_IN_MEMORY && map->tile_map[2]->player_first)
	{
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[2]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - x) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[3] && map->tile_map[3]->in_memory == MAP_IN_MEMORY && map->tile_map[3]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[3]);

		for (tmp = map->tile_map[3]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - y)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[4] && map->tile_map[4]->in_memory == MAP_IN_MEMORY && map->tile_map[4]->player_first)
	{
		yt = y + MAP_HEIGHT(map->tile_map[4]);
		xt = x - MAP_WIDTH(map);

		for (tmp = map->tile_map[4]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[5] && map->tile_map[5]->in_memory == MAP_IN_MEMORY && map->tile_map[5]->player_first)
	{
		xt = x - MAP_WIDTH(map);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[5]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[6] && map->tile_map[6]->in_memory == MAP_IN_MEMORY && map->tile_map[6]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[6]);
		yt = y - MAP_HEIGHT(map);

		for (tmp = map->tile_map[6]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}

	if (map->tile_map[7] && map->tile_map[7]->in_memory == MAP_IN_MEMORY && map->tile_map[7]->player_first)
	{
		xt = x + MAP_WIDTH(map->tile_map[7]);
		yt = y + MAP_HEIGHT(map->tile_map[7]);

		for (tmp = map->tile_map[7]->player_first; tmp; tmp = CONTR(tmp)->map_above)
		{
			if (tmp != op && tmp != op1 && (POW2(tmp->x - xt) + POW2(tmp->y - yt)) <= d)
			{
				new_draw_info(color, tmp, str);
			}
		}
	}
}

/**
 * Send a socket message, similar to new_draw_info() but the message will
 * be sent using Write_String_To_Socket() instead.
 *
 * Used for messages that are sent to player before they have finished
 * logging in.
 * @param flags Flags to send.
 * @param ns Socket to send to.
 * @param buf Message to send. */
void send_socket_message(int flags, socket_struct *ns, const char *buf)
{
	char tmp[MAX_BUF];

	snprintf(tmp, sizeof(tmp), "X%d %s", flags, buf);
	Write_String_To_Socket(ns, BINARY_CMD_DRAWINFO, tmp, strlen(tmp));
}
