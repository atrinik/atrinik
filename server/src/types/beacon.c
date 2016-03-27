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
 * @ref BEACON "Beacon" related code.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <object.h>
#include <object_methods.h>
#include <beacon.h>

/**
 * One beacon.
 */
typedef struct beacon {
    /** The beacon object. */
    object *obj;

    /** Hash handle. */
    UT_hash_handle hh;
} beacon_t;

/** Beacons hashtable. */
static beacon_t *beacons = NULL;

/** @copydoc object_methods_t::init_func */
static void
init_func (object *op)
{
    HARD_ASSERT(op != NULL);

    /* Figure out where the beacon is. */
    object *env = object_get_env(op);

    /* If the beacon is on a unique map, we need to make the beacon name
     * unique to the player that owns the unique map. */
    if (MAP_UNIQUE(env->map) && !map_path_isabs(op->name)) {
        char *filedir = path_dirname(env->map->path);
        char *pl_name = path_basename(filedir);
        char *joined = string_join("-", "/", pl_name, op->name, NULL);

        FREE_AND_COPY_HASH(op->name, joined);

        efree(joined);
        efree(pl_name);
        efree(filedir);
    }

    beacon_t *beacon = emalloc(sizeof(*beacon));
    beacon->obj = op;
    HASH_ADD(hh, beacons, obj->name, sizeof(shstr *), beacon);
}

/** @copydoc object_methods_t::deinit_func */
static void
deinit_func (object *op)
{
    beacon_t *beacon;
    HASH_FIND(hh, beacons, &op->name, sizeof(shstr *), beacon);
    if (unlikely(beacon == NULL)) {
        LOG(ERROR, "Beacon %s not found in hashtable", op->name);
        return;
    }

    HASH_DEL(beacons, beacon);
    efree(beacon);
}

/**
 * Initialize the beacon type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(beacon)
{
    OBJECT_METHODS(BEACON)->init_func = init_func;
    OBJECT_METHODS(BEACON)->deinit_func = deinit_func;
}

/**
 * Locate a beacon object.
 *
 * @param name
 * Name of the beacon to locate. Must be a shared string.
 * @return
 * The beacon object if found, NULL otherwise.
 */
object *
beacon_locate (shstr *name)
{
    HARD_ASSERT(name != NULL);

    beacon_t *beacon;
    HASH_FIND(hh, beacons, &name, sizeof(shstr *), beacon);

    if (beacon != NULL) {
        return beacon->obj;
    }

    return NULL;
}
