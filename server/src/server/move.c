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
 * Handles object moving and pushing. */

#include <global.h>

/**
 * Returns a random direction (1..8).
 * @return The random direction. */
int get_random_dir(void)
{
    return rndm(1, 8);
}

/**
 * Returns a random direction (1..8) similar to a given direction.
 * @param dir The exact direction.
 * @return The randomized direction. */
int get_randomized_dir(int dir)
{
    return absdir(dir + rndm(0, 2) + rndm(0, 2) - 2);
}

/**
 * Move the object to the specified coordinates.
 *
 * Will update the object's sub-layer if necessary.
 * @param op Object.
 * @param dir Direction the object is moving into.
 * @param originator What caused the object to move.
 * @param m Map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return 1 on success, 0 on failure.
 */
int object_move_to(object *op, int dir, object *originator, mapstruct *m,
        int x, int y)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(originator != NULL);
    HARD_ASSERT(m != NULL);

    SOFT_ASSERT_RC(dir > 0 && dir <= NUM_DIRECTION, 0, "Invalid direction: %d",
            dir);
    SOFT_ASSERT_RC(x >= 0 && x < m->width, 0, "Invalid X coordinate: %d", x);
    SOFT_ASSERT_RC(y >= 0 && y < m->height, 0, "Invalid Y coordinate: %d", y);

    object *floor =
            GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, op->sub_layer);
    int z = floor != NULL ? floor->z : 0;
    int z_highest = 0, sub_layer = 0;
    object *floor_tmp;

    FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_FLOOR, -1, floor_tmp) {
        if (floor_tmp->z - z > MOVE_MAX_HEIGHT_DIFF) {
            continue;
        }

        if (floor_tmp->z > z_highest) {
            z_highest = floor_tmp->z;
            sub_layer = floor_tmp->sub_layer;
        }
    } FOR_MAP_LAYER_END

    object_remove(op, 0);

    op->sub_layer = sub_layer;
    op->x = x;
    op->y = y;
    op = insert_ob_in_map(op, m, originator, INS_FALL_THROUGH);

    if (op == NULL) {
        return 1;
    }

    if (op->map == m && op->x == x && op->y == y) {
        floor = GET_MAP_OB_LAYER(m, x, y, LAYER_FLOOR, sub_layer);
        int fall_floors = (int) ((z - (floor != NULL ? floor->z : 0)) / 50.0 +
                0.5);

        if (fall_floors > 0 && IS_LIVE(op)) {
            OBJ_DESTROYED_BEGIN(op) {
                fall_damage_living(op, fall_floors);

                if (OBJ_DESTROYED(op)) {
                    return 1;
                }
            } OBJ_DESTROYED_END();
        }
    }

    if (op->type == PLAYER) {
        CONTR(op)->stat_steps_taken++;
    }

    return 1;
}

/**
 * Try to move object in specified direction.
 * @param op What to move.
 * @param dir Direction to move the object to.
 * @param originator Typically the same as op, but can be different if
 * originator is causing op to move (originator is pushing op).
 * @return 0 if the object is not able to move to the desired space, -1 if the
 * object was not able to move there yet but some sort of action was performed
 * that might allow us to move there (door opening for example), direction
 * number that the object ended up moving in otherwise.
 */
int move_ob(object *op, int dir, object *originator)
{
    mapstruct *m;
    int xt, yt, flags;

    if (op == NULL) {
        return 0;
    }

    if (QUERY_FLAG(op, FLAG_REMOVED)) {
        LOG(BUG, "monster %s has been removed - will not process further", query_name(op, NULL));
        return 0;
    }

    op = HEAD(op);

    if (QUERY_FLAG(op, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    op->anim_flags |= ANIM_FLAG_MOVING;
    op->anim_flags &= ~ANIM_FLAG_STOP_MOVING;
    op->direction = dir;

    xt = op->x + freearr_x[dir];
    yt = op->y + freearr_y[dir];

    /* we have here a get_map_from_coord - we can skip all */
    if (!(m = get_map_from_coord(op->map, &xt, &yt))) {
        return 0;
    }

    if (op->type != PLAYER || !CONTR(op)->tcl) {
        flags = object_blocked(op, m, xt, yt);

        if (flags != 0) {
            if (flags & P_DOOR_CLOSED) {
                door_show_message(op, m, xt, yt);
            }

            return 0;
        }
    }

    if (door_try_open(op, m, xt, yt, 0)) {
        return -1;
    }

    if (!object_move_to(op, dir, originator, m, xt, yt)) {
        return 0;
    }

    return dir;
}

/**
 * Move an object (even linked objects) to another spot on the same map.
 *
 * Does nothing if there is no free spot.
 * @param op What to move.
 * @param x New X coordinate.
 * @param y New Y coordinate.
 * @param randomly If 1, use find_free_spot() to find the destination,
 * otherwise use find_first_free_spot().
 * @param originator What is causing op to move.
 * @param trap Trap.
 * @return 1 if the object was destroyed, 0 otherwise. */
int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap)
{
    int i, ret;

    if (trap != NULL && EXIT_PATH(trap)) {
        if (op->type == PLAYER && trap->msg && strncmp(EXIT_PATH(trap), "/!", 2) && strncmp(EXIT_PATH(trap), "/random/", 8)) {
            draw_info(COLOR_NAVY, op, trap->msg);
        }

        object_enter_map(op, trap, NULL, 0, 0, 0);
        return 1;
    } else if (randomly) {
        i = find_free_spot(op->arch, NULL, op->map, x, y, 0, SIZEOFFREE3);
    } else {
        i = find_first_free_spot(op->arch, op, op->map, x, y);
    }

    /* No free spot */
    if (i == -1) {
        return 0;
    }

    if (op->head != NULL) {
        op = op->head;
    }

    object_remove(op, 0);

    op->x = x + freearr_x[i];
    op->y = y + freearr_y[i];

    ret = (insert_ob_in_map(op, op->map, originator, 0) == NULL);

    return ret;
}

/**
 * An object is being pushed.
 * @param op What is being pushed.
 * @param dir Pushing direction.
 * @param pusher What is pushing op.
 * @return 0 if the object couldn't be pushed, 1 otherwise. */
int push_ob(object *op, int dir, object *pusher)
{
    object *tmp, *floor_ob;
    mapstruct *m;
    int x, y;

    /* Don't allow pushing multi-arch objects. */
    if (op->head) {
        return 0;
    }

    /* Check whether we are strong enough to push this object. */
    if (op->weight && (op->weight / 50000 - 1 > 0 ? rndm(0, op->weight / 50000 - 1) : 0) > pusher->stats.Str) {
        return 0;
    }

    x = op->x + freearr_x[dir];
    y = op->y + freearr_y[dir];

    if (!(m = get_map_from_coord(op->map, &x, &y))) {
        return 0;
    }

    floor_ob = GET_MAP_OB_LAYER(m, x, y, LAYER_FLOOR, 0);

    /* Floor has no-push flag set? */
    if (floor_ob && QUERY_FLAG(floor_ob, FLAG_XRAYS)) {
        return 0;
    }

    if (blocked(op, m, x, y, op->terrain_flag)) {
        return 0;
    }

    /* Try to find something that would block the push. */
    for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above) {
        if (tmp->head || IS_LIVE(tmp) || tmp->type == EXIT) {
            return 0;
        }
    }

    object_remove(op, 0);

    op->x = op->x + freearr_x[dir];
    op->y = op->y + freearr_y[dir];
    insert_ob_in_map(op, op->map, pusher, 0);
    return 1;
}
