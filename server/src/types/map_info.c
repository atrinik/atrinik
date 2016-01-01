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
 * Handles @ref MAP_INFO "map info" objects.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::init_func */
static void
init_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->map == NULL) {
        LOG(ERROR,
            "Map info object not on map: %s",
            object_get_str(op));
        return;
    }

    for (int x = op->x; x <= op->x + op->stats.hp; x++) {
        for (int y = op->y; y <= op->y + op->stats.sp; y++) {
            if (OUT_OF_MAP(op->map, x, y)) {
                LOG(ERROR,
                    "Map info object spans invalid area: %s",
                    object_get_str(op));
                return;
            }

            MapSpace *msp = GET_MAP_SPACE_PTR(op->map, x, y);
            msp->map_info = op;
            msp->map_info_count = op->count;

            if (QUERY_FLAG(op, FLAG_NO_MAGIC)) {
                msp->extra_flags |= MSP_EXTRA_NO_MAGIC;
            }

            if (QUERY_FLAG(op, FLAG_NO_PVP)) {
                msp->extra_flags |= MSP_EXTRA_NO_PVP;
            }

            if (QUERY_FLAG(op, FLAG_STAND_STILL)) {
                msp->extra_flags |= MSP_EXTRA_NO_HARM;
            }

            if (QUERY_FLAG(op, FLAG_CURSED)) {
                msp->extra_flags ^= MSP_EXTRA_IS_BUILDING;
            }

            if (QUERY_FLAG(op, FLAG_IS_MAGICAL)) {
                msp->extra_flags ^= MSP_EXTRA_IS_BALCONY;
            }

            if (QUERY_FLAG(op, FLAG_DAMNED)) {
                msp->extra_flags ^= MSP_EXTRA_IS_OVERLOOK;
            }
        }
    }
}

/**
 * Initialize the map info type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(map_info)
{
    OBJECT_METHODS(MAP_INFO)->init_func = init_func;
}
