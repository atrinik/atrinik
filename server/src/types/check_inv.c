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
 * Handles code for @ref CHECK_INV "inventory checker" objects.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Inventory checker object tries to find a matching object in creature's
 * inventory.
 * @param op What is doing the searching.
 * @param ob Object of which to search the inventory.
 * @return Object that matches, NULL if none matched. */
object *check_inv(object *op, object *ob)
{
    object *tmp, *ret;

    for (tmp = ob->inv; tmp; tmp = tmp->below) {
        if (tmp->inv && !IS_SYS_INVISIBLE(tmp)) {
            ret = check_inv(op, tmp);

            if (ret) {
                return ret;
            }
        } else {
            if (op->stats.hp && tmp->type != op->stats.hp) {
                continue;
            }

            if (op->slaying && (op->stats.sp ? tmp->slaying : tmp->name) != op->slaying) {
                continue;
            }

            if (op->race && tmp->arch->name != op->race) {
                continue;
            }

            return tmp;
        }
    }

    return NULL;
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
    object *match;

    (void) originator;

    if (victim->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    match = check_inv(op, victim);

    if (match && op->last_sp) {
        if (op->last_heal) {
            decrease_ob(match);
        }

        connection_trigger(op, state);
    } else if (!match && !op->last_sp) {
        connection_trigger(op, state);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the inventory checker type object methods. */
void object_type_init_check_inv(void)
{
    object_type_methods[CHECK_INV].move_on_func = move_on_func;
}
