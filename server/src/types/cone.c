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
 * @author Alex Tokar */

#include <global.h>

/**
 * Check whether a part of the cone already exists on the specified
 * position.
 * @param op The cone.
 * @param m Map.
 * @param x X position.
 * @param y Y position. */
static int cone_exists(object *op, mapstruct *m, int x, int y)
{
    object *tmp;

    for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above) {
        if (op->type == tmp->type && op->weight_limit == tmp->weight_limit) {
            return 1;
        }
    }

    return 0;
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    int i, x, y;
    mapstruct *m;
    object *tmp;

    if (!op->map) {
        return;
    }

    if (QUERY_FLAG(op, FLAG_LIFESAVE)) {
        hit_map(op, 0, 0);
        return;
    }

    hit_map(op, 0, 1);

    if ((op->stats.hp -= 2) < 0) {
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    if (op->stats.food) {
        return;
    }

    op->stats.food = 1;

    for (i = -1; i < 2; i++) {
        x = op->x + freearr_x[absdir(op->stats.sp + i)];
        y = op->y + freearr_y[absdir(op->stats.sp + i)];
        m = get_map_from_coord(op->map, &x, &y);

        if (!m) {
            continue;
        }

        if (wall(m, x, y) || cone_exists(op, m, x, y)) {
            continue;
        }

        /* Create the next part of the cone. */
        tmp = arch_to_object(op->arch);
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

        if (!tmp) {
            continue;
        }
    }
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
    (void) originator;

    if (!state) {
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(op, FLAG_SLOW_MOVE)) {
        return OBJECT_METHOD_OK;
    }

    if (IS_LIVE(victim)) {
        hit_player(victim, op->stats.dam, op, AT_INTERNAL);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the cone type object methods. */
void object_type_init_cone(void)
{
    object_type_methods[CONE].move_on_func = move_on_func;
    object_type_methods[CONE].process_func = process_func;
}
