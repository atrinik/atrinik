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
 * Internally used by command_resetmaps() to create an array of the maps to
 * reset.
 * @param tiled Tiled map.
 * @param map Map.
 * @param[out] maps Array of maps that will be reset.
 * @param[out] maps_num Number of maps in 'maps'.
 * @return 0.
 */
static int command_resetmaps_internal(mapstruct *tiled, mapstruct *map,
        mapstruct ***maps, size_t *maps_num)
{
    *maps = erealloc(*maps, sizeof(**maps) * ((*maps_num) + 1));
    (*maps)[*maps_num] = tiled;
    (*maps_num)++;

    return 0;
}

/** @copydoc command_func */
void command_resetmaps(object *op, const char *command, char *params)
{
    mapstruct *m, **maps;
    size_t maps_num, i;
    int failed, success;

    m = NULL;
    maps = NULL;
    maps_num = 0;
    failed = success = 0;

    if (params != NULL && strcasecmp(params, "all") == 0) {
        DL_FOREACH(first_map, m)
        {
            command_resetmaps_internal(m, NULL, &maps, &maps_num);
        }
    } else {
        shstr *mappath;

        mappath = NULL;

        if (params == NULL) {
            m = op->map;
        } else if (!map_path_isabs(params)) {
            char *path;

            path = map_get_path(op->map, params, 0, NULL);
            mappath = add_string(path);
            efree(path);
        } else {
            mappath = add_string(params);
        }

        if (mappath != NULL) {
            m = has_been_loaded_sh(mappath);
            free_string_shared(mappath);
        }

        if (m == NULL) {
            draw_info(COLOR_WHITE, op, "No such map.");
            return;
        }

        MAP_TILES_WALK_START(m, command_resetmaps_internal, &maps, &maps_num)
        {
        }
        MAP_TILES_WALK_END
    }

    if (maps == NULL) {
        LOG(BUG, "Failed to find any maps to reset: %s", m->path);
        draw_info_format(COLOR_RED, op, "Failed to find any maps to reset: %s",
                m->path);
        return;
    }

    for (i = 0; i < maps_num; i++) {
        if (map_force_reset(maps[i]) != NULL) {
            success++;
        } else {
            failed++;
        }
    }

    efree(maps);

    if (success != 0) {
        draw_info_format(COLOR_WHITE, op, "Successfully reset %d maps.",
                success);
    }

    if (failed != 0) {
        draw_info_format(COLOR_RED, op, "Failed to reset %d maps.",
                failed);
    }
}
