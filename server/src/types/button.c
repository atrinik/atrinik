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
 * Handles code for @ref BUTTON "button".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (!DBL_EQUAL(op->speed, 0.0) ||
        (op->stats.exp == -1 && op->value != 0)) {
        return OBJECT_METHOD_OK;
    }

    connection_trigger_button(op, state);
    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    op->value = state;

    if (state != 0 && cause->stats.exp != 0) {
        op->speed = 1.0 / cause->stats.exp;
        update_ob_speed(op);
        op->speed_left = -1;
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::trigger_button_func */
static int
trigger_button_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    uint32_t total = 0;
    FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
        tmp = HEAD(tmp);
        if (tmp == op) {
            continue;
        }

        if (QUERY_FLAG(tmp, FLAG_FLYING)) {
            if (!QUERY_FLAG(op, FLAG_FLY_ON)) {
                continue;
            }
        } else {
            if (!QUERY_FLAG(op, FLAG_WALK_ON)) {
                continue;
            }
        }

        total += tmp->weight * MAX(1, tmp->nrof) + tmp->carrying;
    } FOR_MAP_FINISH();

    op->value = total >= op->weight;

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    op->speed = 0;
    update_ob_speed(op);

    if (op->stats.exp == -1) {
        return;
    }

    op->value = 0;
    connection_trigger(op, op->value);
}

/**
 * Initialize the button type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(button)
{
    OBJECT_METHODS(BUTTON)->move_on_func = move_on_func;
    OBJECT_METHODS(BUTTON)->trigger_func = trigger_func;
    OBJECT_METHODS(BUTTON)->trigger_button_func = trigger_button_func;
    OBJECT_METHODS(BUTTON)->process_func = process_func;
}
