/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
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

    if (!info->map) {
        LOG(BUG, "Map info object not on map.");
        return;
    }

    for (x = info->x; x <= info->x + info->stats.hp; x++) {
        for (y = info->y; y <= info->y + info->stats.sp; y++) {
            if (OUT_OF_MAP(info->map, x, y)) {
                LOG(ERROR, "Map info object spans invalid area: %s",
                        object_get_str(info));
                return;
            }

            msp = GET_MAP_SPACE_PTR(info->map, x, y);
            msp->map_info = info;
            msp->map_info_count = info->count;

            if (QUERY_FLAG(info, FLAG_NO_MAGIC)) {
                msp->extra_flags |= MSP_EXTRA_NO_MAGIC;
            }

            if (QUERY_FLAG(info, FLAG_NO_PVP)) {
                msp->extra_flags |= MSP_EXTRA_NO_PVP;
            }

            if (QUERY_FLAG(info, FLAG_STAND_STILL)) {
                msp->extra_flags |= MSP_EXTRA_NO_HARM;
            }

            if (QUERY_FLAG(info, FLAG_CURSED)) {
                msp->extra_flags ^= MSP_EXTRA_IS_BUILDING;
            }

            if (QUERY_FLAG(info, FLAG_IS_MAGICAL)) {
                msp->extra_flags ^= MSP_EXTRA_IS_BALCONY;
            }

            if (QUERY_FLAG(info, FLAG_DAMNED)) {
                msp->extra_flags ^= MSP_EXTRA_IS_OVERLOOK;
            }
        }
    }
}

/**
 * Initialize the map info type object methods. */
void object_type_init_map_info(void)
{
}
