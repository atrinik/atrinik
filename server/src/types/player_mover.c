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
 * Handles code used for @ref PLAYER_MOVER "player movers".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Handle an object triggering a player mover.
 * @param op Player mover.
 * @param victim Victim.
 * @param process If true, this is called as part of periodic processing (from
 * process_func()).
 */
static void player_mover_handle(object *op, object *victim, bool process)
{
    if (!IS_LIVE(victim)) {
        return;
    }

    if (QUERY_FLAG(victim, FLAG_FLYING) && op->stats.maxhp == 0) {
        return;
    }

    if (QUERY_FLAG(op, FLAG_LIFESAVE) && op->stats.hp-- < 0) {
        destruct_ob(op);
        return;
    }

    int dir;

    /* No direction, this means 'xrays 1' was set; so use the
     * victim's direction instead. */
    if (op->direction == 0 && QUERY_FLAG(op, FLAG_XRAYS)) {
        dir = victim->direction;
    } else {
        dir = op->direction;

        if (dir == 0) {
            dir = get_random_dir();
        }
    }

    int x, y;
    mapstruct *map;

    x = op->x + freearr_x[dir];
    y = op->y + freearr_y[dir];
    map = get_map_from_coord(op->map, &x, &y);

    if (map == NULL) {
        return;
    }

    /* Flag to stop moving if there's a wall. */
    if (QUERY_FLAG(op, FLAG_STAND_STILL) &&
            blocked(victim, map, x, y, victim->terrain_flag)) {
        return;
    }

    if (!DBL_EQUAL(op->speed, 0.0)) {
        object *nextmover;

        FOR_MAP_LAYER_BEGIN(map, x, y, LAYER_SYS, -1, nextmover) {
            if (nextmover->type == op->type && nextmover->value != pticks) {
                nextmover->speed_left--;
            }
        } FOR_MAP_LAYER_END;
    }

    if (victim->type == PLAYER) {
        if (op->level == 0) {
            return;
        }
    } else if (op->stats.hp == 0) {
        return;
    }

    if (process || !QUERY_FLAG(op, FLAG_IS_MAGICAL)) {
        if (victim->type == PLAYER) {
            victim->speed_left = -FABS(victim->speed);
            /* Clear player's path; they probably can't move there
             * any more after being pushed, or might not want to. */
            player_path_clear(CONTR(victim));
        }

        OBJ_DESTROYED_BEGIN(victim) {
            move_object(victim, dir);

            if (OBJ_DESTROYED(victim)) {
                return;
            }
        } OBJ_DESTROYED_END();
    }

    if (op->stats.maxsp == 0 && op->stats.sp) {
        op->stats.maxsp = 2;
    }

    /* Flag to paralyze the player. */
    if (op->stats.sp != 0) {
        victim->speed_left = -(op->stats.maxsp * FABS(victim->speed));
    }
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    op->value = pticks;

    if (!(GET_MAP_FLAGS(op->map, op->x, op->y) & (P_IS_MONSTER |
            P_IS_PLAYER))) {
        return;
    }

    FOR_MAP_PREPARE(op->map, op->x, op->y, victim) {
        player_mover_handle(op, victim, true);
    } FOR_MAP_FINISH();
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator,
        int state)
{
    player_mover_handle(op, victim, false);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the player mover type object methods. */
void object_type_init_player_mover(void)
{
    object_type_methods[PLAYER_MOVER].process_func = process_func;
    object_type_methods[PLAYER_MOVER].move_on_func = move_on_func;
}
