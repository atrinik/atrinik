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
 * Handles movement events. */

#include <global.h>

/**
 * Number of the possible directions. */
#define DIRECTIONS_NUM 9

/**
 * Directions to fire into. */
static const int directions_fire[DIRECTIONS_NUM] =
{
	6, 5, 4, 7, 0, 3, 8, 1, 2
};

void move_keys(int num)
{
	packet_struct *packet;

	if (cpl.fire_on)
	{
		packet = packet_new(SERVER_CMD_FIRE, 64, 64);
		packet_append_uint8(packet, directions_fire[num - 1]);
		packet_append_uint8(packet, RangeFireMode);

		if (RangeFireMode == FIRE_MODE_SKILL)
		{
			if (!fire_mode_tab[FIRE_MODE_SKILL].skill)
			{
				draw_info(COLOR_WHITE, "No skill selected.");
				return;
			}

			packet_append_string_terminated(packet, fire_mode_tab[RangeFireMode].skill->name);
		}
		else if (RangeFireMode == FIRE_MODE_SPELL)
		{
			if (!fire_mode_tab[FIRE_MODE_SPELL].spell)
			{
				draw_info(COLOR_WHITE, "No spell selected.");
				return;
			}

			packet_append_string_terminated(packet, fire_mode_tab[RangeFireMode].spell->name);
		}
		else if (RangeFireMode == FIRE_MODE_BOW)
		{
			if (fire_mode_tab[FIRE_MODE_BOW].item == FIRE_ITEM_NO)
			{
				draw_info(COLOR_WHITE, "No range weapon selected.");
				return;
			}
			else if (fire_mode_tab[FIRE_MODE_BOW].amun == FIRE_ITEM_NO)
			{
				draw_info(COLOR_WHITE, "No ammunition selected.");
				return;
			}
		}
		else if (RangeFireMode == FIRE_MODE_THROW)
		{
			if (fire_mode_tab[FIRE_MODE_THROW].item == FIRE_ITEM_NO)
			{
				draw_info(COLOR_WHITE, "No item selected.");
				return;
			}
		}
		else if (RangeFireMode == FIRE_MODE_WAND)
		{
			if (fire_mode_tab[FIRE_MODE_WAND].item == FIRE_ITEM_NO)
			{
				draw_info(COLOR_WHITE, "No device selected.");
				return;
			}
		}

		socket_send_packet(packet);
	}
	else
	{
		if (num == 5 && !cpl.run_on)
		{
			packet = packet_new(SERVER_CMD_CLEAR, 0, 0);
			socket_send_packet(packet);
		}
		else
		{
			if (num == 5)
			{
				cpl.run_on = 0;
			}

			packet = packet_new(SERVER_CMD_MOVE, 8, 0);
			packet_append_uint8(packet, directions_fire[num - 1]);
			packet_append_uint8(packet, cpl.run_on);
			socket_send_packet(packet);
		}
	}
}

/**
 * Transform tile coordinates into direction, which can be used as a
 * result for functions like move_keys() or ::directions_move (return
 * value - 1).
 * @param tx Tile X.
 * @param ty Tile Y.
 * @return The direction, 1-9. */
int dir_from_tile_coords(int tx, int ty)
{
	int player_tile_x = setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2, player_tile_y = setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2;
	int q, x, y;

	if (tx == player_tile_x && ty == player_tile_y)
	{
		return 5;
	}

	x = -(tx - player_tile_x);
	y = -(ty - player_tile_y);

	if (!y)
	{
		q = -300 * x;
	}
	else
	{
		q = x * 100 / y;
	}

	if (y > 0)
	{
		/* East */
		if (q < -242)
		{
			return 6;
		}

		/* Northeast */
		if (q < -41)
		{
			return 9;
		}

		/* North */
		if (q < 41)
		{
			return 8;
		}

		/* Northwest */
		if (q < 242)
		{
			return 7;
		}

		/* West */
		return 4;
	}

	/* West */
	if (q < -242)
	{
		return 4;
	}

	/* Southwest */
	if (q < -41)
	{
		return 1;
	}

	/* South */
	if (q < 41)
	{
		return 2;
	}

	/* Southeast */
	if (q < 242)
	{
		return 3;
	}

	/* East */
	return 6;
}
