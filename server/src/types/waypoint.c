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
 * Handles the code for @ref WAYPOINT_OBJECT "waypoint objects". */

#include <global.h>
#include <plugin.h>
#include <monster_guard.h>

/**
 * Find a monster's currently active waypoint, if any.
 * @param op The monster.
 * @return Active waypoint of this monster, NULL if none found. */
object *get_active_waypoint(object *op)
{
    object *wp;

    for (wp = op->inv; wp; wp = wp->below) {
        if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_CURSED)) {
            return wp;
        }
    }

    return NULL;
}

/**
 * Find a monster's current aggro waypoint, if any.
 * @param op The monster.
 * @return Aggro waypoint of this monster, NULL if none found. */
object *get_aggro_waypoint(object *op)
{
    object *wp;

    for (wp = op->inv; wp; wp = wp->below) {
        if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_DAMNED)) {
            return wp;
        }
    }

    return NULL;
}

/**
 * Find a monster's current return-home waypoint, if any.
 * @param op The monster.
 * @return Return-home waypoint of this monster, NULL if none
 * found. */
object *get_return_waypoint(object *op)
{
    object *wp;

    for (wp = op->inv; wp; wp = wp->below) {
        if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_REFLECTING)) {
            return wp;
        }
    }

    return NULL;
}

/**
 * Find a monster's waypoint by name (used for getting the next waypoint).
 * @param op The monster.
 * @param name The waypoint name to find.
 * @return The waypoint object if found, NULL otherwise. */
static object *find_waypoint(object *op, shstr *name)
{
    object *wp;

    if (name == NULL) {
        return NULL;
    }

    for (wp = op->inv; wp; wp = wp->below) {
        if (wp->type == WAYPOINT_OBJECT && wp->name == name) {
            return wp;
        }
    }

    return NULL;
}

/**
 * Find the destination map if specified in waypoint, otherwise use current
 * map.
 * @param op Monster.
 * @param waypoint Waypoint.
 * @return Destination map. */
static mapstruct *waypoint_load_dest(object *op, object *waypoint)
{
    mapstruct *destmap;

    if (!map_path_isabs(waypoint->slaying)) {
        char *path;

        path = map_get_path(op->map, waypoint->slaying, MAP_UNIQUE(op->map), NULL);
        FREE_AND_COPY_HASH(waypoint->slaying, path);
        efree(path);
    }

    if (waypoint->slaying == op->map->path) {
        destmap = op->map;
    } else {
        destmap = ready_map_name(waypoint->slaying, NULL, MAP_NAME_SHARED);
    }

    return destmap;
}

/**
 * Perform a path computation for the waypoint object.
 *
 * This function is called whenever our path request is dequeued.
 * @param waypoint The waypoint object. */
void waypoint_compute_path(object *waypoint)
{
    object *op = waypoint->env;
    mapstruct *destmap = op->map;
    path_node_t *path;

    /* Store final path destination (used by aggro wp) */
    if (QUERY_FLAG(waypoint, FLAG_DAMNED)) {
        if (OBJECT_VALID(waypoint->enemy, waypoint->enemy_count)) {
            FREE_AND_COPY_HASH(waypoint->slaying, waypoint->enemy->map->path);
            waypoint->x = waypoint->stats.hp = waypoint->enemy->x;
            waypoint->y = waypoint->stats.sp = waypoint->enemy->y;
        } else {
            LOG(BUG, "Dynamic waypoint without valid target: '%s'", waypoint->name);
            return;
        }
    }

    if (waypoint->slaying) {
        destmap = waypoint_load_dest(op, waypoint);
    }

    if (!destmap) {
        LOG(BUG, "Invalid destination map '%s'", waypoint->slaying);
        return;
    }

    path = path_compress(path_find(op, op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp, NULL));

    if (!path) {
        if (!QUERY_FLAG(waypoint, FLAG_DAMNED)) {
            LOG(BUG, "No path to destination ('%s', %s @ %d,%d -> "
                    "'%s', %s @ %d,%d)", op->name, op->map->path, op->x, op->y,
                    waypoint->name, destmap->path, waypoint->stats.hp,
                    waypoint->stats.sp);
        }

        return;
    }

    /* Skip the first path element (always the starting position), but only
     * if it's not a node with an exit. */
    if (!(path->flags & PATH_NODE_EXIT)) {
        path = path->next;

        if (!path) {
            return;
        }
    }

    /* Textually encoded path */
    FREE_AND_CLEAR_HASH(waypoint->msg);
    /* Map file for last local path step */
    FREE_AND_CLEAR_HASH(waypoint->race);

    waypoint->msg = path_encode(path);
    /* Path offset */
    waypoint->stats.food = 0;
    /* Msg boundary */
    waypoint->attacked_by_distance = strlen(waypoint->msg);

    /* Number of fails */
    waypoint->stats.Int = 0;
    /* Number of fails */
    waypoint->stats.Str = 0;
    /* Best distance */
    waypoint->stats.dam = 30000;
}

/**
 * Move towards waypoint target.
 * @param op Object to move.
 * @param waypoint The waypoint object. */
void waypoint_move(object *op, object *waypoint)
{
    mapstruct *destmap;
    rv_vector local_rv, global_rv, *dest_rv;
    int dir, destx, desty;
    int16_t new_offset = 0;
    uint32_t destflags;

    if (!waypoint || !op || !op->map) {
        return;
    }

    destmap = op->map;

    /* Aggro or static waypoint? */
    if (QUERY_FLAG(waypoint, FLAG_DAMNED)) {
        /* Verify enemy */
        if (waypoint->enemy == op->enemy && waypoint->enemy_count == op->enemy_count && OBJECT_VALID(waypoint->enemy, waypoint->enemy_count)) {
            destmap = waypoint->enemy->map;
            waypoint->stats.hp = waypoint->enemy->x;
            waypoint->stats.sp = waypoint->enemy->y;
        } else {
            /* Owner has either switched or lost enemy. This should work for
             * both cases.
             * switched -> similar to if target moved
             * lost -> we shouldn't be called again without new data */
            waypoint->enemy = op->enemy;
            waypoint->enemy_count = op->enemy_count;
            return;
        }
    } else if (waypoint->slaying) {
        destmap = waypoint_load_dest(op, waypoint);
    }

    if (!destmap) {
        LOG(BUG, "Invalid destination map '%s' for '%s' -> '%s'", waypoint->slaying, op->name, waypoint->name);
        return;
    }

    if (!get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp, &global_rv, RV_RECURSIVE_SEARCH | RV_DIAGONAL_DISTANCE)) {
        LOG(DEBUG, "Could not find rv to: %s, %d, %d", destmap->path, waypoint->stats.hp, waypoint->stats.sp);
        /* Disable this waypoint */
        CLEAR_FLAG(waypoint, FLAG_CURSED);
        return;
    }

    dest_rv = &global_rv;

    /* Reached the final destination? */
    if (global_rv.distance_z == 0 && (int) global_rv.distance <= waypoint->stats.maxsp) {
        object *nextwp = NULL;

        /* Just arrived? */
        if (waypoint->stats.ac == 0) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "'%s' reached destination '%s'", op->name, waypoint->name);
#endif

            /* Trigger the TRIGGER event */
            if (trigger_event(EVENT_TRIGGER, op, waypoint, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
                return;
            }
        }

        /* Waiting at this waypoint? */
        if (waypoint->stats.ac < waypoint->stats.wc) {
            waypoint->stats.ac++;
            return;
        }

        /* Clear timer */
        waypoint->stats.ac = 0;
        /* Set as inactive */
        CLEAR_FLAG(waypoint, FLAG_CURSED);
        /* Remove precomputed path */
        FREE_AND_CLEAR_HASH(waypoint->msg);
        /* Remove precomputed path data */
        FREE_AND_CLEAR_HASH(waypoint->race);

        /* Is it a return-home waypoint? */
        if (QUERY_FLAG(waypoint, FLAG_REFLECTING)) {
            op->move_type = waypoint->move_type;
            monster_guard_activate_gate(op, 0);
        }

        /* Start over with the new waypoint, if any */
        if (!QUERY_FLAG(waypoint, FLAG_DAMNED)) {
            nextwp = find_waypoint(op, waypoint->title);

            if (nextwp) {
#ifdef DEBUG_PATHFINDING
                LOG(DEBUG, "'%s' next waypoint: '%s'", op->name, waypoint->title);
#endif
                SET_FLAG(nextwp, FLAG_CURSED);
            }
#ifdef DEBUG_PATHFINDING
            else {
                LOG(DEBUG, "'%s' is missing next waypoint.", op->name);
            }
#endif
        }

        waypoint->enemy = NULL;
        return;
    }

    if (HAS_EVENT(waypoint, EVENT_CLOSE) && trigger_event(EVENT_CLOSE, op, waypoint, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
        return;
    }

    /* Handle precomputed paths */

    /* If we finished our current path, clear it so that we can get a new one.
     * */
    if (waypoint->msg && (waypoint->msg[waypoint->stats.food] == '\0' || (global_rv.distance_z == 0 && global_rv.distance <= 0))) {
        FREE_AND_CLEAR_HASH(waypoint->msg);
    }

    /* Get new path if target has moved much since the path was created */
    if (QUERY_FLAG(waypoint, FLAG_DAMNED) && waypoint->msg && (waypoint->stats.hp != waypoint->x || waypoint->stats.sp != waypoint->y)) {
        rv_vector rv;

        get_rangevector_from_mapcoords(destmap, waypoint->stats.hp, waypoint->stats.sp, destmap, waypoint->x, waypoint->y, &rv, RV_DIAGONAL_DISTANCE);

        if (global_rv.distance_z != 0 || (rv.distance > 1 && rv.distance > global_rv.distance)) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "Path distance = %d for '%s' -> '%s'. Discarding old path.", rv.distance, op->name, op->enemy->name);
#endif
            FREE_AND_CLEAR_HASH(waypoint->msg);
        }
    }

    /* Are we far enough from the target to require a path? */
    if (global_rv.distance_z != 0 || global_rv.distance > 1) {
        if (!waypoint->msg) {
            /* Request a path if we don't have one */
            path_request(waypoint);
        } else {
            /* If we have precalculated path, take direction to next subwaypoint
             * */
            destx = waypoint->stats.hp;
            desty = waypoint->stats.sp;

            new_offset = waypoint->stats.food;

            if (new_offset < waypoint->attacked_by_distance && path_get_next(waypoint->msg, &new_offset, &waypoint->race, &destmap, &destx, &desty, &destflags)) {
                get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, destx, desty, &local_rv, RV_RECURSIVE_SEARCH | RV_DIAGONAL_DISTANCE);
                dest_rv = &local_rv;
            } else {
                /* We seem to have an invalid path string or offset. */
                FREE_AND_CLEAR_HASH(waypoint->msg);
                path_request(waypoint);
            }
        }
    }

    /* Did we get closer to our goal last time? */
    if ((int) dest_rv->distance < waypoint->stats.dam) {
        waypoint->stats.dam = dest_rv->distance;
        /* Number of times we failed getting closer to (sub)goal */
        waypoint->stats.Str = 0;
    } else if (dest_rv->distance_z == 0 && waypoint->stats.Str++ > 4) {
        /* Discard the current path, so that we can get a new one */
        FREE_AND_CLEAR_HASH(waypoint->msg);

        /* For best-effort waypoints don't try too many times. */
        if (QUERY_FLAG(waypoint, FLAG_NO_ATTACK) && waypoint->stats.Int++ > 10) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "Stuck with a best-effort waypoint (%s). Accepting current position", waypoint->name);
#endif
            /* A bit ugly, but will work for now (we want to trigger the
             * "reached goal" above) */
            waypoint->stats.hp = op->x;
            waypoint->stats.sp = op->y;
            return;
        }
    }

    if ((global_rv.distance_z != 0 || global_rv.distance > 1) &&
            !waypoint->msg && QUERY_FLAG(waypoint, FLAG_WP_PATH_REQUESTED) &&
            !QUERY_FLAG(waypoint, FLAG_DAMNED)) {
#ifdef DEBUG_PATHFINDING
        LOG(DEBUG, "No path found. '%s' standing still.", op->name);
#endif
        return;
    }

    /* Perform the actual move */
    dir = dest_rv->direction;

    if (QUERY_FLAG(op, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    if (dir && !QUERY_FLAG(op, FLAG_STAND_STILL)) {
        /* Can the monster move directly toward waypoint? */
        if (dest_rv->distance != 0 && !move_object(op, dir)) {
            int diff;

            /* Try move around corners otherwise */
            for (diff = 1; diff <= 2; diff++) {
                /* Try left or right first? */
                int m = 1 - (RANDOM() & 2);

                if (move_object(op, absdir(dir + diff * m)) || move_object(op, absdir(dir - diff * m))) {
                    break;
                }
            }
        }

        /* If we had a local destination and we got close enough to it, accept
         * it. */
        if (dest_rv == &local_rv && dest_rv->distance == 1) {
            if (destflags & PATH_NODE_EXIT && op->map == destmap &&
                    op->x == destx && op->y == desty) {
                object *tmp;

                for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL;
                        tmp = tmp->above) {
                    if (tmp->type == EXIT) {
                        object_apply(tmp, op, 0);
                        break;
                    }
                }
            }

            waypoint->stats.food = new_offset;
            /* Number of fails */
            waypoint->stats.Str = 0;
            /* Best distance */
            waypoint->stats.dam = 30000;
        }
    }
}

/**
 * Initialize the waypoint type object methods. */
void object_type_init_waypoint(void)
{
}

