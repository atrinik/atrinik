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
 * Handles the code for @ref WAYPOINT_OBJECT "waypoint objects".
 */

#include <global.h>
#include <plugin.h>
#include <monster_guard.h>
#include <object.h>
#include <object_methods.h>
#include <waypoint.h>

/**
 * Initialize the waypoint type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(waypoint)
{
}

/**
 * Find a monster's currently active waypoint, if any.
 *
 * @param npc
 * The monster.
 * @return
 * Active waypoint of this monster, NULL if none found.
 */
object *
waypoint_get_active (object *npc)
{
    HARD_ASSERT(npc != NULL);

    FOR_INV_PREPARE(npc, tmp) {
        if (tmp->type == WAYPOINT_OBJECT && QUERY_FLAG(tmp, FLAG_CURSED)) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Find a monster's current aggro waypoint, if any.
 *
 * @param npc
 * The monster.
 * @return
 * Aggro waypoint of this monster, NULL if none found.
 */
object *
waypoint_get_aggro (object *npc)
{
    HARD_ASSERT(npc != NULL);

    FOR_INV_PREPARE(npc, tmp) {
        if (tmp->type == WAYPOINT_OBJECT && QUERY_FLAG(tmp, FLAG_DAMNED)) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Find a monster's current home waypoint, if any.
 *
 * @param npc
 * The monster.
 * @return
 * Return-home waypoint of this monster, NULL if none
 * found.
 */
object *
waypoint_get_home (object *npc)
{
    HARD_ASSERT(npc != NULL);

    FOR_INV_PREPARE(npc, tmp) {
        if (tmp->type == WAYPOINT_OBJECT && QUERY_FLAG(tmp, FLAG_REFLECTING)) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Find a monster's waypoint by name (used for getting the next waypoint).
 *
 * @param npc
 * The monster.
 * @param name
 * The waypoint name to find.
 * @return
 * The waypoint object if found, NULL otherwise.
 */
static object *
waypoint_find (object *npc, shstr *name)
{
    HARD_ASSERT(npc != NULL);
    HARD_ASSERT(name != NULL);

    FOR_INV_PREPARE(npc, tmp) {
        if (tmp->type == WAYPOINT_OBJECT && tmp->name == name) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Find the destination map if specified in waypoint, otherwise use current
 * map.
 *
 * @param op
 * Waypoint.
 * @param npc
 * Monster.
 * @return
 * Destination map.
 */
static mapstruct *
waypoint_load_destination (object *op, object *npc)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(npc != NULL);

    if (!map_path_isabs(op->slaying)) {
        char *path = map_get_path(npc->map,
                                  op->slaying,
                                  MAP_UNIQUE(npc->map),
                                  NULL);
        FREE_AND_COPY_HASH(op->slaying, path);
        efree(path);
    }

    mapstruct *destmap;
    if (op->slaying == npc->map->path) {
        destmap = npc->map;
    } else {
        destmap = ready_map_name(op->slaying, NULL, MAP_NAME_SHARED);
    }
    return destmap;
}

/**
 * Perform a path computation for the waypoint object.
 *
 * This function is called whenever our path request is dequeued.
 *
 * @param waypoint
 * The waypoint object.
 */
void
waypoint_compute_path (object *op)
{
    HARD_ASSERT(op != NULL);

    if (unlikely(op->env == NULL)) {
        return;
    }

    /* Store final path destination (used by aggro wp) */
    if (QUERY_FLAG(op, FLAG_DAMNED)) {
        if (OBJECT_VALID(op->enemy, op->enemy_count)) {
            FREE_AND_COPY_HASH(op->slaying, op->enemy->map->path);
            op->x = op->stats.hp = op->enemy->x;
            op->y = op->stats.sp = op->enemy->y;
        } else {
            LOG(ERROR,
                "Dynamic waypoint without valid target: %s",
                object_get_str(op));
            return;
        }
    }


    mapstruct *destmap;
    if (op->slaying != NULL) {
        destmap = waypoint_load_destination(op, op->env);
    } else {
        destmap = op->env->map;
    }

    if (destmap == NULL) {
        LOG(ERROR,
            "Invalid destination map '%s': %s",
            op->slaying,
            object_get_str(op));
        return;
    }

    path_node_t *path = path_find(op->env,
                                  op->env->map,
                                  op->env->x,
                                  op->env->y,
                                  destmap,
                                  op->stats.hp,
                                  op->stats.sp,
                                  NULL);
    path = path_compress(path);
    if (path == NULL) {
        if (!QUERY_FLAG(op, FLAG_DAMNED)) {
            LOG(ERROR,
                "No path to destination ('%s', %s @ %d,%d -> "
                "'%s', %s @ %d,%d) for waypoint: %s",
                op->env->name,
                op->env->map->path,
                op->env->x,
                op->env->y,
                op->name,
                destmap->path,
                op->stats.hp,
                op->stats.sp,
                object_get_str(op));
        }

        return;
    }

    /* Skip the first path element (always the starting position), but only
     * if it's not a node with an exit. */
    if (!(path->flags & PATH_NODE_EXIT)) {
        path = path->next;
        if (path == NULL) {
            return;
        }
    }

    /* Textually encoded path */
    FREE_AND_CLEAR_HASH(op->msg);
    /* Map file for last local path step */
    FREE_AND_CLEAR_HASH(op->race);

    op->msg = path_encode(path);
    /* Path offset */
    op->stats.food = 0;
    /* Msg boundary */
    op->attacked_by_distance = strlen(op->msg);

    /* Number of fails */
    op->stats.Int = 0;
    /* Number of fails */
    op->stats.Str = 0;
    /* Best distance */
    op->stats.dam = 30000;
}

/**
 * Move towards waypoint target.
 *
 * @param op
 * The waypoint object.
 * @param npc
 * Object to move.
 */
void
waypoint_move (object *op, object *npc)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(npc != NULL);
    HARD_ASSERT(npc->map != NULL);

    mapstruct *destmap = npc->map;

    if (QUERY_FLAG(op, FLAG_DAMNED)) {
        if (op->enemy == npc->enemy &&
            op->enemy_count == npc->enemy_count &&
            OBJECT_VALID(op->enemy, op->enemy_count)) {
            destmap = op->enemy->map;
            op->stats.hp = op->enemy->x;
            op->stats.sp = op->enemy->y;
        } else {
            op->enemy = npc->enemy;
            op->enemy_count = npc->enemy_count;
            return;
        }
    } else if (op->slaying != NULL) {
        destmap = waypoint_load_destination(op, npc);
    }

    if (destmap == NULL) {
        LOG(ERROR,
            "Invalid destination map '%s' for NPC %s, waypoint %s",
            op->slaying,
            object_get_str(npc),
            object_get_str(op));
        return;
    }

    rv_vector global_rv;
    if (!get_rangevector_from_mapcoords(npc->map,
                                        npc->x,
                                        npc->y,
                                        destmap,
                                        op->stats.hp,
                                        op->stats.sp,
                                        &global_rv,
                                        RV_RECURSIVE_SEARCH |
                                        RV_DIAGONAL_DISTANCE)) {
        LOG(ERROR,
            "Could not find rv to: %s, %d, %d, waypoint: %s",
            destmap->path,
            op->stats.hp,
            op->stats.sp,
            object_get_str(op));
        /* Disable this waypoint */
        CLEAR_FLAG(op, FLAG_CURSED);
        return;
    }

    rv_vector *dest_rv = &global_rv;

    /* Reached the final destination? */
    if (global_rv.distance_z == 0 &&
        global_rv.distance <= (unsigned int) op->stats.maxsp) {
        /* Just arrived? */
        if (op->stats.ac == 0) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "'%s' reached destination '%s'", npc->name, op->name);
#endif

            /* Trigger the TRIGGER event */
            if (trigger_event(EVENT_TRIGGER,
                              npc,
                              op,
                              NULL,
                              NULL,
                              0,
                              0,
                              0,
                              0) != 0) {
                return;
            }
        }

        /* Waiting at this waypoint? */
        if (op->stats.ac < op->stats.wc) {
            op->stats.ac++;
            return;
        }

        /* Clear timer */
        op->stats.ac = 0;
        /* Set as inactive */
        CLEAR_FLAG(op, FLAG_CURSED);
        /* Remove precomputed path */
        FREE_AND_CLEAR_HASH(op->msg);
        /* Remove precomputed path data */
        FREE_AND_CLEAR_HASH(op->race);

        /* Is it a return-home waypoint? */
        if (QUERY_FLAG(op, FLAG_REFLECTING)) {
            npc->move_type = op->move_type;
            monster_guard_activate_gate(npc, 0);
        }

        /* Start over with the new waypoint, if any */
        if (!QUERY_FLAG(op, FLAG_DAMNED) && op->title != NULL) {
            object *wp = waypoint_find(npc, op->title);
            if (wp != NULL) {
#ifdef DEBUG_PATHFINDING
                LOG(DEBUG, "'%s' next waypoint: '%s'", npc->name, op->title);
#endif
                SET_FLAG(wp, FLAG_CURSED);
            }
#ifdef DEBUG_PATHFINDING
            else {
                LOG(DEBUG, "'%s' is missing next waypoint.", npc->name);
            }
#endif
        }

        op->enemy = NULL;
        return;
    }

    if (HAS_EVENT(op, EVENT_CLOSE) &&
        trigger_event(EVENT_CLOSE, npc, op, NULL, NULL, 0, 0, 0, 0) != 0) {
        return;
    }

    /* Handle precomputed paths */

    /* If we finished our current path, clear it so that we can get a
     * new one. */
    if (op->msg != NULL && (op->msg[op->stats.food] == '\0' ||
                            (global_rv.distance_z == 0 &&
                             global_rv.distance <= 0))) {
        FREE_AND_CLEAR_HASH(op->msg);
    }

    /* Get new path if target has moved much since the path was created. */
    if (QUERY_FLAG(op, FLAG_DAMNED) &&
        op->msg != NULL &&
        (op->stats.hp != op->x || op->stats.sp != op->y)) {
        rv_vector rv;
        ;

        if (global_rv.distance_z != 0 ||
            !get_rangevector_from_mapcoords(destmap,
                                            op->stats.hp,
                                            op->stats.sp,
                                            destmap,
                                            op->x,
                                            op->y,
                                            &rv,
                                            RV_DIAGONAL_DISTANCE) ||
            (rv.distance > 1 && rv.distance > global_rv.distance)) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG,
                "Path distance = %d for '%s' -> '%s'. Discarding old path.",
                rv.distance,
                npc->name,
                npc->enemy->name);
#endif
            FREE_AND_CLEAR_HASH(op->msg);
        }
    }

    int destx;
    int desty;
    int16_t new_offset;
    uint32_t destflags;
    rv_vector local_rv;

    /* Are we far enough from the target to require a path? */
    if (global_rv.distance_z != 0 || global_rv.distance > 1) {
        if (op->msg == NULL) {
            /* Request a path if we don't have one */
            path_request(op);
        } else {
            /* If we have precalculated path, take direction to next
             * subwaypoint. */
            destx = op->stats.hp;
            desty = op->stats.sp;

            new_offset = op->stats.food;

            if (new_offset < op->attacked_by_distance &&
                path_get_next(op->msg,
                              &new_offset,
                              &op->race,
                              &destmap,
                              &destx,
                              &desty,
                              &destflags)) {
                get_rangevector_from_mapcoords(npc->map,
                                               npc->x,
                                               npc->y,
                                               destmap,
                                               destx,
                                               desty,
                                               &local_rv,
                                               RV_RECURSIVE_SEARCH |
                                               RV_DIAGONAL_DISTANCE);
                dest_rv = &local_rv;
            } else {
                /* We seem to have an invalid path string or offset. */
                FREE_AND_CLEAR_HASH(op->msg);
                path_request(op);
            }
        }
    }

    /* Did we get closer to our goal last time? */
    if (dest_rv->distance < (unsigned int) op->stats.dam) {
        op->stats.dam = dest_rv->distance;
        /* Number of times we failed getting closer to (sub)goal */
        op->stats.Str = 0;
    } else if (dest_rv->distance_z == 0 && op->stats.Str++ > 4) {
        /* Discard the current path, so that we can get a new one */
        FREE_AND_CLEAR_HASH(op->msg);

        /* For best-effort waypoints don't try too many times. */
        if (QUERY_FLAG(op, FLAG_NO_ATTACK) && op->stats.Int++ > 10) {
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG,
                "Stuck with a best-effort waypoint (%s). Accepting "
                "current position", op->name);
#endif
            op->stats.hp = npc->x;
            op->stats.sp = npc->y;
            return;
        }
    }

    if ((global_rv.distance_z != 0 || global_rv.distance > 1) &&
        op->msg == NULL &&
        QUERY_FLAG(op, FLAG_WP_PATH_REQUESTED) &&
        !QUERY_FLAG(op, FLAG_DAMNED)) {
#ifdef DEBUG_PATHFINDING
        LOG(DEBUG, "No path found. '%s' standing still.", npc->name);
#endif
        return;
    }

    /* Perform the actual move */
    int dir = dest_rv->direction;
    if (QUERY_FLAG(npc, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    if (dir != 0 && !QUERY_FLAG(npc, FLAG_STAND_STILL)) {
        int ret = 0;

        OBJECTS_DESTROYED_BEGIN(npc) {
            /* Can the monster move directly toward waypoint? */
            if (dest_rv->distance != 0 && (ret = move_object(npc, dir)) == 0) {
                /* Try to move around corners otherwise */
                for (int diff = 1; diff <= 2; diff++) {
                    /* Try left or right first? */
                    int m = 1 - rndm_chance(2) ? 2 : 0;

                    if ((ret = move_object(npc, absdir(dir + diff * m))) != 0) {
                        break;
                    }

                    if ((ret = move_object(npc, absdir(dir - diff * m))) != 0) {
                        break;
                    }
                }
            }

            if (OBJECTS_DESTROYED(npc)) {
                return;
            }
        } OBJECTS_DESTROYED_END();

        /* If we had a local destination and we got close enough to it,
         * accept it. */
        if (dest_rv == &local_rv && dest_rv->distance == 1 && ret != -1) {
            /* Handle entering exits that need to be manually
             * applied (stairs, ladders, etc). */
            if (destflags & PATH_NODE_EXIT &&
                npc->map == destmap &&
                npc->x == destx &&
                npc->y == desty) {
                FOR_MAP_PREPARE(npc->map, npc->x, npc->y, tmp) {
                    if (tmp->type == EXIT) {
                        object_apply(tmp, npc, 0);
                        break;
                    }
                } FOR_MAP_FINISH();
            }

            op->stats.food = new_offset;
            /* Number of fails */
            op->stats.Str = 0;
            /* Best distance */
            op->stats.dam = 30000;
        }
    }
}
