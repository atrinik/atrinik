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
 * Handles code for @ref PEDESTAL "pedestals".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <object_methods.h>
#include <pedestal.h>

/** @copydoc object_methods_t::trigger_button_func */
static int
trigger_button_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    op->value = 0;

    FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
        tmp = HEAD(tmp);
        if (tmp == op) {
            continue;
        }

        if (QUERY_FLAG(tmp, FLAG_FLYING)) {
            if (!QUERY_FLAG(op, FLAG_FLY_ON)) {
                continue;
            }
        } else if (!QUERY_FLAG(op, FLAG_WALK_ON)) {
            continue;
        }

        if (!pedestal_matches_obj(op, tmp)) {
            continue;
        }

        op->value = 1;
        break;
    } FOR_MAP_FINISH();

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the pedestal type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(pedestal)
{
    OBJECT_METHODS(PEDESTAL)->trigger_button_func = trigger_button_func;
    OBJECT_METHODS(PEDESTAL)->fallback = object_methods_get(BUTTON);
}

/**
 * Check whether pedestal matches the specified object.
 *
 * @param op
 * Pedestal.
 * @param tmp
 * Object to check.
 * @return
 * true if the object matches, false otherwise.
 */
bool
pedestal_matches_obj (object *op, object *tmp)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(tmp != NULL);

    /* Check type. */
    if (op->stats.hp != 0 && tmp->type != op->stats.hp) {
        return false;
    }

    /* Check name. */
    if (op->slaying != NULL && tmp->name != op->slaying) {
        return false;
    }

    /* Check archname. */
    if (op->race != NULL && tmp->arch->name != op->race) {
        return false;
    }

    return true;
}
