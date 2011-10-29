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
 * Handles @ref MAP_INFO "map info" objects. */

#include <global.h>

/**
 * Initialize a map info object.
 * @param info The info object. */
void map_info_init(object *info)
{
	int x, y;
	MapSpace *msp;

	if (!info->map)
	{
		LOG(llevBug, "Map info object not on map.\n");
		return;
	}

	for (x = info->x; x <= info->x + info->stats.hp; x++)
	{
		for (y = info->y; y <= info->y + info->stats.sp; y++)
		{
			if (OUT_OF_MAP(info->map, x, y))
			{
				LOG(llevBug, "Map info object (%d, %d) spans invalid area.\n", info->x, info->y);
				return;
			}

			msp = GET_MAP_SPACE_PTR(info->map, x, y);
			msp->map_info = info;
			msp->map_info_count = info->count;

			if (QUERY_FLAG(info, FLAG_NO_MAGIC))
			{
				msp->extra_flags |= MSP_EXTRA_NO_MAGIC;
			}

			if (QUERY_FLAG(info, FLAG_NO_CLERIC))
			{
				msp->extra_flags |= MSP_EXTRA_NO_CLERIC;
			}

			if (QUERY_FLAG(info, FLAG_NO_PVP))
			{
				msp->extra_flags |= MSP_EXTRA_NO_PVP;
			}

			if (QUERY_FLAG(info, FLAG_STAND_STILL))
			{
				msp->extra_flags |= MSP_EXTRA_NO_HARM;
			}
		}
	}
}
