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
 * Handles code for @ref CONE "cones".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <object_methods.h>

/**
 * Check whether a part of the cone already exists on the specified
 * position.
 *
 * @param op
 * The cone.
 * @param m
 * Map.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @return
 * Whether the cone exists.
 */
static bool
cone_exists (object *op, mapstruct *m, int x, int y)
{
    FOR_MAP_PREPARE(m, x, y, tmp) {
        if (op->type == tmp->type && op->weight_limit == tmp->weight_limit) {
            return true;
        }
    } FOR_MAP_FINISH();

    return false;
}

/** @copydoc object_methods_t::process_func */
static void process_func(object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->map == NULL) {
        return;
    }

    if (QUERY_FLAG(op, FLAG_LIFESAVE)) {
        attack_hit_map(op, 0, false);
        return;
    }

    attack_hit_map(op, 0, true);

    op->stats.hp -= 2;
    if (op->stats.hp < 0) {
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    if (op->stats.food != 0) {
        return;
    }

    op->stats.food = 1;

    for (int i = -1; i < 2; i++) {
        int x = op->x + freearr_x[absdir(op->stats.sp + i)];
        int y = op->y + freearr_y[absdir(op->stats.sp + i)];
        mapstruct *m = get_map_from_coord(op->map, &x, &y);
        if (m == NULL) {
            continue;
        }

        if (wall(m, x, y) || cone_exists(op, m, x, y)) {
            continue;
        }

        /* Create the next part of the cone. */
        object *tmp = arch_to_object(op->arch);
        copy_owner(tmp, op);
        tmp->weight_limit = op->weight_limit;
        tmp->x = x;
        tmp->y = y;
        tmp->level = op->level;
        tmp->stats.sp = op->stats.sp;
        tmp->stats.hp = op->stats.hp + 1;
        tmp->stats.maxhp = op->stats.maxhp;
        tmp->stats.dam = op->stats.dam;

        tmp = insert_ob_in_map(tmp, m, op, 0);
        if (tmp == NULL) {
            continue;
        }
    }
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (state == 0) {
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(op, FLAG_SLOW_MOVE)) {
        return OBJECT_METHOD_OK;
    }

    if (IS_LIVE(victim)) {
        attack_hit(victim, op, op->stats.dam);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the cone type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(cone)
{
    OBJECT_METHODS(CONE)->move_on_func = move_on_func;
    OBJECT_METHODS(CONE)->process_func = process_func;
}
