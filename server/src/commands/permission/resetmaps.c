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
 * Implements the /resetmaps command.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Internally used by command_resetmaps() to actually perform the reset of all
 * tiled maps.
 * @param tiled Tiled map.
 * @param map Map.
 * @return 0 on success, 1 on failure.
 */
static int command_resetmaps_internal(mapstruct *tiled, mapstruct *map)
{
    if (map_force_reset(tiled) == NULL) {
        return 1;
    }

    return 0;
}

/** @copydoc command_func */
void command_resetmaps(object *op, const char *command, char *params)
{
    int failed;

    failed = 0;

    MAP_TILES_WALK_START(op->map, command_resetmaps_internal)
    {
        if (MAP_TILES_WALK_RETVAL != 0) {
            failed++;
        }
    }
    MAP_TILES_WALK_END

    if (failed == 0) {
        draw_info(COLOR_WHITE, op, "Successfully reset all tiled maps.");
    } else {
        draw_info_format(COLOR_WHITE, op, "Failed to reset %d tiled maps.",
                failed);
    }
}
