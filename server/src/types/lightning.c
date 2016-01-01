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
 * Handles code related to @ref LIGHTNING "lightning".
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/**
 * Causes lightning to fork.
 *
 * @param op
 * Original bolt.
 * @param tmp
 * First piece of the fork.
 */
static void
lightning_fork (object *op, object *tmp)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(tmp != NULL);

    if (tmp->stats.Dex == 0 || rndm(1, 100) > tmp->stats.Dex) {
        return;
    }

    int dir_adjust;
    if (rndm(0, 99) < tmp->stats.Con) {
        /* Fork left. */
        dir_adjust = -1;
    } else {
        /* Fork right. */
        dir_adjust = 1;
    }

    int dir = absdir(tmp->direction + dir_adjust);

    int x = tmp->x + freearr_x[dir];
    int y = tmp->y + freearr_y[dir];
    mapstruct *m = get_map_from_coord(tmp->map, &x, &y);
    if (m == NULL || wall(m, x, y)) {
        return;
    }

    object *bolt = get_object();
    copy_object(tmp, bolt, 0);

    bolt->stats.food = 0;
    /* Reduce chances of subsequent forking. */
    bolt->stats.Dex -= 10;
    /* Less forks from main bolt too. */
    tmp->stats.Dex -= 10;
    /* Adjust the left bias. */
    bolt->stats.Con += 25 * dir;
    bolt->speed_left = -0.1f;
    bolt->direction = dir;
    SET_ANIMATION_STATE(bolt);
    bolt->stats.hp++;
    bolt->x = x;
    bolt->y = y;
    /* Reduce daughter bolt damage. */
    bolt->stats.dam /= 2;
    bolt->stats.dam++;
    /* Reduce father bolt damage. */
    tmp->stats.dam /= 2;
    tmp->stats.dam++;

    bolt = insert_ob_in_map(bolt, m, op, 0);
    SOFT_ASSERT(bolt != NULL,
                "Failed to insert bolt from %s",
                object_get_str(op));
}

/** @copydoc object_methods_t::projectile_move_func */
static object *
projectile_move_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->stats.food == 0) {
        op = common_object_projectile_move(op);
        if (op == NULL) {
            return NULL;
        }

        op->stats.food = 1;
    } else if (op->stats.food == 1) {
        object *tmp = get_object();
        copy_object(op, tmp, 0);
        tmp->speed_left = -0.1f;
        tmp->x = op->x;
        tmp->y = op->y;
        tmp = insert_ob_in_map(tmp, op->map, op, 0);
        if (tmp == NULL) {
            return NULL;
        }

        tmp->stats.food = 0;
        tmp->last_sp++;
        object_process(tmp);
        lightning_fork(op, tmp);
        op->stats.food = 2;
    }

    return op;
}

/** @copydoc object_methods_t::projectile_stop_func */
static object *
projectile_stop_func (object *op, int reason)
{
    HARD_ASSERT(op != NULL);

    if (reason == OBJECT_PROJECTILE_STOP_EOL) {
        return common_object_projectile_stop_spell(op, reason);
    }

    return op;
}

/**
 * Initialize the lightning type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(lightning)
{
    OBJECT_METHODS(LIGHTNING)->projectile_move_func =
        projectile_move_func;
    OBJECT_METHODS(LIGHTNING)->projectile_stop_func =
        projectile_stop_func;
    OBJECT_METHODS(LIGHTNING)->process_func =
        common_object_projectile_process;
    OBJECT_METHODS(LIGHTNING)->projectile_hit_func =
        common_object_projectile_hit;
    OBJECT_METHODS(LIGHTNING)->move_on_func =
        common_object_projectile_move_on;
}
