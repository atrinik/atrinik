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
 * Handles code related to @ref GATE "gates".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->stats.maxhp != 0 && op->stats.hp != 0) {
        if (--op->stats.hp == 0) {
            op->value = !op->value;
            op->stats.food = 0;
        } else {
            return;
        }
    }

    /* Going down. */
    if (op->value != 0) {
        if (--op->stats.wc <= 0) {
            op->stats.wc = 0;

            if (op->stats.food != 0) {
                op->stats.hp = op->stats.maxhp;
            } else {
                op->speed = 0;
                update_ob_speed(op);
            }
        }

        if (op->stats.wc < (NUM_ANIMATIONS(op) / NUM_FACINGS(op) / 2 + 1)) {
            if (op->stats.ac != 0) {
                CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
            }

            CLEAR_FLAG(op, FLAG_NO_PASS);
            update_object(op, UP_OBJ_FLAGS);
        }
    } else {
        /* Going up. */

        if (++op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) / 2) {
            bool is_blocked = false;
            FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
                if (!QUERY_FLAG(tmp, FLAG_CAN_ROLL) && !IS_LIVE(tmp)) {
                    continue;
                }

                if (IS_LIVE(tmp)) {
                    attack_hit(tmp, op, op->stats.dam);
                    draw_info_format(COLOR_WHITE, tmp,
                                     "You are crushed by the %s!",
                                     op->name);
                }

                int i = find_free_spot(tmp->arch,
                                       tmp,
                                       op->map,
                                       op->x,
                                       op->y,
                                       1,
                                       SIZEOFFREE1);
                /* If there is a free spot, move the object someplace. */
                if (i != -1) {
                    object_remove(tmp, 0);
                    tmp->x += freearr_x[i];
                    tmp->y += freearr_y[i];
                    insert_ob_in_map(tmp, op->map, op, 0);
                } else {
                    /* No free spot, so the gate is blocked. */
                    is_blocked = true;
                }
            } FOR_MAP_FINISH();

            if (is_blocked) {
                op->stats.wc--;
            } else if (!QUERY_FLAG(op, FLAG_NO_PASS)) {
                if (op->stats.ac != 0) {
                    SET_FLAG(op, FLAG_BLOCKSVIEW);
                }

                SET_FLAG(op, FLAG_NO_PASS);
                update_object(op, UP_OBJ_FLAGS);
            }

            if (op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1) {
                op->stats.wc = (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1;

                if (op->stats.food != 0) {
                    op->stats.hp = op->stats.maxhp;
                } else {
                    op->speed = 0;
                    update_ob_speed(op);
                }
            }
        }
    }

    op->state = op->stats.wc;
    SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) *
                       op->direction + op->stats.wc));
    update_object(op, UP_OBJ_FACE);
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    if (!DBL_EQUAL(op->speed, 0.0) && op->stats.maxhp) {
        return OBJECT_METHOD_OK;
    }

    op->speed = 0.5;
    update_ob_speed(op);

    if (op->stats.maxhp != 0) {
        op->stats.food = 1;
        op->value = !op->value;
    } else {
        op->value = op->stats.maxsp != 0 ? !state : state;
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the gate type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(gate)
{
    OBJECT_METHODS(GATE)->trigger_func = trigger_func;
    OBJECT_METHODS(GATE)->process_func = process_func;
}
