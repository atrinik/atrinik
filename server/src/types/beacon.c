/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * @author Alex Tokar */

#include <global.h>

/**
 * One beacon. */
typedef struct beacon_struct
{
    /** The beacon object. */
    object *ob;

    /** Hash handle. */
    UT_hash_handle hh;
} beacon_struct;

/** Beacons hashtable. */
static beacon_struct *beacons = NULL;

/**
 * Add a beacon to ::beacons_list.
 * @param ob Beacon to add. */
void beacon_add(object *ob)
{
    beacon_struct *beacon;
    object *env;

    env = get_env_recursive(ob);

    if (MAP_UNIQUE(env->map) && !map_path_isabs(ob->name)) {
        char *filedir, *pl_name, *joined;

        filedir = path_dirname(env->map->path);
        pl_name = path_basename(filedir);
        joined = string_join("-", "/", pl_name, ob->name, NULL);

        FREE_AND_COPY_HASH(ob->name, joined);

        free(joined);
        free(pl_name);
        free(filedir);
    }

    beacon = malloc(sizeof(beacon_struct));
    beacon->ob = ob;
    HASH_ADD(hh, beacons, ob->name, sizeof(shstr *), beacon);
}

/**
 * Remove a beacon from ::beacons_list.
 * @param ob Beacon to remove. */
void beacon_remove(object *ob)
{
    beacon_struct *beacon;

    HASH_FIND(hh, beacons, &ob->name, sizeof(shstr *), beacon);

    if (beacon) {
        HASH_DEL(beacons, beacon);
        free(beacon);
    }
    else {
        logger_print(LOG(BUG), "Could not remove beacon %s from hashtable.", ob->name);
    }
}

/**
 * Locate a beacon object in ::beacons_list.
 * @param name Name of the beacon to locate. Must be a shared string.
 * @return The beacon object if found, NULL otherwise. */
object *beacon_locate(shstr *name)
{
    beacon_struct *beacon;

    HASH_FIND(hh, beacons, &name, sizeof(shstr *), beacon);

    if (beacon) {
        return beacon->ob;
    }

    return NULL;
}

/**
 * Initialize the beacon type object methods. */
void object_type_init_beacon(void)
{
}
