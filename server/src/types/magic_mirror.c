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
 * Handles code for @ref MAGIC_MIRROR "magic mirrors".
 *
 * Magic mirrors are objects that mirror contents of another tile,
 * effectively creating a map stacking effect. It is also possible to
 * make the magic mirrors zoom out/in mirrored objects to create a depth
 * effect.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>
#include <magic_mirror.h>

/** @copydoc object_methods_t::init_func */
static void
init_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->map == NULL) {
        LOG(ERROR, "Magic mirror not on map.");
        return;
    }

    int mirror_x = (op->stats.hp == -1 ? op->x : op->stats.hp);
    int mirror_y = (op->stats.sp == -1 ? op->y : op->stats.sp);

    /* X/Y adjust. */
    if (op->stats.maxhp != 0) {
        mirror_x += op->stats.maxhp;
    }

    if (op->stats.maxsp != 0) {
        mirror_y += op->stats.maxsp;
    }

    /* No point in doing anything special if we're mirroring the same map. */
    if (op->slaying == NULL && mirror_x == op->x && mirror_y == op->y) {
        return;
    }

    /* No map path specified, use mirror's map path. */
    if (op->slaying == NULL) {
        FREE_AND_ADD_REF_HASH(op->slaying, op->map->path);
    } else if (!map_path_isabs(op->slaying)) {
        char *path = map_get_path(op->map,
                                  op->slaying,
                                  MAP_UNIQUE(op->map),
                                  NULL);
        FREE_AND_COPY_HASH(op->slaying, path);
        efree(path);
    }

    op->custom_attrset = emalloc(sizeof(magic_mirror_struct));
    /* Save x/y and clear map. */
    MMIRROR(op)->x = mirror_x;
    MMIRROR(op)->y = mirror_y;
    MMIRROR(op)->map = NULL;
}

/** @copydoc object_methods_t::deinit_func */
static void
deinit_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->custom_attrset != NULL) {
        efree(op->custom_attrset);
    }
}

/**
 * Initialize the magic mirror type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(magic_mirror)
{
    OBJECT_METHODS(MAGIC_MIRROR)->init_func = init_func;
    OBJECT_METHODS(MAGIC_MIRROR)->deinit_func = deinit_func;
}

/**
 * Get map to which a magic mirror is pointing to. Almost always this
 * should be used instead of accessing magic_mirror_struct::map directly,
 * as it will make sure the map is loaded and will reset the swap timeout.
 *
 * @param op
 * Magic mirror to get map of.
 * @return
 * The map. Can be NULL in case of loading error.
 */
mapstruct *
magic_mirror_get_map (object *op)
{
    HARD_ASSERT(op != NULL);

    magic_mirror_struct *data = MMIRROR(op);

    /* Map good to go? */
    if (data->map && data->map->in_memory == MAP_IN_MEMORY) {
        /* Reset timeout so the mirrored map doesn't get swapped out. */
        MAP_TIMEOUT(data->map) = MAP_DEFAULTTIMEOUT;
        return data->map;
    }

    /* Try to load the map. */
    data->map = ready_map_name(op->slaying, NULL, MAP_NAME_SHARED);
    if (data->map == NULL) {
        LOG(ERROR, "Could not load map '%s': %s",
            op->slaying,
            object_get_str(op));
        return NULL;
    }

    return data->map;
}
